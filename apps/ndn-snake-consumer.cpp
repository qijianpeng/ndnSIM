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

#include "ndn-snake-consumer.hpp"
#include "ndn-cxx/name-component.hpp"

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

#include <ndn-cxx/util/snake-utils.hpp>
#include "snake-parameters.hpp"
#include <ndn-cxx/snake/optimize-strategy.hpp>
NS_LOG_COMPONENT_DEFINE("ndn.SnakeConsumer");

namespace ns3 {
namespace ndn {
static ns3::GlobalValue g_O = ns3::GlobalValue ("O",
                                                     "Time complexity.",
                                                     ns3::StringValue("n"),
                                                     ns3::MakeStringChecker());
namespace snake_util = ::ndn::snake::util;
NS_OBJECT_ENSURE_REGISTERED(SnakeConsumer);

TypeId
SnakeConsumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::SnakeConsumer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                    MakeIntegerAccessor(&SnakeConsumer::m_seq), MakeIntegerChecker<int32_t>())

      .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                    MakeNameAccessor(&SnakeConsumer::m_interestName), MakeNameChecker())
      .AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("1000s"),
                    MakeTimeAccessor(&SnakeConsumer::m_interestLifeTime), MakeTimeChecker())

      .AddAttribute("RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue("50ms"),
                    MakeTimeAccessor(&SnakeConsumer::GetRetxTimer, &SnakeConsumer::SetRetxTimer),
                    MakeTimeChecker())

      .AddTraceSource("LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor(&SnakeConsumer::m_lastRetransmittedInterestDataDelay),
                      "ns3::ndn::SnakeConsumer::LastRetransmittedInterestDataDelayCallback")

      .AddTraceSource("FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor(&SnakeConsumer::m_firstInterestDataDelay),
                      "ns3::ndn::SnakeConsumer::FirstInterestDataDelayCallback");

  return tid;
}

SnakeConsumer::SnakeConsumer()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_seq(0)
  , m_seqMax(0) // don't request anything
{
  NS_LOG_FUNCTION_NOARGS();

  m_rtt = CreateObject<RttMeanDeviation>();
}

void
SnakeConsumer::SetRetxTimer(Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning()) {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
    Simulator::Remove(m_retxEvent); // slower, but better for memory
  }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule(m_retxTimer, &SnakeConsumer::CheckRetxTimeout, this);
}

Time
SnakeConsumer::GetRetxTimer() const
{
  return m_retxTimer;
}

void
SnakeConsumer::CheckRetxTimeout()
{
  Time now = Simulator::Now();

  Time rto = m_rtt->RetransmitTimeout();
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty()) {
    SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
      m_seqTimeouts.get<i_timestamp>().begin();
    if (entry->time + rto <= now) // timeout expired?
    {
      uint32_t seqNo = entry->seq;
      m_seqTimeouts.get<i_timestamp>().erase(entry);
      OnTimeout(seqNo);
    }
    else
      break; // nothing else to do. All later packets need not be retransmitted
  }

  m_retxEvent = Simulator::Schedule(m_retxTimer, &SnakeConsumer::CheckRetxTimeout, this);
}

// Application Methods
void
SnakeConsumer::StartApplication() // Called at time specified by Start
{
  NS_LOG_FUNCTION_NOARGS();

  // do base stuff
  App::StartApplication();

  ScheduleNextPacket();
}

void
SnakeConsumer::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS();

  // cancel periodic packet generation
  Simulator::Cancel(m_sendEvent);

  // cleanup base stuff
  App::StopApplication();
}

uint32_t
SnakeConsumer::getSeq()
{
  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return std::numeric_limits<uint32_t>::max(); // we are totally done
      }
    }

    seq = m_seq++;
  }
  return seq;
}
void
SnakeConsumer::SendPacket()
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();
  

  uint32_t seq = getSeq();
  if(seq == std::numeric_limits<uint32_t>::max()) return;

  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  nameWithSequence->append("snake");
  std::string dataName = "dataName";
  nameWithSequence->append(dataName);
  nameWithSequence->append("snake");
  std::string functionName = "functionName";
  nameWithSequence->append(functionName);
  nameWithSequence->appendSequenceNumber(seq);
  nameWithSequence->append("/Key-TID" + std::to_string(seq));

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(true);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  makeMetadataRequestInterest(interest);


  NS_LOG_INFO("> Interest for " << seq);
  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  ScheduleNextPacket();
}

