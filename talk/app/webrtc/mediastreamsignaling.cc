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

#include "talk/app/webrtc/mediastreamsignaling.h"

#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/mediastreamproxy.h"
#include "talk/app/webrtc/mediastreamtrackproxy.h"
#include "talk/app/webrtc/sctputils.h"
#include "talk/media/sctp/sctpdataengine.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/stringutils.h"

namespace webrtc {

using rtc::scoped_ptr;
using rtc::scoped_refptr;

static bool ParseConstraintsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* options) {
  bool value;
  size_t mandatory_constraints_satisfied = 0;

  // kOfferToReceiveAudio defaults to true according to spec.
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveAudio,
                      &value, &mandatory_constraints_satisfied) || value) {
    options->recv_audio = true;
  }

  // kOfferToReceiveVideo defaults to false according to spec. But
  // if it is an answer and video is offered, we should still accept video
  // per default.
  if (!FindConstraint(constraints,
                      MediaConstraintsInterface::kOfferToReceiveVideo,
                      &value, &mandatory_constraints_satisfied) || value) {
    options->recv_video = true;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kVoiceActivityDetection,
                     &value, &mandatory_constraints_satisfied)) {
    options->vad_enabled = value;
  }

  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kUseRtpMux,
                     &value, &mandatory_constraints_satisfied)) {
    options->bundle_enabled = value;
  } else {
    // kUseRtpMux defaults to true according to spec.
    options->bundle_enabled = true;
  }
  if (FindConstraint(constraints,
                     MediaConstraintsInterface::kIceRestart,
                     &value, &mandatory_constraints_satisfied)) {
    options->transport_options.ice_restart = value;
  } else {
    // kIceRestart defaults to false according to spec.
    options->transport_options.ice_restart = false;
  }

  if (!constraints) {
    return true;
  }
  return mandatory_constraints_satisfied == constraints->GetMandatory().size();
}

// Returns true if if at least one media content is present and
// |options.bundle_enabled| is true.
// Bundle will be enabled  by default if at least one media content is present
// and the constraint kUseRtpMux has not disabled bundle.
static bool EvaluateNeedForBundle(const cricket::MediaSessionOptions& options) {
  return options.bundle_enabled &&
      (options.has_audio() || options.has_video() || options.has_data());
}

static bool MediaContentDirectionHasSend(cricket::MediaContentDirection dir) {
  return dir == cricket::MD_SENDONLY || dir == cricket::MD_SENDRECV;
}

static bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
      (value <= Options::kMaxOfferToReceiveMedia);
}

// Add the stream and RTP data channel info to |session_options|.
static void SetStreams(
    cricket::MediaSessionOptions* session_options,
    const MediaStreamSignaling::RtpDataChannels& rtp_data_channels) {
  session_options->streams.clear();

  // Check for data channels.
  MediaStreamSignaling::RtpDataChannels::const_iterator data_channel_it =
      rtp_data_channels.begin();
  for (; data_channel_it != rtp_data_channels.end(); ++data_channel_it) {
    const DataChannel* channel = data_channel_it->second;
    if (channel->state() == DataChannel::kConnecting ||
        channel->state() == DataChannel::kOpen) {
      // |streamid| and |sync_label| are both set to the DataChannel label
      // here so they can be signaled the same way as MediaStreams and Tracks.
      // For MediaStreams, the sync_label is the MediaStream label and the
      // track label is the same as |streamid|.
      const std::string& streamid = channel->label();
      const std::string& sync_label = channel->label();
      session_options->AddSendStream(
          cricket::MEDIA_TYPE_DATA, streamid, sync_label);
    }
  }
}

// Factory class for creating remote MediaStreams and MediaStreamTracks.
class RemoteMediaStreamFactory {
 public:
  explicit RemoteMediaStreamFactory(rtc::Thread* signaling_thread,
                                    cricket::ChannelManager* channel_manager)
      : signaling_thread_(signaling_thread),
        channel_manager_(channel_manager) {
  }

  rtc::scoped_refptr<MediaStreamInterface> CreateMediaStream(
      const std::string& stream_label) {
    return MediaStreamProxy::Create(
        signaling_thread_, MediaStream::Create(stream_label));
  }

 private:

