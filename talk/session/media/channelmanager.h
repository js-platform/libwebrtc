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

#ifndef TALK_SESSION_MEDIA_CHANNELMANAGER_H_
#define TALK_SESSION_MEDIA_CHANNELMANAGER_H_

#include <string>
#include <vector>

#include "talk/media/base/mediaengine.h"
#include "talk/session/media/channel.h"
#include "webrtc/p2p/base/session.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/thread.h"

namespace cricket {

const int kDefaultAudioDelayOffset = 0;

// ChannelManager allows the MediaEngine to run on a separate thread, and takes
// care of marshalling calls between threads. It also creates and keeps track of
// voice and video channels; by doing so, it can temporarily pause all the
// channels when a new audio or video device is chosen. The voice and video
// channels are stored in separate vectors, to easily allow operations on just
// voice or just video channels.
// ChannelManager also allows the application to discover what devices it has
// using device manager.
class ChannelManager : public rtc::MessageHandler,
                       public sigslot::has_slots<> {
 public:
#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
  // Creates the channel manager, and specifies the worker thread to use.
  explicit ChannelManager(rtc::Thread* worker);
#endif

  // For testing purposes. Allows the media engine and data media
  // engine and dev manager to be mocks.  The ChannelManager takes
  // ownership of these objects.
  ChannelManager(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 rtc::Thread* worker);
  // Same as above, but gives an easier default DataEngine.
  ChannelManager(MediaEngineInterface* me,
                 rtc::Thread* worker);
  ~ChannelManager();

  // Accessors for the worker thread, allowing it to be set after construction,
  // but before Init. set_worker_thread will return false if called after Init.
  rtc::Thread* worker_thread() const { return worker_thread_; }
  bool set_worker_thread(rtc::Thread* thread) {
    if (initialized_) return false;
    worker_thread_ = thread;
    return true;
  }

  // Gets capabilities. Can be called prior to starting the media engine.
  int GetCapabilities();

  // Retrieves the list of supported audio & video codec types.
  // Can be called before starting the media engine.
  void GetSupportedDataCodecs(std::vector<DataCodec>* codecs) const;

  // Indicates whether the media engine is started.
  bool initialized() const { return initialized_; }
  // Starts up the media engine.
  bool Init();
  // Shuts down the media engine.
  void Terminate();

  // The operations below all occur on the worker thread.

  DataChannel* CreateDataChannel(
      BaseSession* session, const std::string& content_name,
      bool rtcp, DataChannelType data_channel_type);
  // Destroys a data channel created with the Create API.
  void DestroyDataChannel(DataChannel* data_channel);

  sigslot::repeater0<> SignalDevicesChange;

 private:
  typedef std::vector<DataChannel*> DataChannels;

  void Construct(MediaEngineInterface* me,
                 DataEngineInterface* dme,
                 rtc::Thread* worker_thread);
  void Terminate_w();
  DataChannel* CreateDataChannel_w(
      BaseSession* session, const std::string& content_name,
      bool rtcp, DataChannelType data_channel_type);
  void DestroyDataChannel_w(DataChannel* data_channel);
  virtual void OnMessage(rtc::Message *message);

  rtc::scoped_ptr<MediaEngineInterface> media_engine_;
  rtc::scoped_ptr<DataEngineInterface> data_media_engine_;
  bool initialized_;
  rtc::Thread* main_thread_;
  rtc::Thread* worker_thread_;

  DataChannels data_channels_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CHANNELMANAGER_H_