void
SnakeConsumer::makeMetadataRequestInterest(shared_ptr<Interest> interest)
{
  
  /**Mark Function state as unexecuted*/
  snake_util::tryToMarkAsFunction(*interest);
  interest->setLongLived(true);
  /**Function requirements(JSON formate)*/
  ::ndn::snake::Parameters paras;
  ns3::StringValue O;
  g_O.GetValue(O);
  std::string str = "{\"O\":\"" + O.Get() + "\", \"S\":\"n\"}";
	  paras.Initialize(str); //< O: n^2, n, log(n), 1
  Block block = ::ndn::encoding::makeStringBlock(::ndn::tlv::ApplicationParameters,
                                                 paras.Serialize());
  interest->setApplicationParameters(block);
  /**Inject session id to establish session*/
  uint64_t currentNodeUuid = snake_util::getCurrentNodeSysInfo()->getUuid();
  snake_util::injectSessionId(*interest, currentNodeUuid, snake_util::xorOperator);

  NS_LOG_DEBUG("> Interest for metadata : " << ::ndn::encoding::readString(interest->getApplicationParameters()));

}

void
 SnakeConsumer::makeResultRequestInterest(shared_ptr<Interest> minCostNotifyInterest, 
  shared_ptr<const Data> metadata)
{
  auto minCostMarkerTagPtr = metadata->getTag<lp::MinCostMarkerTag>();

  /**Transfer min cost marker tag*/
  minCostNotifyInterest->setTag(minCostMarkerTagPtr);
  /**Mark Function state as unexecuted*/
  snake_util::tryToMarkAsFunction(*minCostNotifyInterest);
  /**Function requirements(JSON formate)*/
  const ndn::Block* funcParasBlock = metadata->getMetaInfo().findAppMetaInfo(::ndn::lp::tlv::AppParasInData);
  if(nullptr != funcParasBlock){
    std::string parasStr = ::ndn::encoding::readString(*funcParasBlock);
    ::ndn::snake::Parameters paras;
	  paras.Initialize(parasStr);
    minCostNotifyInterest->setApplicationParameters(
      ::ndn::encoding::makeStringBlock(::ndn::tlv::ApplicationParameters, paras.Serialize()));
    NS_LOG_INFO("> With function parameters: " << paras.Serialize());
  }
  NS_LOG_INFO("> Interest for Request Result: " << minCostNotifyInterest->getName().toUri());
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////
bool
SnakeConsumer::isSnakeMetadata(shared_ptr<const Data> data)
{
  auto metaDataTag = data->getTag<lp::MetaDataTag>();
  return nullptr != metaDataTag;
}

bool
SnakeConsumer::ackSnakeMetadata(shared_ptr<const Data> metadata)
{

  if (!m_active)
    return false;

  NS_LOG_FUNCTION_NOARGS();

  // uint32_t seq = getSeq();
  // if(seq == 0) return;
  int nameSize = metadata->getName().size();
  ::ndn::Name namePrefix = metadata->getName().getSubName(0, nameSize-1); //< ndn:://snake/dataName/snake/functionName/Seq#/Key-TID#/ApplicationParameters
  // shared_ptr<Name> nameWithSequence = make_shared<Name>(namePrefix);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));

  interest->setName(namePrefix);
  interest->setCanBePrefix(true);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  makeResultRequestInterest(interest, metadata);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  return true;
}
void
SnakeConsumer::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  // NS_LOG_INFO ("Received content object: " << boost::cref(*data));

  // This could be a problem......
  uint32_t seq = data->getName().at(-3).toSequenceNumber();
  NS_LOG_INFO("< DATA for " << seq);
  // std::cout << "< DATA for " << seq;
  if(data->getTag<lp::FunctionTag>()) NS_LOG_DEBUG("Results size: " << data->getContent().value_size());
  std::cout << data->getContent().value() << std::endl;
  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);
  
  if(isSnakeMetadata(data)){
    NS_LOG_DEBUG("Entering ack snake metadata pipeline.");
    ackSnakeMetadata(data);
  } else {
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
}

void
SnakeConsumer::OnNack(shared_ptr<const lp::Nack> nack)
{
  /// tracing inside
  App::OnNack(nack);

  NS_LOG_INFO("NACK received for: " << nack->getInterest().getName()
              << ", reason: " << nack->getReason());
}

void
SnakeConsumer::OnTimeout(uint32_t sequenceNumber)
{
  NS_LOG_FUNCTION(sequenceNumber);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
  // m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

  m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);
  ScheduleNextPacket();
}

void
SnakeConsumer::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
                                << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;

  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1);
}

} // namespace ndn
} // namespace ns3
