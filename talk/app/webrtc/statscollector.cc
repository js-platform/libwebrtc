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

#include "talk/app/webrtc/statscollector.h"

#include <utility>
#include <vector>

#include "talk/session/media/channel.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/timing.h"

namespace webrtc {
namespace {

// The following is the enum RTCStatsIceCandidateType from
// http://w3c.github.io/webrtc-stats/#rtcstatsicecandidatetype-enum such that
// our stats report for ice candidate type could conform to that.
const char STATSREPORT_LOCAL_PORT_TYPE[] = "host";
const char STATSREPORT_STUN_PORT_TYPE[] = "serverreflexive";
const char STATSREPORT_PRFLX_PORT_TYPE[] = "peerreflexive";
const char STATSREPORT_RELAY_PORT_TYPE[] = "relayed";

// Strings used by the stats collector to report adapter types. This fits the
// general stype of http://w3c.github.io/webrtc-stats than what
// AdapterTypeToString does.
const char* STATSREPORT_ADAPTER_TYPE_ETHERNET = "lan";
const char* STATSREPORT_ADAPTER_TYPE_WIFI = "wlan";
const char* STATSREPORT_ADAPTER_TYPE_WWAN = "wwan";
const char* STATSREPORT_ADAPTER_TYPE_VPN = "vpn";

double GetTimeNow() {
  return rtc::Timing::WallTimeNow() * rtc::kNumMillisecsPerSec;
}

bool GetTransportIdFromProxy(const cricket::ProxyTransportMap& map,
                             const std::string& proxy,
                             std::string* transport) {
  // TODO(hta): Remove handling of empty proxy name once tests do not use it.
  if (proxy.empty()) {
    transport->clear();
    return true;
  }

  cricket::ProxyTransportMap::const_iterator found = map.find(proxy);
  if (found == map.end()) {
    LOG(LS_ERROR) << "No transport ID mapping for " << proxy;
    return false;
  }

  std::ostringstream ost;
  // Component 1 is always used for RTP.
  ost << "Channel-" << found->second << "-1";
  *transport = ost.str();
  return true;
}

std::string StatsId(const std::string& type, const std::string& id) {
  return type + "_" + id;
}

std::string StatsId(const std::string& type, const std::string& id,
                    StatsCollector::TrackDirection direction) {
  ASSERT(direction == StatsCollector::kSending ||
         direction == StatsCollector::kReceiving);

  // Strings for the direction of the track.
  const char kSendDirection[] = "send";
  const char kRecvDirection[] = "recv";

  const std::string direction_id = (direction == StatsCollector::kSending) ?
      kSendDirection : kRecvDirection;
  return type + "_" + id + "_" + direction_id;
}

bool ExtractValueFromReport(
    const StatsReport& report,
    StatsReport::StatsValueName name,
    std::string* value) {
  StatsReport::Values::const_iterator it = report.values.begin();
  for (; it != report.values.end(); ++it) {
    if (it->name == name) {
      *value = it->value;
      return true;
    }
  }
  return false;
}

void ExtractStats(const cricket::BandwidthEstimationInfo& info,
                  double stats_gathering_started,
                  PeerConnectionInterface::StatsOutputLevel level,
                  StatsReport* report) {
  ASSERT(report->id == StatsReport::kStatsReportVideoBweId);
  report->type = StatsReport::kStatsReportTypeBwe;

  // Clear out stats from previous GatherStats calls if any.
  if (report->timestamp != stats_gathering_started) {
    report->values.clear();
    report->timestamp = stats_gathering_started;
  }

  report->AddValue(StatsReport::kStatsValueNameAvailableSendBandwidth,
                   info.available_send_bandwidth);
  report->AddValue(StatsReport::kStatsValueNameAvailableReceiveBandwidth,
                   info.available_recv_bandwidth);
  report->AddValue(StatsReport::kStatsValueNameTargetEncBitrate,
                   info.target_enc_bitrate);
  report->AddValue(StatsReport::kStatsValueNameActualEncBitrate,
                   info.actual_enc_bitrate);
  report->AddValue(StatsReport::kStatsValueNameRetransmitBitrate,
                   info.retransmit_bitrate);
  report->AddValue(StatsReport::kStatsValueNameTransmitBitrate,
                   info.transmit_bitrate);
  report->AddValue(StatsReport::kStatsValueNameBucketDelay,
                   info.bucket_delay);
  if (level >= PeerConnectionInterface::kStatsOutputLevelDebug) {
    report->AddValue(
        StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaSumDebug,
        info.total_received_propagation_delta_ms);
    if (info.recent_received_propagation_delta_ms.size() > 0) {
      report->AddValue(
          StatsReport::kStatsValueNameRecvPacketGroupPropagationDeltaDebug,
          info.recent_received_propagation_delta_ms);
      report->AddValue(
          StatsReport::kStatsValueNameRecvPacketGroupArrivalTimeDebug,
          info.recent_received_packet_group_arrival_time_ms);
    }
  }
}

void ExtractRemoteStats(const cricket::MediaSenderInfo& info,
                        StatsReport* report) {
  report->timestamp = info.remote_stats[0].timestamp;
  // TODO(hta): Extract some stats here.
}

void ExtractRemoteStats(const cricket::MediaReceiverInfo& info,
                        StatsReport* report) {
  report->timestamp = info.remote_stats[0].timestamp;
  // TODO(hta): Extract some stats here.
}

// Template to extract stats from a data vector.
// In order to use the template, the functions that are called from it,
// ExtractStats and ExtractRemoteStats, must be defined and overloaded
// for each type.
template<typename T>
void ExtractStatsFromList(const std::vector<T>& data,
                          const std::string& transport_id,
                          StatsCollector* collector,
                          StatsCollector::TrackDirection direction) {
  typename std::vector<T>::const_iterator it = data.begin();
  for (; it != data.end(); ++it) {
    std::string id;
    uint32 ssrc = it->ssrc();
    // Each track can have stats for both local and remote objects.
    // TODO(hta): Handle the case of multiple SSRCs per object.
    StatsReport* report = collector->PrepareLocalReport(ssrc, transport_id,
                                                        direction);
    if (report)
      ExtractStats(*it, report);

    if (it->remote_stats.size() > 0) {
      report = collector->PrepareRemoteReport(ssrc, transport_id,
                                              direction);
      if (!report) {
        continue;
      }
      ExtractRemoteStats(*it, report);
    }
  }
}

}  // namespace

const char* IceCandidateTypeToStatsType(const std::string& candidate_type) {
  if (candidate_type == cricket::LOCAL_PORT_TYPE) {
    return STATSREPORT_LOCAL_PORT_TYPE;
  }
  if (candidate_type == cricket::STUN_PORT_TYPE) {
    return STATSREPORT_STUN_PORT_TYPE;
  }
  if (candidate_type == cricket::PRFLX_PORT_TYPE) {
    return STATSREPORT_PRFLX_PORT_TYPE;
  }
  if (candidate_type == cricket::RELAY_PORT_TYPE) {
    return STATSREPORT_RELAY_PORT_TYPE;
  }
  ASSERT(false);
  return "unknown";
}

const char* AdapterTypeToStatsType(rtc::AdapterType type) {
  switch (type) {
    case rtc::ADAPTER_TYPE_UNKNOWN:
      return "unknown";
    case rtc::ADAPTER_TYPE_ETHERNET:
      return STATSREPORT_ADAPTER_TYPE_ETHERNET;
    case rtc::ADAPTER_TYPE_WIFI:
      return STATSREPORT_ADAPTER_TYPE_WIFI;
    case rtc::ADAPTER_TYPE_CELLULAR:
      return STATSREPORT_ADAPTER_TYPE_WWAN;
    case rtc::ADAPTER_TYPE_VPN:
      return STATSREPORT_ADAPTER_TYPE_VPN;
    default:
      ASSERT(false);
      return "";
  }
}

StatsCollector::StatsCollector(WebRtcSession* session)
    : session_(session), stats_gathering_started_(0) {
  ASSERT(session_);
}

StatsCollector::~StatsCollector() {
  ASSERT(session_->signaling_thread()->IsCurrent());
}

void StatsCollector::GetStats(StatsReports* reports) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(reports != NULL);
  ASSERT(reports->empty());

