/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_
#define TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_

#include <map>
#include <string>
#include <vector>

#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sigslot.h"

namespace rtc {
class Thread;
}  // namespace rtc

namespace webrtc {

class RemoteMediaStreamFactory;

// A MediaStreamSignalingObserver is notified when events happen to
// MediaStreams, MediaStreamTracks or DataChannels associated with the observed
// MediaStreamSignaling object. The notifications identify the stream, track or
// channel.
class MediaStreamSignalingObserver {
 public:
  // Triggered when the remote SessionDescription has a new data channel.
  virtual void OnAddDataChannel(DataChannelInterface* data_channel) = 0;

 protected:
  ~MediaStreamSignalingObserver() {}
};

// MediaStreamSignaling works as a glue between MediaStreams and a cricket
// classes for SessionDescriptions.
// It is used for creating cricket::MediaSessionOptions given the local
// MediaStreams and data channels.
//
// It is responsible for creating remote MediaStreams given a remote
// SessionDescription and creating cricket::MediaSessionOptions given
// local MediaStreams.
//
// To signal that a DataChannel should be established:
// 1. Call AddDataChannel with the new DataChannel. Next time
//    GetMediaSessionOptions will include the description of the DataChannel.
// 2. When a local session description is set, call UpdateLocalStreams with the
//    session description. This will set the SSRC used for sending data on
//    this DataChannel.
// 3. When remote session description is set, call UpdateRemoteStream with the
//    session description. If the DataChannel label and a SSRC is included in
//    the description, the DataChannel is updated with SSRC that will be used
//    for receiving data.
// 4. When both the local and remote SSRC of a DataChannel is set the state of
//    the DataChannel change to kOpen.
//
// To setup a DataChannel initialized by the remote end.
// 1. When remote session description is set, call UpdateRemoteStream with the
//    session description. If a label and a SSRC of a new DataChannel is found
//    MediaStreamSignalingObserver::OnAddDataChannel with the label and SSRC is
//    triggered.
// 2. Create a DataChannel instance with the label and set the remote SSRC.
// 3. Call AddDataChannel with this new DataChannel.  GetMediaSessionOptions
//    will include the description of the DataChannel.
// 4. Create a local session description and call UpdateLocalStreams. This will
//    set the local SSRC used by the DataChannel.
// 5. When both the local and remote SSRC of a DataChannel is set the state of
//    the DataChannel change to kOpen.
//
// To close a DataChannel:
// 1. Call DataChannel::Close. This will change the state of the DataChannel to
//    kClosing. GetMediaSessionOptions will not
//    include the description of the DataChannel.
// 2. When a local session description is set, call UpdateLocalStreams with the
//    session description. The description will no longer contain the
//    DataChannel label or SSRC.
// 3. When remote session description is set, call UpdateRemoteStream with the
//    session description. The description will no longer contain the
//    DataChannel label or SSRC. The DataChannel SSRC is updated with SSRC=0.
//    The DataChannel change state to kClosed.

class MediaStreamSignaling : public sigslot::has_slots<> {
 public:
  typedef std::map<std::string, rtc::scoped_refptr<DataChannel> >
      RtpDataChannels;

  MediaStreamSignaling(rtc::Thread* signaling_thread,
                       MediaStreamSignalingObserver* stream_observer,
                       cricket::ChannelManager* channel_manager);
  virtual ~MediaStreamSignaling();

  // Notify all referenced objects that MediaStreamSignaling will be teared
  // down. This method must be called prior to the dtor.
  void TearDown();

  // Set a factory for creating data channels that are initiated by the remote
  // peer.
  void SetDataChannelFactory(DataChannelFactory* data_channel_factory) {
    data_channel_factory_ = data_channel_factory;
  }

  // Checks if |id| is available to be assigned to a new SCTP data channel.
  bool IsSctpSidAvailable(int sid) const;

  // Gets the first available SCTP id that is not assigned to any existing
  // data channels.
  bool AllocateSctpSid(rtc::SSLRole role, int* sid);

  // Checks if any data channel has been added.
  bool HasDataChannels() const;
  // Adds |data_channel| to the collection of DataChannels that will be
  // be offered in a SessionDescription.
  bool AddDataChannel(DataChannel* data_channel);
  // After we receive an OPEN message, create a data channel and add it.
  bool AddDataChannelFromOpenMessage(const cricket::ReceiveDataParams& params,
                                     const rtc::Buffer& payload);
  void RemoveSctpDataChannel(int sid);

  // Returns a MediaSessionOptions struct with options decided by |options|,
  // the local MediaStreams and DataChannels.
  virtual bool GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  virtual bool GetOptionsForAnswer(
      const MediaConstraintsInterface* constraints,
      cricket::MediaSessionOptions* options);

