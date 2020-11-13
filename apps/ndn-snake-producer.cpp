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

#include "ndn-snake-producer.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/hash.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/snake-utils.hpp>
#include <ndn-cxx/meta-info.hpp>
#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.SnakeProducer");

namespace ns3 {
namespace ndn {
namespace snake_util = ::ndn::snake::util;

NS_OBJECT_ENSURE_REGISTERED(SnakeProducer);

TypeId
SnakeProducer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::SnakeProducer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<SnakeProducer>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&SnakeProducer::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&SnakeProducer::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&SnakeProducer::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&SnakeProducer::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&SnakeProducer::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&SnakeProducer::m_keyLocator), MakeNameChecker());
  return tid;
}

SnakeProducer::SnakeProducer()
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
SnakeProducer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
}

void
SnakeProducer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}
bool 
SnakeProducer::isRequestForMetadata(shared_ptr<const Interest> interest)
{ 
  NS_LOG_FUNCTION(this << interest);
  //This should be a problem...
  //Try to use Interest instead in the future.
  // return nullptr == interest->getTag<lp::MinCostMarkerTag>();

  std::string interestName = interest->getName().getSubName(0, 4).toUri();
  Block funcParasBlock = interest->getApplicationParameters();
  std::string funcParas = ::ndn::encoding::readString(funcParasBlock);
  uint64_t hashId = snake_util::hashing(interestName, funcParas, GetNode()->GetId());
  
  auto interestKey = repliedMetadataContiner.find(hashId);
  if(interestKey == repliedMetadataContiner.end()){//1st incoming interest
    NS_LOG_DEBUG("Inserting metadata request: " << hashId << " from : " << interestName << ", " << funcParas << ", " << GetNode()->GetId());
    repliedMetadataContiner.insert(hashId);
    return true;
  } else { //2nd incoming interest carrying min cost marker.
    NS_LOG_DEBUG("Erasing metadata: " << hashId << " from : " << interestName << ", " << funcParas << ", " << GetNode()->GetId());
    repliedMetadataContiner.erase(interestKey);
    return false;
  }
}

bool
SnakeProducer::ackMetadata(shared_ptr<const Interest> interest)
{
  NS_LOG_FUNCTION(this << interest);
  
  Name dataName(interest->getName());

  auto data = make_shared<Data>();
  auto functionTag = interest->getTag<lp::FunctionTag>();
  if(functionTag != nullptr){
    data->setTag(functionTag);
    NS_LOG_DEBUG("Marking a function tag into data: " << *(data->getTag<lp::FunctionTag>()));
  }
  auto sessionTag = interest->getTag<lp::SessionTag>();
  if(sessionTag != nullptr){//It's a session request.
    uint64_t currentNodeUuid = snake_util::getCurrentNodeSysInfo()->getUuid();
    snake_util::injectSessionId(*data, *sessionTag, snake_util::xorOperator);
    snake_util::injectSessionId(*data, currentNodeUuid, snake_util::xorOperator);
    NS_LOG_DEBUG("Establishing session from " << *sessionTag << " to " << currentNodeUuid
                << ", SessionId: " << *(data->getTag<lp::SessionTag>()));
  }
  data->setName(dataName);
  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  
  ::ndn::MetaInfo metaInfo;
  metaInfo.addAppMetaInfo(::ndn::encoding::makeStringBlock(lp::tlv::MetaData, "{\"filetype\":1, \"size\":10240}"));
  if(interest->hasApplicationParameters()){
    Block funcParasBlock = interest->getApplicationParameters();
    std::string funcParas = ::ndn::encoding::readString(funcParasBlock);
    NS_LOG_DEBUG("Transfer app paras into Data: " << funcParas);
    metaInfo.addAppMetaInfo(::ndn::encoding::makeStringBlock(lp::tlv::AppParasInData, funcParas));
  }
  data->setMetaInfo(metaInfo);
  data->setTag(make_shared<lp::MetaDataTag>(1));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));
  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }
  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Metadata: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_appLink->onReceiveData(*data);
  
  return true;
}
void
SnakeProducer::OnInterest(shared_ptr<const Interest> interest)
{


  if (!m_active)
    return;
  App::OnInterest(interest); // tracing inside
  NS_LOG_FUNCTION(this << interest);

  if(isRequestForMetadata(interest)){
    ackMetadata(interest);
    return;
  }

  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  // auto functionTag = interest->getTag<lp::FunctionTag>();
  // if(functionTag != nullptr){
  //   data->setTag(functionTag);
  //   NS_LOG_DEBUG("Marking a function tag into data: " << *(data->getTag<lp::FunctionTag>()));
  // }
  // auto sessionTag = interest->getTag<lp::SessionTag>();
  // if(sessionTag != nullptr){//It's a session request.
  //   uint64_t currentNodeUuid = snake_util::getCurrentNodeSysInfo()->getUuid();
  //   snake_util::injectSessionId(*data, *sessionTag, snake_util::xorOperator);
  //   snake_util::injectSessionId(*data, currentNodeUuid, snake_util::xorOperator);
  //   NS_LOG_DEBUG("Establishing session from " << *sessionTag << " to " << currentNodeUuid
  //               << ", SessionId: " << *(data->getTag<lp::SessionTag>()));
  // }
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  data->setContent(::ndn::encoding::makeStringBlock(::ndn::tlv::Content, "original data content"));
  // data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());


  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);

  m_appLink->onReceiveData(*data);
}


} // namespace ndn
} // namespace ns3