  StatsSet::const_iterator it;
  for (it = reports_.begin(); it != reports_.end(); ++it)
    reports->push_back(&(*it));
  return;
}


void
StatsCollector::UpdateStats(PeerConnectionInterface::StatsOutputLevel level) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  double time_now = GetTimeNow();
  // Calls to UpdateStats() that occur less than kMinGatherStatsPeriod number of
  // ms apart will be ignored.
  const double kMinGatherStatsPeriod = 50;
  if (stats_gathering_started_ != 0 &&
      stats_gathering_started_ + kMinGatherStatsPeriod > time_now) {
    return;
  }
  stats_gathering_started_ = time_now;

  if (session_) {
    ExtractSessionInfo();
  }
}

StatsReport* StatsCollector::PrepareLocalReport(
    uint32 ssrc,
    const std::string& transport_id,
    TrackDirection direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  const std::string ssrc_id = rtc::ToString<uint32>(ssrc);
  StatsReport* report = reports_.Find(
      StatsId(StatsReport::kStatsReportTypeSsrc, ssrc_id, direction));

  report = GetOrCreateReport(
      StatsReport::kStatsReportTypeSsrc, ssrc_id, direction);

  // Clear out stats from previous GatherStats calls if any.
  // This is required since the report will be returned for the new values.
  // Having the old values in the report will lead to multiple values with
  // the same name.
  // TODO(xians): Consider changing StatsReport to use map instead of vector.
  report->values.clear();
  report->timestamp = stats_gathering_started_;

  report->AddValue(StatsReport::kStatsValueNameSsrc, ssrc_id);
  // Add the mapping of SSRC to transport.
  report->AddValue(StatsReport::kStatsValueNameTransportId,
                   transport_id);
  return report;
}

