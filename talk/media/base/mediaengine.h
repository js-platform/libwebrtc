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

#ifndef TALK_MEDIA_BASE_MEDIAENGINE_H_
#define TALK_MEDIA_BASE_MEDIAENGINE_H_

#include <limits.h>

#include <string>
#include <vector>

#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediacommon.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/thread.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(CHROMIUM_BUILD)
#define DISABLE_MEDIA_ENGINE_FACTORY
#endif

namespace cricket {

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  // Default value to be used for SetAudioDelayOffset().
  static const int kDefaultAudioDelayOffset;

  virtual ~MediaEngineInterface() {}

  // Initialization
  // Starts the engine.
  virtual bool Init(rtc::Thread* worker_thread) = 0;
  // Shuts down the engine.
  virtual void Terminate() = 0;
  // Returns what the engine is capable of, as a set of Capabilities, above.
  virtual int GetCapabilities() = 0;
};


#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)
class MediaEngineFactory {
 public:
  typedef cricket::MediaEngineInterface* (*MediaEngineCreateFunction)();
  // Creates a media engine, using either the compiled system default or the
  // creation function specified in SetCreateFunction, if specified.
  static MediaEngineInterface* Create();
  // Sets the function used when calling Create. If unset, the compiled system
  // default will be used. Returns the old create function, or NULL if one
  // wasn't set. Likewise, NULL can be used as the |function| parameter to
  // reset to the default behavior.
  static MediaEngineCreateFunction SetCreateFunction(
      MediaEngineCreateFunction function);
 private:
  static MediaEngineCreateFunction create_function_;
};
#endif

enum DataChannelType {
  DCT_NONE = 0,
  DCT_RTP = 1,
  DCT_SCTP = 2
};

class DataEngineInterface {
 public:
  virtual ~DataEngineInterface() {}
  virtual DataMediaChannel* CreateChannel(DataChannelType type) = 0;
  virtual const std::vector<DataCodec>& data_codecs() = 0;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIAENGINE_H_
