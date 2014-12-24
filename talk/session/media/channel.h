/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_SESSION_MEDIA_CHANNEL_H_
#define TALK_SESSION_MEDIA_CHANNEL_H_

#include <string>
#include <vector>

#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"
#include "talk/media/base/streamparams.h"
#include "webrtc/p2p/base/session.h"
#include "webrtc/p2p/client/socketmonitor.h"
#include "talk/session/media/bundlefilter.h"
#include "talk/session/media/mediamonitor.h"
#include "talk/session/media/mediasession.h"
#include "talk/session/media/rtcpmuxfilter.h"
#include "talk/session/media/srtpfilter.h"
#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/network.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/window.h"

namespace cricket {

struct CryptoParams;
class MediaContentDescription;
struct TypingMonitorOptions;
class TypingMonitor;
struct ViewRequest;

enum SinkType {
  SINK_PRE_CRYPTO,  // Sink packets before encryption or after decryption.
  SINK_POST_CRYPTO  // Sink packets after encryption or before decryption.
};

// BaseChannel contains logic common to voice and video, including
// enable/mute, marshaling calls to a worker thread, and
// connection and media monitors.
//
// WARNING! SUBCLASSES MUST CALL Deinit() IN THEIR DESTRUCTORS!
// This is required to avoid a data race between the destructor modifying the
// vtable, and the media channel's thread using BaseChannel as the
// NetworkInterface.

class BaseChannel
    : public rtc::MessageHandler, public sigslot::has_slots<>,
      public MediaChannel::NetworkInterface {
 public:
  BaseChannel(rtc::Thread* thread, MediaEngineInterface* media_engine,
              MediaChannel* channel, BaseSession* session,
              const std::string& content_name, bool rtcp);
  virtual ~BaseChannel();
  bool Init(TransportChannel* transport_channel,
            TransportChannel* rtcp_transport_channel);
  // Deinit may be called multiple times and is simply ignored if it's alreay
  // done.
  void Deinit();

  rtc::Thread* worker_thread() const { return worker_thread_; }
  BaseSession* session() const { return session_; }
  const std::string& content_name() { return content_name_; }
  TransportChannel* transport_channel() const {
    return transport_channel_;
  }
  TransportChannel* rtcp_transport_channel() const {
    return rtcp_transport_channel_;
  }
  bool enabled() const { return enabled_; }

  // This function returns true if we are using SRTP.
  bool secure() const { return srtp_filter_.IsActive(); }
  // The following function returns true if we are using
  // DTLS-based keying. If you turned off SRTP later, however
  // you could have secure() == false and dtls_secure() == true.
  bool secure_dtls() const { return dtls_keyed_; }
  // This function returns true if we require secure channel for call setup.
  bool secure_required() const { return secure_required_; }

  bool writable() const { return writable_; }
  bool IsStreamMuted(uint32 ssrc);

  // Channel control
  bool SetLocalContent(const MediaContentDescription* content,
                       ContentAction action,
                       std::string* error_desc);
  bool SetRemoteContent(const MediaContentDescription* content,
                        ContentAction action,
                        std::string* error_desc);

  bool Enable(bool enable);

  // Multiplexing
  bool AddRecvStream(const StreamParams& sp);
  bool RemoveRecvStream(uint32 ssrc);
  bool AddSendStream(const StreamParams& sp);
  bool RemoveSendStream(uint32 ssrc);

  // Monitoring
  void StartConnectionMonitor(int cms);
  void StopConnectionMonitor();

  void set_srtp_signal_silent_time(uint32 silent_time) {
    srtp_filter_.set_signal_silent_time(silent_time);
  }

  void set_content_name(const std::string& content_name) {
    ASSERT(signaling_thread()->IsCurrent());
    ASSERT(!writable_);
    if (session_->state() != BaseSession::STATE_INIT) {
      LOG(LS_ERROR) << "Content name for a channel can be changed only "
                    << "when BaseSession is in STATE_INIT state.";
      return;
    }
    content_name_ = content_name;
  }

  template <class T>
  void RegisterSendSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool),
                        SinkType type) {
    rtc::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalSendPacketPostCrypto.disconnect(sink);
      SignalSendPacketPostCrypto.connect(sink, OnPacket);
    } else {
      SignalSendPacketPreCrypto.disconnect(sink);
      SignalSendPacketPreCrypto.connect(sink, OnPacket);
    }
  }

  void UnregisterSendSink(sigslot::has_slots<>* sink,
                          SinkType type) {
    rtc::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalSendPacketPostCrypto.disconnect(sink);
    } else {
      SignalSendPacketPreCrypto.disconnect(sink);
    }
  }

  bool HasSendSinks(SinkType type) {
    rtc::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      return !SignalSendPacketPostCrypto.is_empty();
    } else {
      return !SignalSendPacketPreCrypto.is_empty();
    }
  }

  template <class T>
  void RegisterRecvSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool),
                        SinkType type) {
    rtc::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalRecvPacketPostCrypto.disconnect(sink);
      SignalRecvPacketPostCrypto.connect(sink, OnPacket);
    } else {
      SignalRecvPacketPreCrypto.disconnect(sink);
      SignalRecvPacketPreCrypto.connect(sink, OnPacket);
    }
  }

  void UnregisterRecvSink(sigslot::has_slots<>* sink,
                          SinkType type) {
    rtc::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalRecvPacketPostCrypto.disconnect(sink);
    } else {
      SignalRecvPacketPreCrypto.disconnect(sink);
    }
  }

  bool HasRecvSinks(SinkType type) {
    rtc::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      return !SignalRecvPacketPostCrypto.is_empty();
    } else {
      return !SignalRecvPacketPreCrypto.is_empty();
    }
  }

  BundleFilter* bundle_filter() { return &bundle_filter_; }

  const std::vector<StreamParams>& local_streams() const {
    return local_streams_;
  }
  const std::vector<StreamParams>& remote_streams() const {
    return remote_streams_;
  }

  // Used for latency measurements.
  sigslot::signal1<BaseChannel*> SignalFirstPacketReceived;

  // Used to alert UI when the muted status changes, perhaps autonomously.
  sigslot::repeater2<BaseChannel*, bool> SignalAutoMuted;

  // Made public for easier testing.
  void SetReadyToSend(TransportChannel* channel, bool ready);

 protected:
  MediaEngineInterface* media_engine() const { return media_engine_; }
  virtual MediaChannel* media_channel() const { return media_channel_; }
  void set_rtcp_transport_channel(TransportChannel* transport);
  bool was_ever_writable() const { return was_ever_writable_; }
  void set_local_content_direction(MediaContentDirection direction) {
    local_content_direction_ = direction;
  }
  void set_remote_content_direction(MediaContentDirection direction) {
    remote_content_direction_ = direction;
  }
  bool IsReadyToReceive() const;
  bool IsReadyToSend() const;
  rtc::Thread* signaling_thread() { return session_->signaling_thread(); }
  SrtpFilter* srtp_filter() { return &srtp_filter_; }
  bool rtcp() const { return rtcp_; }

  void FlushRtcpMessages();

  // NetworkInterface implementation, called by MediaEngine
  virtual bool SendPacket(rtc::Buffer* packet,
                          rtc::DiffServCodePoint dscp);
  virtual bool SendRtcp(rtc::Buffer* packet,
                        rtc::DiffServCodePoint dscp);
  virtual int SetOption(SocketType type, rtc::Socket::Option o, int val);

  // From TransportChannel
  void OnWritableState(TransportChannel* channel);
  virtual void OnChannelRead(TransportChannel* channel,
                             const char* data,
                             size_t len,
                             const rtc::PacketTime& packet_time,
                             int flags);
  void OnReadyToSend(TransportChannel* channel);

  bool PacketIsRtcp(const TransportChannel* channel, const char* data,
                    size_t len);
  bool SendPacket(bool rtcp, rtc::Buffer* packet,
                  rtc::DiffServCodePoint dscp);
  virtual bool WantsPacket(bool rtcp, rtc::Buffer* packet);
  void HandlePacket(bool rtcp, rtc::Buffer* packet,
                    const rtc::PacketTime& packet_time);

  // Apply the new local/remote session description.
  void OnNewLocalDescription(BaseSession* session, ContentAction action);
  void OnNewRemoteDescription(BaseSession* session, ContentAction action);

  void EnableMedia_w();
  void DisableMedia_w();
  void ChannelWritable_w();
  void ChannelNotWritable_w();
  bool AddRecvStream_w(const StreamParams& sp);
  bool RemoveRecvStream_w(uint32 ssrc);
  bool AddSendStream_w(const StreamParams& sp);
  bool RemoveSendStream_w(uint32 ssrc);
  virtual bool ShouldSetupDtlsSrtp() const;
  // Do the DTLS key expansion and impose it on the SRTP/SRTCP filters.
  // |rtcp_channel| indicates whether to set up the RTP or RTCP filter.
  bool SetupDtlsSrtp(bool rtcp_channel);
  // Set the DTLS-SRTP cipher policy on this channel as appropriate.
  bool SetDtlsSrtpCiphers(TransportChannel *tc, bool rtcp);

  virtual void ChangeState() = 0;

  // Gets the content info appropriate to the channel (audio or video).
  virtual const ContentInfo* GetFirstContent(
      const SessionDescription* sdesc) = 0;
  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            ContentAction action,
                            std::string* error_desc);
  bool UpdateRemoteStreams_w(const std::vector<StreamParams>& streams,
                             ContentAction action,
                             std::string* error_desc);
  bool SetBaseLocalContent_w(const MediaContentDescription* content,
                             ContentAction action,
                             std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc) = 0;
  bool SetBaseRemoteContent_w(const MediaContentDescription* content,
                              ContentAction action,
                              std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc) = 0;

  // Helper method to get RTP Absoulute SendTime extension header id if
  // present in remote supported extensions list.
  void MaybeCacheRtpAbsSendTimeHeaderExtension(
    const std::vector<RtpHeaderExtension>& extensions);

  bool SetRecvRtpHeaderExtensions_w(const MediaContentDescription* content,
                                    MediaChannel* media_channel,
                                    std::string* error_desc);
  bool SetSendRtpHeaderExtensions_w(const MediaContentDescription* content,
                                    MediaChannel* media_channel,
                                    std::string* error_desc);

  bool CheckSrtpConfig(const std::vector<CryptoParams>& cryptos,
                       bool* dtls,
                       std::string* error_desc);
  bool SetSrtp_w(const std::vector<CryptoParams>& params,
                 ContentAction action,
                 ContentSource src,
                 std::string* error_desc);
  bool SetRtcpMux_w(bool enable,
                    ContentAction action,
                    ContentSource src,
                    std::string* error_desc);

  // From MessageHandler
  virtual void OnMessage(rtc::Message* pmsg);

  // Handled in derived classes
  // Get the SRTP ciphers to use for RTP media
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const = 0;
  virtual void OnConnectionMonitorUpdate(SocketMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) = 0;

  // Helper function for invoking bool-returning methods on the worker thread.
  template <class FunctorT>
  bool InvokeOnWorker(const FunctorT& functor) {
    return worker_thread_->Invoke<bool>(functor);
  }

 private:
  sigslot::signal3<const void*, size_t, bool> SignalSendPacketPreCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalSendPacketPostCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalRecvPacketPreCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalRecvPacketPostCrypto;
  rtc::CriticalSection signal_send_packet_cs_;
  rtc::CriticalSection signal_recv_packet_cs_;

  rtc::Thread* worker_thread_;
  MediaEngineInterface* media_engine_;
  BaseSession* session_;
  MediaChannel* media_channel_;
  std::vector<StreamParams> local_streams_;
  std::vector<StreamParams> remote_streams_;

  std::string content_name_;
  bool rtcp_;
  TransportChannel* transport_channel_;
  TransportChannel* rtcp_transport_channel_;
  SrtpFilter srtp_filter_;
  RtcpMuxFilter rtcp_mux_filter_;
  BundleFilter bundle_filter_;
  rtc::scoped_ptr<SocketMonitor> socket_monitor_;
  bool enabled_;
  bool writable_;
  bool rtp_ready_to_send_;
  bool rtcp_ready_to_send_;
  bool was_ever_writable_;
  MediaContentDirection local_content_direction_;
  MediaContentDirection remote_content_direction_;
  bool has_received_packet_;
  bool dtls_keyed_;
  bool secure_required_;
  int rtp_abs_sendtime_extn_id_;
};