StatsReport* StatsCollector::PrepareRemoteReport(
    uint32 ssrc,
    const std::string& transport_id,
    TrackDirection direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  const std::string ssrc_id = rtc::ToString<uint32>(ssrc);
  StatsReport* report = reports_.Find(
      StatsId(StatsReport::kStatsReportTypeRemoteSsrc, ssrc_id, direction));

  report = GetOrCreateReport(
      StatsReport::kStatsReportTypeRemoteSsrc, ssrc_id, direction);

  // Clear out stats from previous GatherStats calls if any.
  // The timestamp will be added later. Zero it for debugging.
  report->values.clear();
  report->timestamp = 0;

  report->AddValue(StatsReport::kStatsValueNameSsrc, ssrc_id);
  // Add the mapping of SSRC to transport.
  report->AddValue(StatsReport::kStatsValueNameTransportId,
                   transport_id);
  return report;
}

std::string StatsCollector::AddOneCertificateReport(
    const rtc::SSLCertificate* cert, const std::string& issuer_id) {
  // TODO(bemasc): Move this computation to a helper class that caches these
  // values to reduce CPU use in GetStats.  This will require adding a fast
  // SSLCertificate::Equals() method to detect certificate changes.

  std::string digest_algorithm;
  if (!cert->GetSignatureDigestAlgorithm(&digest_algorithm))
    return std::string();

  rtc::scoped_ptr<rtc::SSLFingerprint> ssl_fingerprint(
      rtc::SSLFingerprint::Create(digest_algorithm, cert));

  // SSLFingerprint::Create can fail if the algorithm returned by
  // SSLCertificate::GetSignatureDigestAlgorithm is not supported by the
  // implementation of SSLCertificate::ComputeDigest.  This currently happens
  // with MD5- and SHA-224-signed certificates when linked to libNSS.
  if (!ssl_fingerprint)
    return std::string();

  std::string fingerprint = ssl_fingerprint->GetRfc4572Fingerprint();

  rtc::Buffer der_buffer;
  cert->ToDER(&der_buffer);
  std::string der_base64;
  rtc::Base64::EncodeFromArray(
      der_buffer.data(), der_buffer.length(), &der_base64);

  StatsReport* report = reports_.ReplaceOrAddNew(
      StatsId(StatsReport::kStatsReportTypeCertificate, fingerprint));
  report->type = StatsReport::kStatsReportTypeCertificate;
  report->timestamp = stats_gathering_started_;
  report->AddValue(StatsReport::kStatsValueNameFingerprint, fingerprint);
  report->AddValue(StatsReport::kStatsValueNameFingerprintAlgorithm,
                   digest_algorithm);
  report->AddValue(StatsReport::kStatsValueNameDer, der_base64);
  if (!issuer_id.empty())
    report->AddValue(StatsReport::kStatsValueNameIssuerId, issuer_id);
  return report->id;
}

