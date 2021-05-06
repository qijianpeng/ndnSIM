/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_BOLT_H
#define NDN_BOLT_H

// #include "ns3/ndnSIM/model/ndn-common.hpp"
#include <ns3/random-variable-stream.h>
#include "ns3/object.h"

#include "ndn-cxx/name.hpp"
#include <ndn-cxx/mgmt/nfd/controller.hpp>
#include <ndn-cxx/mgmt/nfd/fib-entry.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include "ndn-cxx/util/logger.hpp"
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/snake-utils.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/snake/fm/snake-metadata.hpp>
#include "request-container.hpp"



#include <map>
#include <chrono>
#include <ctime>
#include <stdio.h>
#include <boost/thread/thread.hpp>
#include <cstdio>
#include <set>
#include <random>
#include <limits.h>

namespace ns3 {

namespace ndn {
using namespace std::chrono;
using ::ndn::nfd::Controller;
using namespace rapidjson;
using namespace ::ndn;
using namespace ::ndn::nfd;

typedef std::chrono::high_resolution_clock Clock;
typedef ndn::snake::SnakeMetadata DataWrapper;
enum FORWARDING_ACTION:uint8_t{
  TURN_BACK_ACTION = 0,
  GO_FORWARD_ACTION = 1,
};
/**
 * \ingroup ns3
 * \defgroup ndn-snake-apps NDN applications
 */
/**
 * @ingroup ndn-snake-apps
 * @brief Base class that all Snake bolt NDN applications should be derived from.
 */
class Bolt: public Object{
public:
  void
  runp()
  {
    a_face.processEvents();
  }
  /**
   * @brief Default constructor
   */
  Bolt(KeyChain& keyChain): a_face(nullptr, keyChain), m_keyChain(keyChain), 
                               m_scheduler(a_face.getIoService()){
    a_face.setInterestFilter("/Trace",
                             bind(&Bolt::onInterest, this, _1, _2),
                             RegisterPrefixSuccessCallback(), // RegisterPrefixSuccessCallback is optional
                             bind(&Bolt::onRegisterFailed, this, _1, _2));
  }
  ~Bolt(){}

public:
  double mean =0;
  std::string nodeId;
private:
  Ptr<UniformRandomVariable> intRandom = CreateObject<UniformRandomVariable> ();
  
	int
	number(){
    return intRandom->GetInteger(0, INT_MAX);
  }
  
	std::string
  getcomp(const Interest& interest, int i){
    return interest.getName().at(-i).toUri();;
  }

  void
  onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
        << prefix << "\" in local hub's daemon (" << reason << ")"
        << std::endl;
    a_face.shutdown();
  }
  void
  clean(uint32_t key)
  {
    auto it = loop.find(key);
    loop.erase(it);
    auto itr = Reqs.find(key);
    Reqs.erase(itr);
  }

