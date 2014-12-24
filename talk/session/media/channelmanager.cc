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

#include "talk/session/media/channelmanager.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>

#include "talk/media/base/hybriddataengine.h"
#include "talk/media/base/rtpdataengine.h"
#ifdef HAVE_SCTP
#include "talk/media/sctp/sctpdataengine.h"
#endif
#include "talk/session/media/srtpfilter.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace cricket {

enum {
  MSG_VIDEOCAPTURESTATE = 1,
};

using rtc::Bind;

static DataEngineInterface* ConstructDataEngine() {
#ifdef HAVE_SCTP
  return new HybridDataEngine(new RtpDataEngine(), new SctpDataEngine());
#else
  return new RtpDataEngine();
#endif
}

#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
ChannelManager::ChannelManager(rtc::Thread* worker_thread) {
  Construct(MediaEngineFactory::Create(),
            ConstructDataEngine(),
            worker_thread);
}
#endif

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               rtc::Thread* worker_thread) {
  Construct(me, dme, worker_thread);
}

ChannelManager::ChannelManager(MediaEngineInterface* me,
                               rtc::Thread* worker_thread) {
  Construct(me,
            ConstructDataEngine(),
            worker_thread);
}

void ChannelManager::Construct(MediaEngineInterface* me,
                               DataEngineInterface* dme,
                               rtc::Thread* worker_thread) {
  media_engine_.reset(me);
  data_media_engine_.reset(dme);
  initialized_ = false;
  main_thread_ = rtc::Thread::Current();
  worker_thread_ = worker_thread;
}

ChannelManager::~ChannelManager() {
  if (initialized_) {
    Terminate();
    // If srtp is initialized (done by the Channel) then we must call
    // srtp_shutdown to free all crypto kernel lists. But we need to make sure
    // shutdown always called at the end, after channels are destroyed.
    // ChannelManager d'tor is always called last, it's safe place to call
    // shutdown.
    ShutdownSrtp();
  }
}

int ChannelManager::GetCapabilities() {
  return media_engine_->GetCapabilities();
}

void ChannelManager::GetSupportedDataCodecs(
    std::vector<DataCodec>* codecs) const {
  *codecs = data_media_engine_->data_codecs();
}

bool ChannelManager::Init() {
  ASSERT(!initialized_);
  if (initialized_) {
    return false;
  }

  ASSERT(worker_thread_ != NULL);
  if (worker_thread_) {
    if (worker_thread_ != rtc::Thread::Current()) {
      // Do not allow invoking calls to other threads on the worker thread.
      worker_thread_->Invoke<bool>(rtc::Bind(
          &rtc::Thread::SetAllowBlockingCalls, worker_thread_, false));
    }

    if (media_engine_->Init(worker_thread_)) {
      initialized_ = true;
    }
  }
  return initialized_;
}

void ChannelManager::Terminate() {
  ASSERT(initialized_);
  if (!initialized_) {
    return;
  }
  worker_thread_->Invoke<void>(Bind(&ChannelManager::Terminate_w, this));
  media_engine_->Terminate();
  initialized_ = false;
}

void ChannelManager::Terminate_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
}

DataChannel* ChannelManager::CreateDataChannel(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType channel_type) {
  return worker_thread_->Invoke<DataChannel*>(
      Bind(&ChannelManager::CreateDataChannel_w, this, session, content_name,
           rtcp, channel_type));
}

DataChannel* ChannelManager::CreateDataChannel_w(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType data_channel_type) {
  // This is ok to alloc from a thread other than the worker thread.
  ASSERT(initialized_);
  DataMediaChannel* media_channel = data_media_engine_->CreateChannel(
      data_channel_type);
  if (!media_channel) {
    LOG(LS_WARNING) << "Failed to create data channel of type "
                    << data_channel_type;
    return NULL;
  }

  DataChannel* data_channel = new DataChannel(
      worker_thread_, media_channel,
      session, content_name, rtcp);
  if (!data_channel->Init()) {
    LOG(LS_WARNING) << "Failed to init data channel.";
    delete data_channel;
    return NULL;
  }
  data_channels_.push_back(data_channel);
  return data_channel;
}

void ChannelManager::DestroyDataChannel(DataChannel* data_channel) {
  if (data_channel) {
    worker_thread_->Invoke<void>(
        Bind(&ChannelManager::DestroyDataChannel_w, this, data_channel));
  }
}

void ChannelManager::DestroyDataChannel_w(DataChannel* data_channel) {
  // Destroy data channel.
  ASSERT(initialized_);
  DataChannels::iterator it = std::find(data_channels_.begin(),
      data_channels_.end(), data_channel);
  ASSERT(it != data_channels_.end());
  if (it == data_channels_.end())
    return;

  data_channels_.erase(it);
  delete data_channel;
}

void ChannelManager::OnMessage(rtc::Message* message) {

}

}  // namespace cricket