// DataChannel is a specialization for data.
class DataChannel : public BaseChannel {
 public:
  DataChannel(rtc::Thread* thread,
              DataMediaChannel* media_channel,
              BaseSession* session,
              const std::string& content_name,
              bool rtcp);
  ~DataChannel();
  bool Init();

  virtual bool SendData(const SendDataParams& params,
                        const rtc::Buffer& payload,
                        SendDataResult* result);

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();

  // Should be called on the signaling thread only.
  bool ready_to_send_data() const {
    return ready_to_send_data_;
  }

  sigslot::signal2<DataChannel*, const DataMediaInfo&> SignalMediaMonitor;
  sigslot::signal2<DataChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;
  sigslot::signal3<DataChannel*, uint32, DataMediaChannel::Error>
      SignalMediaError;
  sigslot::signal3<DataChannel*,
                   const ReceiveDataParams&,
                   const rtc::Buffer&>
      SignalDataReceived;
  // Signal for notifying when the channel becomes ready to send data.
  // That occurs when the channel is enabled, the transport is writable,
  // both local and remote descriptions are set, and the channel is unblocked.
  sigslot::signal1<bool> SignalReadyToSendData;
  // Signal for notifying that the remote side has closed the DataChannel.
  sigslot::signal1<uint32> SignalStreamClosedRemotely;

