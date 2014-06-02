//
//  RtcgwSignalling.cpp
//  RtcgwRobot
//
//  Created by Allan Knight on 5/30/14.
//  Copyright (c) 2014 Allan Knight. All rights reserved.
//

#include "RtcgwSignalling.h"

#include "talk/app/webrtc/jsep.h"

#include "cpprest/http_client.h"

using web::http::uri_builder;
using web::http::uri;
using web::http::client::http_client;
using web::http::http_response;
using web::http::http_request;
using web::http::methods;
using web::http::status_codes;
using web::http::http_exception;

using webrtc::SessionDescriptionInterface;
using webrtc::IceCandidateInterface;

using web::json::value;

using std::string;

namespace citrix {

RtcgwSignalling::RtcgwSignalling(const std::string& initialUri) :
  _serverUri(initialUri) {
}

bool RtcgwSignalling::Initialize(const std::string& accessCode) {
  uri_builder initBuilder;
  initBuilder.append_path("api")
             .append_path(std::to_string(_apiVersion))
             .append_path("meetings")
             .append_path(accessCode);
  
  http_client client(_serverUri);
  auto request = client.request(methods::GET, initBuilder.path());
  
  request.then([=](http_response response){
    if(response.status_code() != web::http::status_codes::OK) {
      std::cerr << "Unable to intialize signalling for RTCGW: "
                << response.status_code()
                << std::endl;
      return;
    }
    
    response.extract_json().then([=](value body){
      if(!body.has_field("audio")) {
        std::cerr << "Initializtion response has no audio value" << std::endl;
        return;
      }
      
      value audio = body["audio"];
      if(!audio.has_field("connectionParameters")) {
        std::cerr << "Intialization response has no connection parameters."
                     " Maybe the meeting is not started."
                  << std::endl;
        return;
      }
      
      std::cout << "Joining audio session" << std::endl;
      _joinAudioSession(audio);
    });
    
  });
;
  
  return true;
}

bool RtcgwSignalling::SendOffer(SessionDescriptionInterface* offer) {
  value jsonOffer;
  
  std::string sdpStr;
  offer->ToString(&sdpStr);
  
  jsonOffer["type"] = value::string(offer->type());
  jsonOffer["sdp"]  = value::string(sdpStr);
  
  _putJson(jsonOffer, _offerUri);
  
  return true;
}

bool RtcgwSignalling::SendAnswer(SessionDescriptionInterface* answer) {
  value jsonAnswer;
  
  std::string sdpStr;
  answer->ToString(&sdpStr);
  
  jsonAnswer["type"] = value::string(answer->type());
  jsonAnswer["sdp"]  = value::string(sdpStr);
  
  _putJson(jsonAnswer, _offerUri);

  return true;
}

bool RtcgwSignalling::SendCandidate(const IceCandidateInterface* candidate) {
  value jsonCandidate;
  
  string sdpStr;
  candidate->ToString(&sdpStr);
  
  jsonCandidate["sdpMid"]        = value::string(candidate->sdp_mid());
  jsonCandidate["sdpMLineIndex"] = value::number(candidate->sdp_mline_index());
  jsonCandidate["sdp"]           = value::string(sdpStr);
  jsonCandidate["type"]          = value::string("candidate");
  
  _putJson(jsonCandidate, _candidateUri);
  
  return true;
}

void RtcgwSignalling::registerObserver(ISignallingObserver* observer) {
  if(observer == NULL) {
		std::cerr << "Registering NULL Observer" << std::endl;
		return;
	}

	auto found = std::find(_observers.begin(), _observers.end(), observer);
	if(found != _observers.end()) {
		std::cerr << "Reregistering Observer that has already been registered"
              << std::endl;
		return;
	}

	_observers.push_back(observer);
}

void RtcgwSignalling::unregisterObserver(ISignallingObserver* observer) {
  if(observer == NULL) {
		std::cerr << "Unregistering NULL observer" << std::endl;
		return;
	}

	_observers.erase(std::remove(_observers.begin(), _observers.end(), observer));
}


void RtcgwSignalling::_joinAudioSession(const value &audio) {
  uri_builder joinBuilder;
  joinBuilder.append_path("api")
             .append_path(std::to_string(_apiVersion))
             .append_path("audio");
  
  http_client client(_serverUri);
  auto request = client.request(methods::POST, joinBuilder.path(), audio);
  
  request.then([=](http_response response){
    if(response.status_code() != web::http::status_codes::Created) {
      std::cerr << "Unable to join audio session: "
                << response.status_code()
                << std::endl;
      return;
    }
    
    response.extract_json().then([=](value body){
      if(!body.has_field("status") && !body.has_field("resources")) {
        std::cerr << "Missing fields in audio response" << std::endl;
        return;
      }
      
      value status = body["status"];
      if(status.as_string() != "success") {
        std::cerr << "Audio session not successfully joined: "
                  << status.as_string()
                  << std::endl;
        return;
      }
      
      value resources = body["resources"];
      if(!resources.has_field("events")){
        std::cerr << "Events URI not found in response." << std::endl;
        return;
      }
      
      _eventsUri = resources["events"].as_string();
      _pollEvents();
    });
  });
}

void RtcgwSignalling::_pollEvents() {
  uri_builder eventsBuilder(_eventsUri);

  http_client client("http://" + eventsBuilder.host());
  auto request = client.request(methods::GET, eventsBuilder.path());
  
  request.then([=](http_response response) {
    if(response.status_code() != status_codes::OK) {
      std::cerr << "Error while retrieving latest events: "
                << response.status_code()
                << std::endl;
      return;
    }
    
    response.extract_json().then([=](value body) {
      if(body.has_field("events") && body["events"].is_array()) {
        _processEvents(body["events"].as_array());
      }
      
      this->_pollEvents();
    });
  });
}

void RtcgwSignalling::_processEvents(const web::json::array &events) {
  for(auto ev = events.begin(); ev != events.end(); ev++) {
    value type  = (*ev).at("type");
    value value = (*ev).at("value");
    
    std::cout << type.as_string() << std::endl;
    std::cout << value.serialize() << std::endl;
    
    string typeStr = type.as_string();
    
    if(typeStr == "mediaAvailable") {
      _processResources(value["resources"]);
      _notifyReadyObservers();
    } else if(typeStr == "webRTCCandidate") {
      _processCandidate(value);
    } else if(typeStr == "webRTCAnswer") {
      _processAnswer(value);
    }
  }
}

void RtcgwSignalling::_processResources(const value &resources) {
  if(resources.has_field("webRtcCandidate")) {
    _candidateUri = resources.at("webRtcCandidate").as_string();
    std::cout << "webRtcCandidate: " << _candidateUri << std::endl;
  }
  
  if(resources.has_field("webRtcOffer")) {
    _offerUri = resources.at("webRtcOffer").as_string();
    std::cout << "webRtcOffer: " << _offerUri << std::endl;
  }
}

void RtcgwSignalling::_notifyOfferObservers(SessionDescriptionInterface* offer) {
  for(auto i = _observers.begin(); i != _observers.end(); i++) {
    (*i)->OnReceivedOffer(offer);
  }
}

void RtcgwSignalling::_notifyAnswerObservers(SessionDescriptionInterface* answer) {
  for(auto i = _observers.begin(); i != _observers.end(); i++) {
    (*i)->OnReceivedAnswer(answer);
  }
}

void RtcgwSignalling::_notifyCandidateObservers(IceCandidateInterface* candidate) {
  for(auto i = _observers.begin(); i != _observers.end(); i++) {
    (*i)->OnReceivedCandidate(candidate);
  }
}

void RtcgwSignalling::_notifyReadyObservers() {
  for(auto i = _observers.begin(); i != _observers.end(); i++) {
    (*i)->OnReady();
  }
}

void RtcgwSignalling::_putJson(const value &json, const std::string &uri) {
  _sendJson(methods::PUT, json, uri);
}

void RtcgwSignalling::_sendJson(const string &mtd, const value &json,
                                const string &uri) {
  
  uri_builder builder(uri);
  
  http_client client("http://" + builder.host());
  auto request = client.request(mtd, builder.path(), json);
  
  request.then([=](http_response response){
    if(response.status_code() != status_codes::OK &&
       response.status_code() != status_codes::Created) {
      std::cerr << "Error while sending JSON to server: "
                << response.status_code()
                << std::endl;
      return;
    }
    
    response.extract_json().then([=](value body) {
      if(!body.has_field("status") || body["status"].as_string() == "failed") {
        std::cerr << "JSON object not sent successfully!" << std::endl;
      }
    });
  });
}

void RtcgwSignalling::_processAnswer(const web::json::value &answer) {
  if(!answer.has_field("sdp")) {
    std::cerr << "No sdp field in Answer" << std::endl;
    return;
  }
  
  std::string sdp = answer.at("sdp").as_string();
  
  SessionDescriptionInterface* desc = webrtc::CreateSessionDescription("answer", sdp);
  _notifyAnswerObservers(desc);
}

void RtcgwSignalling::_processCandidate(const web::json::value &candidate) {
  if(!candidate.has_field("candidate")     &&
     !candidate.has_field("sdpMLineIndex") &&
     !candidate.has_field("sdpMid")) {
     std::cerr << "Malformed candidate object" << std::endl;
     return;
  }
  
  std::string sdp      = candidate.at("candidate").as_string();
  std::string sdpMid   = candidate.at("sdpMid").as_string();
  uint32 sdpMLineIndex = candidate.at("sdpMLineIndex").as_number().to_uint32();
  
  IceCandidateInterface* c = webrtc::CreateIceCandidate(sdpMid, sdpMLineIndex, sdp);
  _notifyCandidateObservers(c);
}

} // namespace citrix