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

#ifndef TALK_MEDIA_BASE_MEDIACHANNEL_H_
#define TALK_MEDIA_BASE_MEDIACHANNEL_H_

#include <string>
#include <vector>

#include "talk/media/base/codec.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/streamparams.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/window.h"

namespace rtc {
class Buffer;
class RateLimiter;
class Timing;
}

namespace cricket {

struct RtpHeader;

const int kMinRtpHeaderExtensionId = 1;
const int kMaxRtpHeaderExtensionId = 255;
const int kScreencastDefaultFps = 5;
const int kHighStartBitrate = 1500;

// Used in AudioOptions and VideoOptions to signify "unset" values.
template <class T>
class Settable {
 public:
  Settable() : set_(false), val_() {}
  explicit Settable(T val) : set_(true), val_(val) {}

  bool IsSet() const {
    return set_;
  }

  bool Get(T* out) const {
    *out = val_;
    return set_;
  }

  T GetWithDefaultIfUnset(const T& default_value) const {
    return set_ ? val_ : default_value;
  }

  void Set(T val) {
    set_ = true;
    val_ = val;
  }

  void Clear() {
    Set(T());
    set_ = false;
  }

  void SetFrom(const Settable<T>& o) {
    // Set this value based on the value of o, iff o is set.  If this value is
    // set and o is unset, the current value will be unchanged.
    T val;
    if (o.Get(&val)) {
      Set(val);
    }
  }

  std::string ToString() const {
    return set_ ? rtc::ToString(val_) : "";
  }

  bool operator==(const Settable<T>& o) const {
    // Equal if both are unset with any value or both set with the same value.
    return (set_ == o.set_) && (!set_ || (val_ == o.val_));
  }

  bool operator!=(const Settable<T>& o) const {
    return !operator==(o);
  }

 protected:
  void InitializeValue(const T &val) {
    val_ = val;
  }

 private:
  bool set_;
  T val_;
};

template <class T>
static std::string ToStringIfSet(const char* key, const Settable<T>& val) {
  std::string str;
  if (val.IsSet()) {
    str = key;
    str += ": ";
    str += val.ToString();
    str += ", ";
  }
  return str;
}

struct RtpHeaderExtension {
  RtpHeaderExtension() : id(0) {}
  RtpHeaderExtension(const std::string& u, int i) : uri(u), id(i) {}
  std::string uri;
  int id;
  // TODO(juberti): SendRecv direction;

  bool operator==(const RtpHeaderExtension& ext) const {
    // id is a reserved word in objective-c. Therefore the id attribute has to
    // be a fully qualified name in order to compile on IOS.
    return this->id == ext.id &&
        uri == ext.uri;
  }
};

// Returns the named header extension if found among all extensions, NULL
// otherwise.
inline const RtpHeaderExtension* FindHeaderExtension(
    const std::vector<RtpHeaderExtension>& extensions,
    const std::string& name) {
  for (std::vector<RtpHeaderExtension>::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    if (it->uri == name)
      return &(*it);
  }
  return NULL;
}

enum MediaChannelOptions {
  // Tune the stream for conference mode.
  OPT_CONFERENCE = 0x0001
};

class MediaChannel : public sigslot::has_slots<> {
 public:
  class NetworkInterface {
   public:
    enum SocketType { ST_RTP, ST_RTCP };
    virtual bool SendPacket(
        rtc::Buffer* packet,
        rtc::DiffServCodePoint dscp = rtc::DSCP_NO_CHANGE) = 0;
    virtual bool SendRtcp(
        rtc::Buffer* packet,
        rtc::DiffServCodePoint dscp = rtc::DSCP_NO_CHANGE) = 0;
    virtual int SetOption(SocketType type, rtc::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  MediaChannel() : network_interface_(NULL) {}
  virtual ~MediaChannel() {}

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(NetworkInterface *iface) {
    rtc::CritScope cs(&network_interface_crit_);
    network_interface_ = iface;
  }

  // Called when a RTP packet is received.
  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time) = 0;
  // Called when a RTCP packet is received.
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time) = 0;
  // Called when the socket's ability to send has changed.
  virtual void OnReadyToSend(bool ready) = 0;
  // Creates a new outgoing media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddSendStream(const StreamParams& sp) = 0;
  // Removes an outgoing media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveSendStream(uint32 ssrc) = 0;
  // Creates a new incoming media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  // Removes an incoming media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveRecvStream(uint32 ssrc) = 0;

  // Mutes the channel.
  virtual bool MuteStream(uint32 ssrc, bool on) = 0;