std::string StatsCollector::AddCertificateReports(
    const rtc::SSLCertificate* cert) {
  // Produces a chain of StatsReports representing this certificate and the rest
  // of its chain, and adds those reports to |reports_|.  The return value is
  // the id of the leaf report.  The provided cert must be non-null, so at least
  // one report will always be provided and the returned string will never be
  // empty.
  ASSERT(cert != NULL);

  std::string issuer_id;
  rtc::scoped_ptr<rtc::SSLCertChain> chain;
  if (cert->GetChain(chain.accept())) {
    // This loop runs in reverse, i.e. from root to leaf, so that each
    // certificate's issuer's report ID is known before the child certificate's
    // report is generated.  The root certificate does not have an issuer ID
    // value.
    for (ptrdiff_t i = chain->GetSize() - 1; i >= 0; --i) {
      const rtc::SSLCertificate& cert_i = chain->Get(i);
      issuer_id = AddOneCertificateReport(&cert_i, issuer_id);
    }
  }
  // Add the leaf certificate.
  return AddOneCertificateReport(cert, issuer_id);
}

std::string StatsCollector::AddCandidateReport(
    const cricket::Candidate& candidate,
    const std::string& report_type) {
  std::ostringstream ost;
  ost << "Cand-" << candidate.id();
  StatsReport* report = reports_.Find(ost.str());
  if (!report) {
    report = reports_.InsertNew(ost.str());
    DCHECK(StatsReport::kStatsReportTypeIceLocalCandidate == report_type ||
           StatsReport::kStatsReportTypeIceRemoteCandidate == report_type);
    report->type = report_type;
    if (report_type == StatsReport::kStatsReportTypeIceLocalCandidate) {
      report->AddValue(StatsReport::kStatsValueNameCandidateNetworkType,
                       AdapterTypeToStatsType(candidate.network_type()));
    }
    report->timestamp = stats_gathering_started_;
    report->AddValue(StatsReport::kStatsValueNameCandidateIPAddress,
                     candidate.address().ipaddr().ToString());
    report->AddValue(StatsReport::kStatsValueNameCandidatePortNumber,
                     candidate.address().PortAsString());
    report->AddValue(StatsReport::kStatsValueNameCandidatePriority,
                     candidate.priority());
    report->AddValue(StatsReport::kStatsValueNameCandidateType,
                     IceCandidateTypeToStatsType(candidate.type()));
    report->AddValue(StatsReport::kStatsValueNameCandidateTransportType,
                     candidate.protocol());
  }

  return ost.str();
}

