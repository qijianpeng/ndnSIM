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

#include "ndn-trace-consumer.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <ndn-cxx/lp/tags.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/prettywriter.h> // for stringify JSON
#include <cstdio>
NS_LOG_COMPONENT_DEFINE("ndn.TraceConsumer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(TraceConsumer);

TypeId
TraceConsumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::TraceConsumer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                    MakeIntegerAccessor(&TraceConsumer::m_seq), MakeIntegerChecker<int32_t>())

      .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                    MakeNameAccessor(&TraceConsumer::m_interestName), MakeNameChecker())
      .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("2s"),
                    MakeTimeAccessor(&TraceConsumer::m_interestLifeTime), MakeTimeChecker())

      .AddAttribute("RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue("50ms"),
                    MakeTimeAccessor(&TraceConsumer::GetRetxTimer, &TraceConsumer::SetRetxTimer),
                    MakeTimeChecker())

      .AddTraceSource("LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor(&TraceConsumer::m_lastRetransmittedInterestDataDelay),
                      "ns3::ndn::Consumer::LastRetransmittedInterestDataDelayCallback")

      .AddTraceSource("FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor(&TraceConsumer::m_firstInterestDataDelay),
                      "ns3::ndn::Consumer::FirstInterestDataDelayCallback");

  return tid;
}

void
TraceConsumer::SendPacket()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }

    seq = m_seq++;
  }

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->append("snake");
  nameWithSequence->appendSequenceNumber(seq);
  nameWithSequence->append("/Key-TID" + std::to_string(seq));
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(false);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << seq);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  ScheduleNextPacket();
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////
struct node{
  std::string Id;
  double delay;
  double Sig;
};

std::vector<node>
pars(std::string s, std::vector<node> vect){

  rapidjson::Document document;
  if ( document.Parse<0>( s.c_str() ).HasParseError() ) {
    std::cout << "Parsing error" << std::endl;
  }else{
    node n;
    assert(document.IsObject());
    assert(document["Id"].IsString());
    n.Id=document["Id"].GetString();
    n.delay=document["delay"].GetDouble();
    n.Sig=document["overhead"].GetDouble();
    vect.push_back(n);
    if(document.HasMember("next")){

      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer( sb );
      document["next"].Accept( writer );
      vect = pars(sb.GetString(), vect);
    }else{
      return vect;
    }
  }
  return vect;
}
  float sum(int x, std::vector<node> vect){
    std::vector<node> v = vect;
    float f =0;
    for (int index =1; index <=x; index++ ){
      node n = v[index];
      f = f + n.Sig;
    }
    return f;
  }
void
getPathDetails(std::string data_a){
  std::vector<node> collected;
  collected = pars(data_a, collected);
  if(collected.size()==1){
    std::cout << "Traced object is on this node " << std::endl;
    return;
  }else{
    std::cout << "       Localhost to: " << std::endl;
    std::cout << "                     " << std::endl;
    node a = collected[0];
    for (unsigned int index = 1; index < collected.size(); ++index){
      node  k = collected[index];
      float f = (a.delay-k.delay-sum(index, collected));
      std::string pp = collected[index].Id;
      std::cout << "                 " <<index << "   " << pp << "   " << f << " seconds" << std::endl;
    }
    std::cout << "                     " << std::endl;
    return;
  }
}
void
TraceConsumer::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  Consumer::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  // NS_LOG_INFO ("Received content object: " << boost::cref(*data));

  // This could be a problem......
  uint32_t seq = data->getName().at(-2).toSequenceNumber();
  NS_LOG_INFO("< DATA for " << seq);
  std::string contents = std::string(reinterpret_cast<const char*>(data->getContent().value()), 
                                    data->getContent().value_size());
  NS_LOG_INFO("Details: " << contents);
  getPathDetails(contents);
  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);

  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);
  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(this, seq, Simulator::Now() - entry->time, hopCount);
  }

  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);

  m_rtt->AckSeq(SequenceNumber32(seq));
}


} // namespace ndn
} // namespace ns3