  // Sets the RTP extension headers and IDs to use when sending RTP.
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  // Returns the absoulte sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const {
    return -1;
  }
  // Sets the maximum allowed bandwidth to use when sending data.
  virtual bool SetMaxSendBandwidth(int bps) = 0;

  // Base method to send packet using NetworkInterface.
  bool SendPacket(rtc::Buffer* packet) {
    return DoSendPacket(packet, false);
  }

  bool SendRtcp(rtc::Buffer* packet) {
    return DoSendPacket(packet, true);
  }

  int SetOption(NetworkInterface::SocketType type,
                rtc::Socket::Option opt,
                int option) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return -1;

    return network_interface_->SetOption(type, opt, option);
  }

 protected:
  // This method sets DSCP |value| on both RTP and RTCP channels.
  int SetDscp(rtc::DiffServCodePoint value) {
    int ret;
    ret = SetOption(NetworkInterface::ST_RTP,
                    rtc::Socket::OPT_DSCP,
                    value);
    if (ret == 0) {
      ret = SetOption(NetworkInterface::ST_RTCP,
                      rtc::Socket::OPT_DSCP,
                      value);
    }
    return ret;
  }

 private:
  bool DoSendPacket(rtc::Buffer* packet, bool rtcp) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return false;

    return (!rtcp) ? network_interface_->SendPacket(packet) :
                     network_interface_->SendRtcp(packet);
  }

  // |network_interface_| can be accessed from the worker_thread and
  // from any MediaEngine threads. This critical section is to protect accessing
  // of network_interface_ object.
  rtc::CriticalSection network_interface_crit_;
  NetworkInterface* network_interface_;
};

enum SendFlags {
  SEND_NOTHING,
  SEND_RINGBACKTONE,
  SEND_MICROPHONE
};

// The stats information is structured as follows:
// Media are represented by either MediaSenderInfo or MediaReceiverInfo.
// Media contains a vector of SSRC infos that are exclusively used by this
// media. (SSRCs shared between media streams can't be represented.)

// Information about an SSRC.
// This data may be locally recorded, or received in an RTCP SR or RR.
struct SsrcSenderInfo {
  SsrcSenderInfo()
      : ssrc(0),
    timestamp(0) {
  }
  uint32 ssrc;
  double timestamp;  // NTP timestamp, represented as seconds since epoch.
};

struct SsrcReceiverInfo {
  SsrcReceiverInfo()
      : ssrc(0),
        timestamp(0) {
  }
  uint32 ssrc;
  double timestamp;
};

