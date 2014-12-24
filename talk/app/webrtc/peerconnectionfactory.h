/*
 * libjingle
 * Copyright 2011, Google Inc.
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
#ifndef TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/session/media/channelmanager.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"

namespace webrtc {

class PeerConnectionFactory : public PeerConnectionFactoryInterface,
                              public rtc::MessageHandler {
 public:
  virtual void SetOptions(const Options& options) {
    options_ = options;
  }

  virtual rtc::scoped_refptr<PeerConnectionInterface>
      CreatePeerConnection(
          const PeerConnectionInterface::RTCConfiguration& configuration,
          const MediaConstraintsInterface* constraints,
          PortAllocatorFactoryInterface* allocator_factory,
          DTLSIdentityServiceInterface* dtls_identity_service,
          PeerConnectionObserver* observer);

  bool Initialize();

  virtual cricket::ChannelManager* channel_manager();
  virtual rtc::Thread* signaling_thread();
  virtual rtc::Thread* worker_thread();
  const Options& options() const { return options_; }

 protected:
  PeerConnectionFactory();
  PeerConnectionFactory(
      rtc::Thread* worker_thread,
      rtc::Thread* signaling_thread);
  virtual ~PeerConnectionFactory();

 private:
  bool Initialize_s();
  void Terminate_s();

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection_s(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      PortAllocatorFactoryInterface* allocator_factory,
      DTLSIdentityServiceInterface* dtls_identity_service,
      PeerConnectionObserver* observer);

  bool StartAecDump_s(rtc::PlatformFile file);

  // Implements rtc::MessageHandler.
  void OnMessage(rtc::Message* msg);

  bool owns_ptrs_;
  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  Options options_;
  rtc::scoped_refptr<PortAllocatorFactoryInterface> allocator_factory_;
  rtc::scoped_ptr<cricket::ChannelManager> channel_manager_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONFACTORY_H_
