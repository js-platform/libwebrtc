/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCMEDIAENGINE_H_
#define TALK_MEDIA_WEBRTCMEDIAENGINE_H_

#include "talk/media/base/mediaengine.h"
#include "talk/media/webrtc/webrtcexport.h"

#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)

WRME_EXPORT
cricket::MediaEngineInterface* CreateWebRtcMediaEngine();

WRME_EXPORT
void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine);

#endif  // !defined(LIBPEERCONNECTION_LIB) &&
        // !defined(LIBPEERCONNECTION_IMPLEMENTATION)

namespace cricket {

class WebRtcMediaEngineFactory {
 public:
  static MediaEngineInterface* Create();
};

}  // namespace cricket


#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)

namespace cricket {

// TODO(pthacther): Move this code into webrtcmediaengine.cc once
// Chrome compiles it.  Right now it relies on only the .h file.
class DelegatingWebRtcMediaEngine : public cricket::MediaEngineInterface {
 public:
  DelegatingWebRtcMediaEngine()
      : delegate_(CreateWebRtcMediaEngine()) {
  }
  virtual ~DelegatingWebRtcMediaEngine() {
    DestroyWebRtcMediaEngine(delegate_);
  }
  virtual bool Init(rtc::Thread* worker_thread) OVERRIDE {
    return delegate_->Init(worker_thread);
  }
  virtual void Terminate() OVERRIDE {
    delegate_->Terminate();
  }
  virtual int GetCapabilities() OVERRIDE {
    return delegate_->GetCapabilities();
  }
  virtual VoiceMediaChannel* CreateChannel() OVERRIDE {
    return delegate_->CreateChannel();
  }

 private:
  cricket::MediaEngineInterface* delegate_;
};

// Used by PeerConnectionFactory to create a media engine passed into
// ChannelManager.
MediaEngineInterface* WebRtcMediaEngineFactory::Create() {
  return new cricket::DelegatingWebRtcMediaEngine();
}

}  // namespace cricket

#endif  // !defined(LIBPEERCONNECTION_LIB) &&
        // !defined(LIBPEERCONNECTION_IMPLEMENTATION)

#endif  // TALK_MEDIA_WEBRTCMEDIAENGINE_H_