struct MediaSenderInfo {
  MediaSenderInfo()
      : bytes_sent(0),
        packets_sent(0),
        packets_lost(0),
        fraction_lost(0.0),
        rtt_ms(0) {
  }
  void add_ssrc(const SsrcSenderInfo& stat) {
    local_stats.push_back(stat);
  }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32 ssrc) {
    SsrcSenderInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  // Utility accessor for clients that are only interested in ssrc numbers.
  std::vector<uint32> ssrcs() const {
    std::vector<uint32> retval;
    for (std::vector<SsrcSenderInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32 ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }
  int64 bytes_sent;
  int packets_sent;
  int packets_lost;
  float fraction_lost;
  int rtt_ms;
  std::string codec_name;
  std::vector<SsrcSenderInfo> local_stats;
  std::vector<SsrcReceiverInfo> remote_stats;
};

template<class T>
struct VariableInfo {
  VariableInfo()
      : min_val(),
        mean(0.0),
        max_val(),
        variance(0.0) {
  }
  T min_val;
  double mean;
  T max_val;
  double variance;
};

struct MediaReceiverInfo {
  MediaReceiverInfo()
      : bytes_rcvd(0),
        packets_rcvd(0),
        packets_lost(0),
        fraction_lost(0.0) {
  }
  void add_ssrc(const SsrcReceiverInfo& stat) {
    local_stats.push_back(stat);
  }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32 ssrc) {
    SsrcReceiverInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  std::vector<uint32> ssrcs() const {
    std::vector<uint32> retval;
    for (std::vector<SsrcReceiverInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32 ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }

  int64 bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  float fraction_lost;
  std::string codec_name;
  std::vector<SsrcReceiverInfo> local_stats;
  std::vector<SsrcSenderInfo> remote_stats;
};

struct VoiceSenderInfo : public MediaSenderInfo {
  VoiceSenderInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        audio_level(0),
        aec_quality_min(0.0),
        echo_delay_median_ms(0),
        echo_delay_std_ms(0),
        echo_return_loss(0),
        echo_return_loss_enhancement(0),
        typing_noise_detected(false) {
  }

  int ext_seqnum;
  int jitter_ms;
  int audio_level;
  float aec_quality_min;
  int echo_delay_median_ms;
  int echo_delay_std_ms;
  int echo_return_loss;
  int echo_return_loss_enhancement;
  bool typing_noise_detected;
};

struct VoiceReceiverInfo : public MediaReceiverInfo {
  VoiceReceiverInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        jitter_buffer_ms(0),
        jitter_buffer_preferred_ms(0),
        delay_estimate_ms(0),
        audio_level(0),
        expand_rate(0),
        decoding_calls_to_silence_generator(0),
        decoding_calls_to_neteq(0),
        decoding_normal(0),
        decoding_plc(0),
        decoding_cng(0),
        decoding_plc_cng(0),
        capture_start_ntp_time_ms(-1) {
  }

  int ext_seqnum;
  int jitter_ms;
  int jitter_buffer_ms;
  int jitter_buffer_preferred_ms;
  int delay_estimate_ms;
  int audio_level;
  // fraction of synthesized speech inserted through pre-emptive expansion
  float expand_rate;
  int decoding_calls_to_silence_generator;
  int decoding_calls_to_neteq;
  int decoding_normal;
  int decoding_plc;
  int decoding_cng;
  int decoding_plc_cng;
  // Estimated capture start time in NTP time in ms.
  int64 capture_start_ntp_time_ms;
};

struct VideoSenderInfo : public MediaSenderInfo {
  VideoSenderInfo()
      : packets_cached(0),
        firs_rcvd(0),
        plis_rcvd(0),
        nacks_rcvd(0),
        input_frame_width(0),
        input_frame_height(0),
        send_frame_width(0),
        send_frame_height(0),
        framerate_input(0),
        framerate_sent(0),
        nominal_bitrate(0),
        preferred_bitrate(0),
        adapt_reason(0),
        adapt_changes(0),
        capture_jitter_ms(0),
        avg_encode_ms(0),
        encode_usage_percent(0),
        capture_queue_delay_ms_per_s(0) {
  }

  std::vector<SsrcGroup> ssrc_groups;
  int packets_cached;
  int firs_rcvd;
  int plis_rcvd;
  int nacks_rcvd;
  int input_frame_width;
  int input_frame_height;
  int send_frame_width;
  int send_frame_height;
  int framerate_input;
  int framerate_sent;
  int nominal_bitrate;
  int preferred_bitrate;
  int adapt_reason;
  int adapt_changes;
  int capture_jitter_ms;
  int avg_encode_ms;
  int encode_usage_percent;
  int capture_queue_delay_ms_per_s;
  VariableInfo<int> adapt_frame_drops;
  VariableInfo<int> effects_frame_drops;
  VariableInfo<double> capturer_frame_time;
};

struct VideoReceiverInfo : public MediaReceiverInfo {
  VideoReceiverInfo()
      : packets_concealed(0),
        firs_sent(0),
        plis_sent(0),
        nacks_sent(0),
        frame_width(0),
        frame_height(0),
        framerate_rcvd(0),
        framerate_decoded(0),
        framerate_output(0),
        framerate_render_input(0),
        framerate_render_output(0),
        decode_ms(0),
        max_decode_ms(0),
        jitter_buffer_ms(0),
        min_playout_delay_ms(0),
        render_delay_ms(0),
        target_delay_ms(0),
        current_delay_ms(0),
        capture_start_ntp_time_ms(-1) {
  }

  std::vector<SsrcGroup> ssrc_groups;
  int packets_concealed;
  int firs_sent;
  int plis_sent;
  int nacks_sent;
  int frame_width;
  int frame_height;
  int framerate_rcvd;
  int framerate_decoded;
  int framerate_output;
  // Framerate as sent to the renderer.
  int framerate_render_input;
  // Framerate that the renderer reports.
  int framerate_render_output;

  // All stats below are gathered per-VideoReceiver, but some will be correlated
  // across MediaStreamTracks.  NOTE(hta): when sinking stats into per-SSRC
  // structures, reflect this in the new layout.

  // Current frame decode latency.
  int decode_ms;
  // Maximum observed frame decode latency.
  int max_decode_ms;
  // Jitter (network-related) latency.
  int jitter_buffer_ms;
  // Requested minimum playout latency.
  int min_playout_delay_ms;
  // Requested latency to account for rendering delay.
  int render_delay_ms;
  // Target overall delay: network+decode+render, accounting for
  // min_playout_delay_ms.
  int target_delay_ms;
  // Current overall delay, possibly ramping towards target_delay_ms.
  int current_delay_ms;

  // Estimated capture start time in NTP time in ms.
  int64 capture_start_ntp_time_ms;
};

struct DataSenderInfo : public MediaSenderInfo {
  DataSenderInfo()
      : ssrc(0) {
  }

  uint32 ssrc;
};

struct DataReceiverInfo : public MediaReceiverInfo {
  DataReceiverInfo()
      : ssrc(0) {
  }

  uint32 ssrc;
};

struct BandwidthEstimationInfo {
  BandwidthEstimationInfo()
      : available_send_bandwidth(0),
        available_recv_bandwidth(0),
        target_enc_bitrate(0),
        actual_enc_bitrate(0),
        retransmit_bitrate(0),
        transmit_bitrate(0),
        bucket_delay(0),
        total_received_propagation_delta_ms(0) {
  }

  int available_send_bandwidth;
  int available_recv_bandwidth;
  int target_enc_bitrate;
  int actual_enc_bitrate;
  int retransmit_bitrate;
  int transmit_bitrate;
  int bucket_delay;
  // The following stats are only valid when
  // StatsOptions::include_received_propagation_stats is true.
  int total_received_propagation_delta_ms;
  std::vector<int> recent_received_propagation_delta_ms;
  std::vector<int64_t> recent_received_packet_group_arrival_time_ms;
};

struct VoiceMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<VoiceSenderInfo> senders;
  std::vector<VoiceReceiverInfo> receivers;
};

struct VideoMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
    bw_estimations.clear();
  }
  std::vector<VideoSenderInfo> senders;
  std::vector<VideoReceiverInfo> receivers;
  std::vector<BandwidthEstimationInfo> bw_estimations;
};