  rtc::Thread* signaling_thread_;
  cricket::ChannelManager* channel_manager_;
};

MediaStreamSignaling::MediaStreamSignaling(
    rtc::Thread* signaling_thread,
    MediaStreamSignalingObserver* stream_observer,
    cricket::ChannelManager* channel_manager)
    : signaling_thread_(signaling_thread),
      data_channel_factory_(NULL),
      stream_observer_(stream_observer),
      remote_stream_factory_(new RemoteMediaStreamFactory(signaling_thread,
                                                          channel_manager)),
      last_allocated_sctp_even_sid_(-2),
      last_allocated_sctp_odd_sid_(-1) {
}

MediaStreamSignaling::~MediaStreamSignaling() {
}

void MediaStreamSignaling::TearDown() {
  OnDataChannelClose();
}

bool MediaStreamSignaling::IsSctpSidAvailable(int sid) const {
  if (sid < 0 || sid > static_cast<int>(cricket::kMaxSctpSid))
    return false;

  return FindDataChannelBySid(sid) < 0;
}

// Gets the first unused odd/even id based on the DTLS role. If |role| is
// SSL_CLIENT, the allocated id starts from 0 and takes even numbers; otherwise,
// the id starts from 1 and takes odd numbers. Returns false if no id can be
// allocated.
bool MediaStreamSignaling::AllocateSctpSid(rtc::SSLRole role, int* sid) {
  int& last_id = (role == rtc::SSL_CLIENT) ?
      last_allocated_sctp_even_sid_ : last_allocated_sctp_odd_sid_;

  do {
    last_id += 2;
  } while (last_id <= static_cast<int>(cricket::kMaxSctpSid) &&
           !IsSctpSidAvailable(last_id));

  if (last_id > static_cast<int>(cricket::kMaxSctpSid)) {
    return false;
  }

  *sid = last_id;
  return true;
}

bool MediaStreamSignaling::HasDataChannels() const {
  return !rtp_data_channels_.empty() || !sctp_data_channels_.empty();
}

bool MediaStreamSignaling::AddDataChannel(DataChannel* data_channel) {
  ASSERT(data_channel != NULL);
  if (data_channel->data_channel_type() == cricket::DCT_RTP) {
    if (rtp_data_channels_.find(data_channel->label()) !=
        rtp_data_channels_.end()) {
      LOG(LS_ERROR) << "DataChannel with label " << data_channel->label()
                    << " already exists.";
      return false;
    }
    rtp_data_channels_[data_channel->label()] = data_channel;
  } else {
    ASSERT(data_channel->data_channel_type() == cricket::DCT_SCTP);
    sctp_data_channels_.push_back(data_channel);
  }
  return true;
}

bool MediaStreamSignaling::AddDataChannelFromOpenMessage(
    const cricket::ReceiveDataParams& params,
    const rtc::Buffer& payload) {
  if (!data_channel_factory_) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but DataChannels "
                    << "are not supported.";
    return false;
  }

  std::string label;
  InternalDataChannelInit config;
  config.id = params.ssrc;
  if (!ParseDataChannelOpenMessage(payload, &label, &config)) {
    LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                    << params.ssrc;
    return false;
  }
  config.open_handshake_role = InternalDataChannelInit::kAcker;

  scoped_refptr<DataChannel> channel(
      data_channel_factory_->CreateDataChannel(label, &config));
  if (!channel.get()) {
    LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return false;
  }

  stream_observer_->OnAddDataChannel(channel);
  return true;
}

void MediaStreamSignaling::RemoveSctpDataChannel(int sid) {
  ASSERT(sid >= 0);
  for (SctpDataChannels::iterator iter = sctp_data_channels_.begin();
       iter != sctp_data_channels_.end();
       ++iter) {
    if ((*iter)->id() == sid) {
      sctp_data_channels_.erase(iter);

      if (rtc::IsEven(sid) && sid <= last_allocated_sctp_even_sid_) {
        last_allocated_sctp_even_sid_ = sid - 2;
      } else if (rtc::IsOdd(sid) && sid <= last_allocated_sctp_odd_sid_) {
        last_allocated_sctp_odd_sid_ = sid - 2;
      }
      return;
    }
  }
}

