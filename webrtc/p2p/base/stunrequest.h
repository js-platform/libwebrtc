/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_STUNREQUEST_H_
#define WEBRTC_P2P_BASE_STUNREQUEST_H_

#include <map>
#include <string>
#include "webrtc/p2p/base/stun.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"

namespace cricket {

class StunRequest;

// Manages a set of STUN requests, sending and resending until we receive a
// response or determine that the request has timed out.
class StunRequestManager {
public:
  StunRequestManager(rtc::Thread* thread);
  ~StunRequestManager();

  // Starts sending the given request (perhaps after a delay).
  void Send(StunRequest* request);
  void SendDelayed(StunRequest* request, int delay);

  // Removes a stun request that was added previously.  This will happen
  // automatically when a request succeeds, fails, or times out.
  void Remove(StunRequest* request);

  // Removes all stun requests that were added previously.
  void Clear();

  // Determines whether the given message is a response to one of the
  // outstanding requests, and if so, processes it appropriately.
  bool CheckResponse(StunMessage* msg);
  bool CheckResponse(const char* data, size_t size);

  bool empty() { return requests_.empty(); }

  // Raised when there are bytes to be sent.
  sigslot::signal3<const void*, size_t, StunRequest*> SignalSendPacket;

private:
  typedef std::map<std::string, StunRequest*> RequestMap;

  rtc::Thread* thread_;
  RequestMap requests_;

  friend class StunRequest;
};

// Represents an individual request to be sent.  The STUN message can either be
// constructed beforehand or built on demand.
class StunRequest : public rtc::MessageHandler {
public:
  StunRequest();
  StunRequest(StunMessage* request);
  virtual ~StunRequest();

  // Causes our wrapped StunMessage to be Prepared
  void Construct();

  // The manager handling this request (if it has been scheduled for sending).
  StunRequestManager* manager() { return manager_; }

  // Returns the transaction ID of this request.
  const std::string& id() { return msg_->transaction_id(); }

  // Returns the STUN type of the request message.
  int type();

  // Returns a const pointer to |msg_|.
  const StunMessage* msg() const;

  // Time elapsed since last send (in ms)
  uint32 Elapsed() const;

protected:
  int count_;
  bool timeout_;

  // Fills in a request object to be sent.  Note that request's transaction ID
  // will already be set and cannot be changed.
  virtual void Prepare(StunMessage* request) {}

  // Called when the message receives a response or times out.
  virtual void OnResponse(StunMessage* response) {}
  virtual void OnErrorResponse(StunMessage* response) {}
  virtual void OnTimeout() {}
  virtual int GetNextDelay();

private:
  void set_manager(StunRequestManager* manager);

  // Handles messages for sending and timeout.
  void OnMessage(rtc::Message* pmsg);

  StunRequestManager* manager_;
  StunMessage* msg_;
  uint32 tstamp_;

  friend class StunRequestManager;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_STUNREQUEST_H_