  // Called when the remote session description has changed. The purpose is to
  // update remote MediaStreams and DataChannels with the current
  // session state.
  // If the remote SessionDescription contain information about a new remote
  // MediaStreams a new remote MediaStream is created and
  // MediaStreamSignalingObserver::OnAddStream is called.
  // If a remote MediaStream is missing from
  // the remote SessionDescription MediaStreamSignalingObserver::OnRemoveStream
  // is called.
  // If the SessionDescription contains information about a new DataChannel,
  // MediaStreamSignalingObserver::OnAddDataChannel is called with the
  // DataChannel.
  void OnRemoteDescriptionChanged(const SessionDescriptionInterface* desc);

  // Called when the local session description has changed. The purpose is to
  // update local and remote MediaStreams and DataChannels with the current
  // session state.
  // If |desc| indicates that the media type should be rejected, the method
  // ends the remote MediaStreamTracks.
  // It also updates local DataChannels with information about its local SSRC.
  void OnLocalDescriptionChanged(const SessionDescriptionInterface* desc);

  // Called when the data channel closes.
  void OnDataChannelClose();

  void OnDataTransportCreatedForSctp();
  void OnDtlsRoleReadyForSctp(rtc::SSLRole role);
  void OnRemoteSctpDataChannelClosed(uint32 sid);

 private:
  struct RemotePeerInfo {
    RemotePeerInfo()
        : msid_supported(false),
          default_audio_track_needed(false),
          default_video_track_needed(false) {
    }
    // True if it has been discovered that the remote peer support MSID.
    bool msid_supported;
    // The remote peer indicates in the session description that audio will be
    // sent but no MSID is given.
    bool default_audio_track_needed;
    // The remote peer indicates in the session description that video will be
    // sent but no MSID is given.
    bool default_video_track_needed;

    bool IsDefaultMediaStreamNeeded() {
      return !msid_supported && (default_audio_track_needed ||
          default_video_track_needed);
    }
  };

  struct TrackInfo {
    TrackInfo() : ssrc(0) {}
    TrackInfo(const std::string& stream_label,
              const std::string track_id,
              uint32 ssrc)
        : stream_label(stream_label),
          track_id(track_id),
          ssrc(ssrc) {
    }
    std::string stream_label;
    std::string track_id;
    uint32 ssrc;
  };
  typedef std::vector<TrackInfo> TrackInfos;

  // Loops through the vector of |streams| and finds added and removed
  // StreamParams since last time this method was called.
  // For each new or removed StreamParam NotifyLocalTrackAdded or
  // NotifyLocalTrackRemoved in invoked.
  void UpdateLocalTracks(const std::vector<cricket::StreamParams>& streams,
                         cricket::MediaType media_type);

  // Triggered when a local track has been seen for the first time in a local
  // session description.
  // This method triggers MediaStreamSignaling::OnAddLocalAudioTrack or
  // MediaStreamSignaling::OnAddLocalVideoTrack if the rtp streams in the local
  // SessionDescription can be mapped to a MediaStreamTrack in a MediaStream in
  // |local_streams_|
  void OnLocalTrackSeen(const std::string& stream_label,
                        const std::string& track_id,
                        uint32 ssrc,
                        cricket::MediaType media_type);

  // Triggered when a local track has been removed from a local session
  // description.
  // This method triggers MediaStreamSignaling::OnRemoveLocalAudioTrack or
  // MediaStreamSignaling::OnRemoveLocalVideoTrack if a stream has been removed
  // from the local SessionDescription and the stream can be mapped to a
  // MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalTrackRemoved(const std::string& stream_label,
                           const std::string& track_id,
                           uint32 ssrc,
                           cricket::MediaType media_type);

  void UpdateLocalRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateRemoteRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateClosingDataChannels(
      const std::vector<std::string>& active_channels, bool is_local_update);
  void CreateRemoteDataChannel(const std::string& label, uint32 remote_ssrc);

  const TrackInfo* FindTrackInfo(const TrackInfos& infos,
                                 const std::string& stream_label,
                                 const std::string track_id) const;

  // Returns the index of the specified SCTP DataChannel in sctp_data_channels_,
  // or -1 if not found.
  int FindDataChannelBySid(int sid) const;

  RemotePeerInfo remote_info_;
  rtc::Thread* signaling_thread_;
  DataChannelFactory* data_channel_factory_;
  MediaStreamSignalingObserver* stream_observer_;
  rtc::scoped_ptr<RemoteMediaStreamFactory> remote_stream_factory_;

  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  int last_allocated_sctp_even_sid_;
  int last_allocated_sctp_odd_sid_;

  typedef std::vector<rtc::scoped_refptr<DataChannel> > SctpDataChannels;

  RtpDataChannels rtp_data_channels_;
  SctpDataChannels sctp_data_channels_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_