bool MediaStreamSignaling::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;
  if (!IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) ||
      !IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video)) {
    return false;
  }

  SetStreams(session_options, rtp_data_channels_);

  // According to the spec, offer to receive audio/video if the constraint is
  // not set and there are send streams.
  if (rtc_options.offer_to_receive_audio == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_audio =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_AUDIO);
  } else {
    session_options->recv_audio = (rtc_options.offer_to_receive_audio > 0);
  }
  if (rtc_options.offer_to_receive_video == RTCOfferAnswerOptions::kUndefined) {
    session_options->recv_video =
        session_options->HasSendMediaStream(cricket::MEDIA_TYPE_VIDEO);
  } else {
    session_options->recv_video = (rtc_options.offer_to_receive_video > 0);
  }

  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->transport_options.ice_restart = rtc_options.ice_restart;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;

  session_options->bundle_enabled = EvaluateNeedForBundle(*session_options);
  return true;
}

bool MediaStreamSignaling::GetOptionsForAnswer(
    const MediaConstraintsInterface* constraints,
    cricket::MediaSessionOptions* options) {
  SetStreams(options, rtp_data_channels_);

  options->recv_audio = false;
  options->recv_video = false;
  if (!ParseConstraintsForAnswer(constraints, options)) {
    return false;
  }
  options->bundle_enabled = EvaluateNeedForBundle(*options);
  return true;
}

// Updates or creates remote MediaStream objects given a
// remote SessionDesription.
// If the remote SessionDesription contains new remote MediaStreams
// the observer OnAddStream method is called. If a remote MediaStream is missing
// from the remote SessionDescription OnRemoveStream is called.
void MediaStreamSignaling::OnRemoteDescriptionChanged(
    const SessionDescriptionInterface* desc) {
  const cricket::SessionDescription* remote_desc = desc->description();
  rtc::scoped_refptr<StreamCollection> new_streams(
      StreamCollection::Create());

  // Update the DataChannels with the information from the remote peer.
  const cricket::ContentInfo* data_content = GetFirstDataContent(remote_desc);
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(
            data_desc->protocol().data(), cricket::kMediaProtocolRtpPrefix)) {
      UpdateRemoteRtpDataChannels(data_desc->streams());
    }
  }
}

void MediaStreamSignaling::OnLocalDescriptionChanged(
    const SessionDescriptionInterface* desc) {
  const cricket::ContentInfo* data_content =
      GetFirstDataContent(desc->description());
  if (data_content) {
    const cricket::DataContentDescription* data_desc =
        static_cast<const cricket::DataContentDescription*>(
            data_content->description);
    if (rtc::starts_with(
            data_desc->protocol().data(), cricket::kMediaProtocolRtpPrefix)) {
      UpdateLocalRtpDataChannels(data_desc->streams());
    }
  }
}

void MediaStreamSignaling::OnDataChannelClose() {
  // Use a temporary copy of the RTP/SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  RtpDataChannels temp_rtp_dcs;
  temp_rtp_dcs.swap(rtp_data_channels_);
  RtpDataChannels::iterator it1 = temp_rtp_dcs.begin();
  for (; it1 != temp_rtp_dcs.end(); ++it1) {
    it1->second->OnDataEngineClose();
  }

  SctpDataChannels temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  SctpDataChannels::iterator it2 = temp_sctp_dcs.begin();
  for (; it2 != temp_sctp_dcs.end(); ++it2) {
    (*it2)->OnDataEngineClose();
  }
}

void MediaStreamSignaling::UpdateLocalRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (cricket::StreamParamsVec::const_iterator it =streams.begin();
       it != streams.end(); ++it) {
    // |it->sync_label| is actually the data channel label. The reason is that
    // we use the same naming of data channels as we do for
    // MediaStreams and Tracks.
    // For MediaStreams, the sync_label is the MediaStream label and the
    // track label is the same as |streamid|.
    const std::string& channel_label = it->sync_label;
    RtpDataChannels::iterator data_channel_it =
        rtp_data_channels_.find(channel_label);
    if (!VERIFY(data_channel_it != rtp_data_channels_.end())) {
      continue;
    }
    // Set the SSRC the data channel should use for sending.
    data_channel_it->second->SetSendSsrc(it->first_ssrc());
    existing_channels.push_back(data_channel_it->first);
  }

  UpdateClosingDataChannels(existing_channels, true);
}