void StatsCollector::ExtractSessionInfo() {
  ASSERT(session_->signaling_thread()->IsCurrent());
  // Extract information from the base session.
  StatsReport* report = reports_.ReplaceOrAddNew(
      StatsId(StatsReport::kStatsReportTypeSession, session_->id()));
  report->type = StatsReport::kStatsReportTypeSession;
  report->timestamp = stats_gathering_started_;
  report->values.clear();
  report->AddBoolean(StatsReport::kStatsValueNameInitiator,
                     session_->initiator());

  cricket::SessionStats stats;
  if (session_->GetStats(&stats)) {
    // Store the proxy map away for use in SSRC reporting.
    proxy_to_transport_ = stats.proxy_to_transport;

    for (cricket::TransportStatsMap::iterator transport_iter
             = stats.transport_stats.begin();
         transport_iter != stats.transport_stats.end(); ++transport_iter) {
      // Attempt to get a copy of the certificates from the transport and
      // expose them in stats reports.  All channels in a transport share the
      // same local and remote certificates.
      //
      // Note that Transport::GetIdentity and Transport::GetRemoteCertificate
      // invoke method calls on the worker thread and block this thread, but
      // messages are still processed on this thread, which may blow way the
      // existing transports. So we cannot reuse |transport| after these calls.
      std::string local_cert_report_id, remote_cert_report_id;

      cricket::Transport* transport =
          session_->GetTransport(transport_iter->second.content_name);
      rtc::scoped_ptr<rtc::SSLIdentity> identity;
      if (transport && transport->GetIdentity(identity.accept())) {
        local_cert_report_id =
            AddCertificateReports(&(identity->certificate()));
      }

      transport = session_->GetTransport(transport_iter->second.content_name);
      rtc::scoped_ptr<rtc::SSLCertificate> cert;
      if (transport && transport->GetRemoteCertificate(cert.accept())) {
        remote_cert_report_id = AddCertificateReports(cert.get());
      }

      for (cricket::TransportChannelStatsList::iterator channel_iter
               = transport_iter->second.channel_stats.begin();
           channel_iter != transport_iter->second.channel_stats.end();
           ++channel_iter) {
        std::ostringstream ostc;
        ostc << "Channel-" << transport_iter->second.content_name
             << "-" << channel_iter->component;
        StatsReport* channel_report = reports_.ReplaceOrAddNew(ostc.str());
        channel_report->type = StatsReport::kStatsReportTypeComponent;
        channel_report->timestamp = stats_gathering_started_;
        channel_report->AddValue(StatsReport::kStatsValueNameComponent,
                                 channel_iter->component);
        if (!local_cert_report_id.empty())
          channel_report->AddValue(
              StatsReport::kStatsValueNameLocalCertificateId,
              local_cert_report_id);
        if (!remote_cert_report_id.empty())
          channel_report->AddValue(
              StatsReport::kStatsValueNameRemoteCertificateId,
              remote_cert_report_id);
        for (size_t i = 0;
             i < channel_iter->connection_infos.size();
             ++i) {
          std::ostringstream ost;
          ost << "Conn-" << transport_iter->first << "-"
              << channel_iter->component << "-" << i;
          StatsReport* report = reports_.ReplaceOrAddNew(ost.str());
          report->type = StatsReport::kStatsReportTypeCandidatePair;
          report->timestamp = stats_gathering_started_;
          // Link from connection to its containing channel.
          report->AddValue(StatsReport::kStatsValueNameChannelId,
                           channel_report->id);

          const cricket::ConnectionInfo& info =
              channel_iter->connection_infos[i];
          report->AddValue(StatsReport::kStatsValueNameBytesSent,
                           info.sent_total_bytes);
          report->AddValue(StatsReport::kStatsValueNameSendPacketsDiscarded,
                           info.sent_discarded_packets);
          report->AddValue(StatsReport::kStatsValueNamePacketsSent,
                           info.sent_total_packets);
          report->AddValue(StatsReport::kStatsValueNameBytesReceived,
                           info.recv_total_bytes);
          report->AddBoolean(StatsReport::kStatsValueNameWritable,
                             info.writable);
          report->AddBoolean(StatsReport::kStatsValueNameReadable,
                             info.readable);
          report->AddBoolean(StatsReport::kStatsValueNameActiveConnection,
                             info.best_connection);
          report->AddValue(StatsReport::kStatsValueNameLocalCandidateId,
                           AddCandidateReport(
                               info.local_candidate,
                               StatsReport::kStatsReportTypeIceLocalCandidate));
          report->AddValue(
              StatsReport::kStatsValueNameRemoteCandidateId,
              AddCandidateReport(
                  info.remote_candidate,
                  StatsReport::kStatsReportTypeIceRemoteCandidate));
          report->AddValue(StatsReport::kStatsValueNameLocalAddress,
                           info.local_candidate.address().ToString());
          report->AddValue(StatsReport::kStatsValueNameRemoteAddress,
                           info.remote_candidate.address().ToString());
          report->AddValue(StatsReport::kStatsValueNameRtt, info.rtt);
          report->AddValue(StatsReport::kStatsValueNameTransportType,
                           info.local_candidate.protocol());
          report->AddValue(StatsReport::kStatsValueNameLocalCandidateType,
                           info.local_candidate.type());
          report->AddValue(StatsReport::kStatsValueNameRemoteCandidateType,
                           info.remote_candidate.type());
        }
      }
    }
  }
}

StatsReport* StatsCollector::GetOrCreateReport(const std::string& type,
                                               const std::string& id,
                                               TrackDirection direction) {
  ASSERT(session_->signaling_thread()->IsCurrent());
  ASSERT(type == StatsReport::kStatsReportTypeSsrc ||
         type == StatsReport::kStatsReportTypeRemoteSsrc);
  StatsReport* report = NULL;
  if (report == NULL) {
    std::string statsid = StatsId(type, id, direction);
    report = reports_.FindOrAddNew(statsid);
    ASSERT(report->id == statsid);
    report->type = type;
  }

  return report;
}

void StatsCollector::ClearUpdateStatsCacheForTest() {
  stats_gathering_started_ = 0;
}

}  // namespace webrtc
