/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/app/webrtc/peerconnectionfactory.h"

#include "talk/app/webrtc/peerconnection.h"
#include "talk/app/webrtc/peerconnectionproxy.h"
#include "talk/app/webrtc/portallocatorfactory.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "webrtc/base/bind.h"

using rtc::scoped_refptr;

namespace {

typedef rtc::TypedMessageData<bool> InitMessageData;

struct CreatePeerConnectionParams : public rtc::MessageData {
  CreatePeerConnectionParams(
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration,
      const webrtc::MediaConstraintsInterface* constraints,
      webrtc::PortAllocatorFactoryInterface* allocator_factory,
      webrtc::DTLSIdentityServiceInterface* dtls_identity_service,
      webrtc::PeerConnectionObserver* observer)
      : configuration(configuration),
        constraints(constraints),
        allocator_factory(allocator_factory),
        dtls_identity_service(dtls_identity_service),
        observer(observer) {
  }
  scoped_refptr<webrtc::PeerConnectionInterface> peerconnection;
  const webrtc::PeerConnectionInterface::RTCConfiguration& configuration;
  const webrtc::MediaConstraintsInterface* constraints;
  scoped_refptr<webrtc::PortAllocatorFactoryInterface> allocator_factory;
  webrtc::DTLSIdentityServiceInterface* dtls_identity_service;
  webrtc::PeerConnectionObserver* observer;
};

enum {
  MSG_INIT_FACTORY = 1,
  MSG_TERMINATE_FACTORY,
  MSG_CREATE_PEERCONNECTION,
  MSG_CREATE_AUDIOSOURCE,
  MSG_CREATE_VIDEOSOURCE,
  MSG_START_AEC_DUMP,
};

}  // namespace

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>());

  if (!pc_factory->Initialize()) {
    return NULL;
  }
  return pc_factory;
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread) {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>(worker_thread,
                                                             signaling_thread));
  if (!pc_factory->Initialize()) {
    return NULL;
  }
  return pc_factory;
}

PeerConnectionFactory::PeerConnectionFactory()
    : owns_ptrs_(true),
      signaling_thread_(new rtc::Thread),
      worker_thread_(new rtc::Thread) {
  bool result = signaling_thread_->Start();
  ASSERT(result);
  result = worker_thread_->Start();
  ASSERT(result);
}

PeerConnectionFactory::PeerConnectionFactory(
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread)
    : owns_ptrs_(false),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread) {
  ASSERT(worker_thread != NULL);
  ASSERT(signaling_thread != NULL);
  // TODO: Currently there is no way creating an external adm in
  // libjingle source tree. So we can 't currently assert if this is NULL.
  // ASSERT(default_adm != NULL);
}

PeerConnectionFactory::~PeerConnectionFactory() {
  signaling_thread_->Clear(this);
  signaling_thread_->Send(this, MSG_TERMINATE_FACTORY);
  if (owns_ptrs_) {
    delete signaling_thread_;
    delete worker_thread_;
  }
}

bool PeerConnectionFactory::Initialize() {
  InitMessageData result(false);
  signaling_thread_->Send(this, MSG_INIT_FACTORY, &result);
  return result.data();
}

void PeerConnectionFactory::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_INIT_FACTORY: {
     InitMessageData* pdata = static_cast<InitMessageData*>(msg->pdata);
     pdata->data() = Initialize_s();
     break;
    }
    case MSG_TERMINATE_FACTORY: {
      Terminate_s();
      break;
    }
    case MSG_CREATE_PEERCONNECTION: {
      CreatePeerConnectionParams* pdata =
          static_cast<CreatePeerConnectionParams*> (msg->pdata);
      pdata->peerconnection = CreatePeerConnection_s(
          pdata->configuration,
          pdata->constraints,
          pdata->allocator_factory,
          pdata->dtls_identity_service,
          pdata->observer);
      break;
    }
  }
}

bool PeerConnectionFactory::Initialize_s() {
  rtc::InitRandom(rtc::Time());

  allocator_factory_ = PortAllocatorFactory::Create(worker_thread_);
  if (!allocator_factory_)
    return false;

  cricket::MediaEngineInterface* media_engine(
      cricket::WebRtcMediaEngineFactory::Create());


  channel_manager_.reset(new cricket::ChannelManager(
      media_engine, worker_thread_));
  if (!channel_manager_->Init()) {
    return false;
  }
  return true;
}

// Terminate what we created on the signaling thread.
void PeerConnectionFactory::Terminate_s() {
  channel_manager_.reset(NULL);
  allocator_factory_ = NULL;
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    DTLSIdentityServiceInterface* dtls_identity_service,
    PeerConnectionObserver* observer) {
  CreatePeerConnectionParams params(configuration, constraints,
                                    allocator_factory, dtls_identity_service,
                                    observer);
  signaling_thread_->Send(
      this, MSG_CREATE_PEERCONNECTION, &params);
  return params.peerconnection;
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection_s(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    const MediaConstraintsInterface* constraints,
    PortAllocatorFactoryInterface* allocator_factory,
    DTLSIdentityServiceInterface* dtls_identity_service,
    PeerConnectionObserver* observer) {
  ASSERT(allocator_factory || allocator_factory_);
  rtc::scoped_refptr<PeerConnection> pc(
      new rtc::RefCountedObject<PeerConnection>(this));
  if (!pc->Initialize(
      configuration,
      constraints,
      allocator_factory ? allocator_factory : allocator_factory_.get(),
      dtls_identity_service,
      observer)) {
    return NULL;
  }
  return PeerConnectionProxy::Create(signaling_thread(), pc);
}

cricket::ChannelManager* PeerConnectionFactory::channel_manager() {
  return channel_manager_.get();
}

rtc::Thread* PeerConnectionFactory::signaling_thread() {
  return signaling_thread_;
}

rtc::Thread* PeerConnectionFactory::worker_thread() {
  return worker_thread_;
}

}  // namespace webrtc