    /// loop check
  bool
  Islooped(const Interest& interest){
    bool verdict = false;
    // int keyIndex = interest.hasApplicationParameters()? -2 : -1;
    // std::string keyTid = interest.getName().at(keyIndex).toUri();

    if(loop.find(interest.getNonce()) != loop.end()){
      verdict = true;
    } else {
      loop.insert(interest.getNonce());
    }
    return verdict;
  }

protected:
  void
  preProcessingData(const Interest& interest, const Data& inData, shared_ptr<Data> outData)
	{
    Quest* quest =  & Reqs[interest.getNonce()];

    Name dataName(quest->Oname);
    outData->wireDecode(inData.wireEncode());
    outData->setName(dataName);
    
    // outData->setFreshnessPeriod(inData.getFreshnessPeriod()); // to be adjusted to the parameter in the user
    ::ndn::snake::util::cloneTags(inData, *outData);
  }
  void virtual
  turnbackAfterProcessingData(const Interest& interest, const Data &inData, shared_ptr<Data> outData){

  }
	void
	afterProcessingData(const Interest& interest, const Data &inData, shared_ptr<Data> outData){
    //Checking logic
    Quest* quest = & Reqs[interest.getNonce()];
    std::string outDataName = outData->getName().toUri();
    BOOST_ASSERT(outDataName == quest->Oname);
    m_keyChain.sign(*outData);
    uint64_t sendTime = 0;
    auto processingTimeTag = outData->getTag<lp::ProcessingTimeTag>();
    if(nullptr != processingTimeTag){
      sendTime = *processingTimeTag;
      outData->removeTag<lp::ProcessingTimeTag>();
    }
    clean(interest.getNonce());
    m_scheduler.schedule(ndn::time::milliseconds(sendTime), [outData, this] {
            a_face.put(*outData);
          });
  }
  void turnBackPreprocessingInterest(const Interest& currInterest, Interest& outInterest){
    // Quest r = Reqs[currInterest.getNonce()];
    // Quest out;
    // out.Oquest = &r;
    // Reqs[outInterest.getNonce()] = out;

    int k = (-1)*(currInterest.getName().size());
    int i;
    Reqs[currInterest.getNonce()].p1 = currInterest.getName().at(k+1).toUri();
    Reqs[currInterest.getNonce()].p2 = currInterest.getName().at(k+2).toUri();

    const ::ndn::Name c = currInterest.getName().at(k+3).toUri();
    const ::ndn::Name nx = c.toUri() ;
    ::ndn::Name v = nx.toUri();
    // Getting the name to lookup
    int lastBound = currInterest.hasApplicationParameters()? -2 : -1;
    for(i=k+4; i< lastBound; i++){
      v = v.toUri() + "/" + currInterest.getName().at(i).toUri();  // The name to lookup
    }
    Reqs[currInterest.getNonce()].Lname = v.toUri();
    ::ndn::Name n = "/Trace/S/"+Reqs[currInterest.getNonce()].p2+ v.toUri()+ "/Key-TID" + std::to_string(number()) ;
    outInterest.setName(n);
    outInterest.setNonce(currInterest.getNonce());
    outInterest.setCanBePrefix(currInterest.getCanBePrefix());
    outInterest.setInterestLifetime(currInterest.getInterestLifetime());
    outInterest.setLongLived(false);
    ::ndn::snake::util::cloneTags(currInterest, outInterest);
  }

  void 
	preProcessingInterest(const Interest& inInterest, 
                                 Interest& outInterest){
    struct Quest r;
    r.First_Arr = steady_clock::now();    // Record arrival time of this request
    r.Oname = inInterest.getName().toUri(); // Record original name of this request
    Reqs[inInterest.getNonce()]=r;

    int k = (-1)*(inInterest.getName().size());
    int i;
    Reqs[inInterest.getNonce()].p1 = inInterest.getName().at(k+1).toUri();
    Reqs[inInterest.getNonce()].p2 = inInterest.getName().at(k+2).toUri();

    const ::ndn::Name c = inInterest.getName().at(k+3).toUri();
    const ::ndn::Name nx = c.toUri() ;
    ::ndn::Name v = nx.toUri();
    // Getting the name to lookup
    int lastBound = inInterest.hasApplicationParameters()? -2 : -1;
    for(i=k+4; i< lastBound; i++){
      v = v.toUri() + "/" + inInterest.getName().at(i).toUri();  // The name to lookup
    }
    Reqs[inInterest.getNonce()].Lname = v.toUri();
    ::ndn::Name n = "/Trace/S/"+Reqs[inInterest.getNonce()].p2+ v.toUri()+ "/Key-TID" + std::to_string(number()) ;
    outInterest.setName(n);
    outInterest.setNonce(inInterest.getNonce());
    outInterest.setCanBePrefix(inInterest.getCanBePrefix());
    outInterest.setInterestLifetime(inInterest.getInterestLifetime());
    outInterest.setLongLived(inInterest.isLongLived());
    ::ndn::snake::util::cloneTags(inInterest, outInterest);
  }
  void 
	afterProcessingInterest(const Interest& inInterest, 
                                 Interest& outInterest){
    // check logic
      Quest* quest =  & Reqs[inInterest.getNonce()];
      quest->Express_time = steady_clock::now();
      // Reqs[inInterest.getNonce()].LFname = outInterest.getName().toUri();
      m_scheduler.schedule(ndn::time::seconds(0), [outInterest, this] {
                a_face.expressInterest(outInterest,
                                       bind(&Bolt::onData, this,  _1, _2),
                                       bind(&Bolt::onNack, this, _1, _2),
                                       bind(&Bolt::onTimeout, this, _1));
      					});
  }

protected:
  void
  onInterest(const InterestFilter& filter, const Interest& interest)
  {
    if (Islooped(interest)){
      return;
    }

		ndn::Interest outInterest;
		preProcessingInterest(interest, outInterest);
		processingInterest(filter, interest, outInterest);
		afterProcessingInterest(interest, outInterest);
		return;
  }
 /** \brief processing the incoming \c inInterest.
   * 
   */
  virtual void processingInterest(const InterestFilter& filter, const Interest& inInterest, 
                                 Interest& outInterest)
  {
    //do nothing
		// can set application parameters
  }
  void
  onData(const Interest& interest, const Data& inData)
  {
		shared_ptr<Data> outData = make_shared<Data>();
		preProcessingData(interest, inData, outData);
		FORWARDING_ACTION action = processingData(interest, Reqs[interest.getNonce()], inData, outData);
    switch(action){
        case FORWARDING_ACTION::GO_FORWARD_ACTION: {
		      afterProcessingData(interest, inData, outData);
          break;
        }
        case FORWARDING_ACTION::TURN_BACK_ACTION:  {
          turnbackAfterProcessingData(interest, inData, outData);
          break;
        }
    }
  }

