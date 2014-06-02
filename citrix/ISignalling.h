//
//  ISignalling.h
//  RtcgwRobot
//
//  Created by Allan Knight on 5/30/14.
//  Copyright (c) 2014 Allan Knight. All rights reserved.
//

#ifndef __CITRIX_ISIGNALLING_H__
#define __CITRIX_ISIGNALLING_H__

#include <string>

namespace webrtc {
  class SessionDescriptionInterface;
  class IceCandidateInterface;
}

namespace citrix {

class ISignallingObserver {
public:
  virtual ~ISignallingObserver() {}
  
public:
  virtual void OnReady() = 0;
  virtual void OnReceivedOffer(webrtc::SessionDescriptionInterface* offer) = 0;
  virtual void OnReceivedAnswer(webrtc::SessionDescriptionInterface* answer) = 0;
  virtual void OnReceivedCandidate(webrtc::IceCandidateInterface* candidate) = 0;
};

class ISignalling {
public:
  virtual ~ISignalling() {}
  
public:
  virtual bool Initialize(const std::string& accessCode) = 0;
  virtual bool SendOffer(webrtc::SessionDescriptionInterface* offer) = 0;
  virtual bool SendAnswer(webrtc::SessionDescriptionInterface* answer) = 0;
  virtual bool SendCandidate(const webrtc::IceCandidateInterface* candidate) = 0;
  
public:
  virtual void registerObserver(ISignallingObserver* observer) = 0;
  virtual void unregisterObserver(ISignallingObserver* observer) = 0;
};

} // namespace citrix

#endif // __CITRIX_ISIGNALLING_H__