 protected:
  // downcasts a MediaChannel.
  virtual DataMediaChannel* media_channel() const {
    return static_cast<DataMediaChannel*>(BaseChannel::media_channel());
  }

 private:
  struct SendDataMessageData : public rtc::MessageData {
    SendDataMessageData(const SendDataParams& params,
                        const rtc::Buffer* payload,
                        SendDataResult* result)
        : params(params),
          payload(payload),
          result(result),
          succeeded(false) {
    }

    const SendDataParams& params;
    const rtc::Buffer* payload;
    SendDataResult* result;
    bool succeeded;
  };

  struct DataReceivedMessageData : public rtc::MessageData {
    // We copy the data because the data will become invalid after we
    // handle DataMediaChannel::SignalDataReceived but before we fire
    // SignalDataReceived.
    DataReceivedMessageData(
        const ReceiveDataParams& params, const char* data, size_t len)
        : params(params),
          payload(data, len) {
    }
    const ReceiveDataParams params;
    const rtc::Buffer payload;
  };

  typedef rtc::TypedMessageData<bool> DataChannelReadyToSendMessageData;

  // overrides from BaseChannel
  virtual const ContentInfo* GetFirstContent(const SessionDescription* sdesc);
  // If data_channel_type_ is DCT_NONE, set it.  Otherwise, check that
  // it's the same as what was set previously.  Returns false if it's
  // set to one type one type and changed to another type later.
  bool SetDataChannelType(DataChannelType new_data_channel_type,
                          std::string* error_desc);
  // Same as SetDataChannelType, but extracts the type from the
  // DataContentDescription.
  bool SetDataChannelTypeFromContent(const DataContentDescription* content,
                                     std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc);
  virtual void ChangeState();
  virtual bool WantsPacket(bool rtcp, rtc::Buffer* packet);

  virtual void OnMessage(rtc::Message* pmsg);
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const;
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos);
  virtual void OnMediaMonitorUpdate(
      DataMediaChannel* media_channel, const DataMediaInfo& info);
  virtual bool ShouldSetupDtlsSrtp() const;
  void OnDataReceived(
      const ReceiveDataParams& params, const char* data, size_t len);
  void OnDataChannelError(uint32 ssrc, DataMediaChannel::Error error);
  void OnDataChannelReadyToSend(bool writable);
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);
  void OnStreamClosedRemotely(uint32 sid);

  rtc::scoped_ptr<DataMediaMonitor> media_monitor_;
  // TODO(pthatcher): Make a separate SctpDataChannel and
  // RtpDataChannel instead of using this.
  DataChannelType data_channel_type_;
  bool ready_to_send_data_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CHANNEL_H_