  /** \brief processing the \c outData according to the \c inData
   * \c Name of the \c outData should not be changed, otherwise an error will occur.
   * \c interest the interest binding with \c inData.
   * \param inData incoming data.
   * \param outData outcoming data.
   */
  virtual FORWARDING_ACTION processingData(const Interest& interest, Quest& quest, const Data &inData, shared_ptr<Data> outData)
	{
		//default: assign 1 KiB virtual payload.
    outData->setContent(make_shared< ::ndn::Buffer>(1024));
    return FORWARDING_ACTION::GO_FORWARD_ACTION;
  }

  void
  onNack(const Interest& interest, const lp::Nack& nack)
  {
		if ((nack.getReason()== lp::NackReason::PRODUCER_LOCAL)||(nack.getReason()== lp::NackReason::CACHE_LOCAL)){ // expected           nack could alsp be No route / prohibited / not supported ...
			Reqs[interest.getNonce()].Rep_time = steady_clock::now();
			nackprocessing(interest);
		}else{
			std::cout << "Other non supported Nacks for now" << interest << std::endl;
			return;
		}
  }

  void
  nackprocessing(const Interest& interest){
    // delay time to get a nack (<>>strategy)
    steady_clock::duration time_span = Reqs[interest.getNonce()].Rep_time-Reqs[interest.getNonce()].Express_time;
    double elapsedn_secs = double(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
    // First overhead time
    steady_clock::duration time_span2 = Reqs[interest.getNonce()].Express_time-Reqs[interest.getNonce()].First_Arr;
    double extra = double(time_span2.count()) * steady_clock::period::num / steady_clock::period::den;
    // Create new name, based on Interest's name
    Name dataName(Reqs[interest.getNonce()].Oname);
    double d = mean+extra;
    std::string idsp = createNack(interest, nodeId, elapsedn_secs, d);
    // Create Data packet
    shared_ptr<Data> data = make_shared<Data>();
    data->setName(dataName);
    data->setFreshnessPeriod(time::seconds(0));
    data->setContent(reinterpret_cast<const uint8_t*>(idsp.c_str()), idsp.size());
    clean(interest.getNonce());
    m_keyChain.sign(*data);
    a_face.put(*data);
  }

  void createmulti(std::map<std::string, std::string> m, const Interest& interest){

    shared_ptr<Data> data = make_shared<Data>();
    data->setName(Reqs[interest.getNonce()].Oname);
    data->setFreshnessPeriod(time::seconds(0));
    std::string temp = format(m);
    data->setContent(reinterpret_cast<const uint8_t*>(temp.c_str()), temp.size());
    m_keyChain.sign(*data);
    a_face.put(*data);
  }
  
  std::string format(std::map<std::string, std::string> m){
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Value array(rapidjson::kArrayType);
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
    // here
    for (std::map<std::string,std::string>::iterator it=m.begin(); it!=m.end(); ++it){
      Value v;
      Value vid;
      Value vdelay;
      Value vover;

      // Split the string
      std::size_t found = it->second.find(",");
      std::string tmpstr = it->second.substr (0, found);
      std::size_t found2 = tmpstr.find (":");
      vid.SetString (tmpstr.substr (found2+2, tmpstr.size () - found2 - 3).c_str (), allocator);

      std::size_t found3 = it->second.find (",",found + 1);
      tmpstr = it->second.substr (found + 1, found3 - found);
      found2 = tmpstr.find (":");
      vdelay.SetDouble (std::atof ((tmpstr.substr (found2+1)).c_str ()));

      std::size_t found4 = it->second.find (",",found3 + 1);
      tmpstr = it->second.substr (found3 + 1, found4 - found3);
      found2 = tmpstr.find (":");
      vover.SetDouble (std::atof ((tmpstr.substr (found2+1)).c_str ()));


      v.SetObject ().AddMember ("Id", vid, allocator).AddMember ("delay", vdelay, allocator).AddMember ("overhead", vover, allocator);
      array.PushBack (v, allocator);

    }
    document.AddMember("next", array, document.GetAllocator());
    StringBuffer strbuf;
    Writer<StringBuffer> writer(strbuf);
    document.Accept(writer);
    std::string temp = strbuf.GetString();
    //return (reinterpret_cast<const uint8_t*>(temp.c_str()), temp.size());
    return temp;
  }

  void
  onTimeout(const Interest& interest)
  {
    std::cout << "Timeout " << interest << std::endl;
  }


  std::string createNack(const Interest& interest, std::string id, double delay, double over)
  {
    //std::cout << " Created nack " << strbuf.GetString() << '\n';
    return id + "'s nack.";
  }

  void
  query(Controller& controller, const ndn::Name& name, uint32_t nonce, const ::ndn::nfd::CommandOptions& options = {})
  {
    parameters param;
    param.nonce = nonce;
    param.name = name;

    controller.fetch<::ndn::nfd::FibDataset>([this, param] (const std::vector<::ndn::nfd::FibEntry>& result) {
      for (size_t prefixLen = param.name.size(); prefixLen > 0; --prefixLen) {
        for (const ::ndn::nfd::FibEntry& item : result) {
          if (item.getPrefix().size() == prefixLen && item.getPrefix().isPrefixOf(param.name)) {
            Reqs[param.nonce].mhops.clear();
            for (const ::ndn::nfd::NextHopRecord& nh : item.getNextHopRecords()) {
              Reqs[param.nonce].mhops.push_back(nh.getFaceId());
            };
          }

        }
      }
      std::string aa;
      // Create N corresponding interests
      for(unsigned int i=0; i<Reqs[param.nonce].mhops.size(); i++){
        ::ndn::Interest I;
        aa = "Key-TID" + std::to_string(number());
        ::ndn::Name n = "/Trace/M/"+ Reqs[param.nonce].p2 + Reqs[param.nonce].Lname+ "/" + aa + "/" + std::to_string(Reqs[param.nonce].mhops[i]) ;  // attaching the face id as x
        I.setName(n);
        I.setNonce(param.nonce);
        I.setInterestLifetime(::ndn::time::milliseconds(10000));
        I.setMustBeFresh(true);
        Reqs[param.nonce].m_exptime[getcomp(I, 2)]=steady_clock::now();
        a_face.expressInterest(I,
            bind(&Bolt::onData, this,  _1, _2),
            bind(&Bolt::onNack, this, _1, _2),
            bind(&Bolt::onTimeout, this, _1));
      }
    },

    bind([]{ std::cout << "failure\n";}), options);
  }
  const Quest getQuest(uint32_t nonce)
  {
    return Reqs[nonce];
  }
private:
  struct parameters{
    ndn::Name name;
    uint32_t nonce;
  };
  // map to match a request (nonce) with an object of struct
  std::map<uint32_t, Quest> Reqs;
  ::ndn::Face a_face;
  ::ndn::KeyChain& m_keyChain;
  std::set<uint32_t> loop;
  ::ndn::Scheduler m_scheduler;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_DATA_BOLT_H