void MediaStreamSignaling::UpdateRemoteRtpDataChannels(
    const cricket::StreamParamsVec& streams) {
  std::vector<std::string> existing_channels;

  // Find new and active data channels.
  for (cricket::StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    // The data channel label is either the mslabel or the SSRC if the mslabel
    // does not exist. Ex a=ssrc:444330170 mslabel:test1.
    std::string label = it->sync_label.empty() ?
        rtc::ToString(it->first_ssrc()) : it->sync_label;
    RtpDataChannels::iterator data_channel_it =
        rtp_data_channels_.find(label);
    if (data_channel_it == rtp_data_channels_.end()) {
      // This is a new data channel.
      CreateRemoteDataChannel(label, it->first_ssrc());
    } else {
      data_channel_it->second->SetReceiveSsrc(it->first_ssrc());
    }
    existing_channels.push_back(label);
  }

  UpdateClosingDataChannels(existing_channels, false);
}

void MediaStreamSignaling::UpdateClosingDataChannels(
    const std::vector<std::string>& active_channels, bool is_local_update) {
  RtpDataChannels::iterator it = rtp_data_channels_.begin();
  while (it != rtp_data_channels_.end()) {
    DataChannel* data_channel = it->second;
    if (std::find(active_channels.begin(), active_channels.end(),
                  data_channel->label()) != active_channels.end()) {
      ++it;
      continue;
    }

    if (is_local_update)
      data_channel->SetSendSsrc(0);
    else
      data_channel->RemotePeerRequestClose();

    if (data_channel->state() == DataChannel::kClosed) {
      rtp_data_channels_.erase(it);
      it = rtp_data_channels_.begin();
    } else {
      ++it;
    }
  }
}

void MediaStreamSignaling::CreateRemoteDataChannel(const std::string& label,
                                                   uint32 remote_ssrc) {
  if (!data_channel_factory_) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but DataChannels "
                    << "are not supported.";
    return;
  }
  scoped_refptr<DataChannel> channel(
      data_channel_factory_->CreateDataChannel(label, NULL));
  if (!channel.get()) {
    LOG(LS_WARNING) << "Remote peer requested a DataChannel but"
                    << "CreateDataChannel failed.";
    return;
  }
  channel->SetReceiveSsrc(remote_ssrc);
  stream_observer_->OnAddDataChannel(channel);
}

void MediaStreamSignaling::OnDataTransportCreatedForSctp() {
  SctpDataChannels::iterator it = sctp_data_channels_.begin();
  for (; it != sctp_data_channels_.end(); ++it) {
    (*it)->OnTransportChannelCreated();
  }
}

void MediaStreamSignaling::OnDtlsRoleReadyForSctp(rtc::SSLRole role) {
  SctpDataChannels::iterator it = sctp_data_channels_.begin();
  for (; it != sctp_data_channels_.end(); ++it) {
    if ((*it)->id() < 0) {
      int sid;
      if (!AllocateSctpSid(role, &sid)) {
        LOG(LS_ERROR) << "Failed to allocate SCTP sid.";
        continue;
      }
      (*it)->SetSctpSid(sid);
    }
  }
}


void MediaStreamSignaling::OnRemoteSctpDataChannelClosed(uint32 sid) {
  int index = FindDataChannelBySid(sid);
  if (index < 0) {
    LOG(LS_WARNING) << "Unexpected sid " << sid
                    << " of the remotely closed DataChannel.";
    return;
  }
  sctp_data_channels_[index]->Close();
}

const MediaStreamSignaling::TrackInfo*
MediaStreamSignaling::FindTrackInfo(
    const MediaStreamSignaling::TrackInfos& infos,
    const std::string& stream_label,
    const std::string track_id) const {

  for (TrackInfos::const_iterator it = infos.begin();
      it != infos.end(); ++it) {
    if (it->stream_label == stream_label && it->track_id == track_id)
      return &*it;
  }
  return NULL;
}

int MediaStreamSignaling::FindDataChannelBySid(int sid) const {
  for (size_t i = 0; i < sctp_data_channels_.size(); ++i) {
    if (sctp_data_channels_[i]->id() == sid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace webrtc
