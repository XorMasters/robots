//
//  RtcgwSignalling.h
//  RtcgwRobot
//
//  Created by Allan Knight on 5/30/14.
//  Copyright (c) 2014 Allan Knight. All rights reserved.
//

#ifndef __CITRIX__RTCGWSIGNALLING__
#define __CITRIX__RTCGWSIGNALLING__

#include "citrix/ISignalling.h"
#include <string>
#include <vector>

namespace web { namespace json {
  class value;
  class array;
}}

namespace citrix {

class RtcgwSignalling : public ISignalling {
public:
  RtcgwSignalling(const std::string& intialUri);
  virtual ~RtcgwSignalling() {}
  
public:
  virtual bool Initialize(const std::string& accessCode);
  virtual bool SendOffer(webrtc::SessionDescriptionInterface* offer);
  virtual bool SendAnswer(webrtc::SessionDescriptionInterface* answer);
  virtual bool SendCandidate(const webrtc::IceCandidateInterface* candidate);

public:
  virtual void registerObserver(ISignallingObserver* observer);
  virtual void unregisterObserver(ISignallingObserver* observer);
  
private:
  void _joinAudioSession(const web::json::value& audio);
  void _pollEvents();
  void _processEvents(const web::json::array& events);
  void _processResources(const web::json::value& resources);
  void _processCandidate(const web::json::value& candidate);
  void _processAnswer(const web::json::value& answer);
  
private:
  void _notifyOfferObservers(webrtc::SessionDescriptionInterface* offer);
  void _notifyAnswerObservers(webrtc::SessionDescriptionInterface* answer);
  void _notifyCandidateObservers(webrtc::IceCandidateInterface* candidate);
  void _notifyReadyObservers();

private:
  void _putJson(const web::json::value& json, const std::string& uri);
  void _sendJson(const std::string& mtd, const web::json::value& json, const std::string& uri);
  
protected:
  std::string _serverUri;
  std::string _eventsUri;
  std::string _audioUri;
  std::string _offerUri;
  std::string _candidateUri;
  
  std::vector<ISignallingObserver*> _observers;
  
  static const size_t _apiVersion = 2;
};

} // namespace citrix

#endif // __CITRIX__RTCGWSIGNALLING__