struct DataMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<DataSenderInfo> senders;
  std::vector<DataReceiverInfo> receivers;
};

struct StatsOptions {
  StatsOptions() : include_received_propagation_stats(false) {}

  bool include_received_propagation_stats;
};

enum DataMessageType {
  // Chrome-Internal use only.  See SctpDataMediaChannel for the actual PPID
  // values.
  DMT_NONE = 0,
  DMT_CONTROL = 1,
  DMT_BINARY = 2,
  DMT_TEXT = 3,
};

// Info about data received in DataMediaChannel.  For use in
// DataMediaChannel::SignalDataReceived and in all of the signals that
// signal fires, on up the chain.
struct ReceiveDataParams {
  // The in-packet stream indentifier.
  // For SCTP, this is really SID, not SSRC.
  uint32 ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;
  // A per-stream value incremented per packet in the stream.
  int seq_num;
  // A per-stream value monotonically increasing with time.
  int timestamp;

  ReceiveDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      seq_num(0),
      timestamp(0) {
  }
};

struct SendDataParams {
  // The in-packet stream indentifier.
  // For SCTP, this is really SID, not SSRC.
  uint32 ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;

  // For SCTP, whether to send messages flagged as ordered or not.
  // If false, messages can be received out of order.
  bool ordered;
  // For SCTP, whether the messages are sent reliably or not.
  // If false, messages may be lost.
  bool reliable;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending up to this many times.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_count;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending for up to this many milliseconds.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_ms;

  SendDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      // TODO(pthatcher): Make these true by default?
      ordered(false),
      reliable(false),
      max_rtx_count(0),
      max_rtx_ms(0) {
  }
};

enum SendDataResult { SDR_SUCCESS, SDR_ERROR, SDR_BLOCK };

class DataMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_SEND_SRTP_ERROR = 200,          // Generic SRTP failure.
    ERROR_SEND_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_ERROR,                // Generic SRTP failure.
    ERROR_RECV_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_REPLAY,               // Packet replay detected.
  };

  virtual ~DataMediaChannel() {}

  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs) = 0;
  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs) = 0;

  virtual bool MuteStream(uint32 ssrc, bool on) { return false; }
  // TODO(pthatcher): Implement this.
  virtual bool GetStats(DataMediaInfo* info) { return true; }

  virtual bool SetSend(bool send) = 0;
  virtual bool SetReceive(bool receive) = 0;

  virtual bool SendData(
      const SendDataParams& params,
      const rtc::Buffer& payload,
      SendDataResult* result = NULL) = 0;
  // Signals when data is received (params, data, len)
  sigslot::signal3<const ReceiveDataParams&,
                   const char*,
                   size_t> SignalDataReceived;
  // Signal errors from MediaChannel.  Arguments are:
  //     ssrc(uint32), and error(DataMediaChannel::Error).
  sigslot::signal2<uint32, DataMediaChannel::Error> SignalMediaError;
  // Signal when the media channel is ready to send the stream. Arguments are:
  //     writable(bool)
  sigslot::signal1<bool> SignalReadyToSend;
  // Signal for notifying that the remote side has closed the DataChannel.
  sigslot::signal1<uint32> SignalStreamClosedRemotely;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIACHANNEL_H_
