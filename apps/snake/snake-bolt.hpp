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
#ifndef NDN_SNAKE_BOLT_H
#define NDN_SNAKE_BOLT_H

#include <climits>

#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/ndnSIM/apps/bolts/bolt.hpp"
#include "ns3/node-list.h"
#include "ns3/computation-module.h"
#include "ns3/core-module.h"
#include "ns3/simulator.h"//< Just for simulation.

#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/snake-utils.hpp>
#include "ndn-cxx/util/logger.hpp"
#include "ndn-cxx/snake-parameters.hpp"
#include "ndn-cxx/snake/optimize-strategy.hpp"
NS_LOG_COMPONENT_DEFINE("ndn.SnakeBolt");

namespace ns3 {

namespace ndn {
static ns3::GlobalValue g_switchRttOpt = ns3::GlobalValue ("RttOpt",
                                                     "A global value to on/off RTT optimization.",
                                                     ns3::StringValue ("on"),
                                                     ns3::MakeStringChecker());
static ns3::GlobalValue g_dataSize = ns3::GlobalValue ("DataSize",
                                                     "A global value(byte) to data size.",
                                                     ns3::UintegerValue (1024),
                                                     ns3::MakeUintegerChecker<uint64_t>(0,ULLONG_MAX));

namespace snake_util = ::ndn::snake::util;

class SnakeBolt: public Bolt {
protected:
   RTT_OPT rttOpt = ON;
public:
  SnakeBolt(Ptr<Node> node): Bolt(ndn::StackHelper::getKeyChain())
  {
    ns3::StringValue switchRttOpt;
    g_switchRttOpt.GetValue(switchRttOpt);
    std::string switchRttOptStr = switchRttOpt.Get();
    rttOpt = getRttOptStrategy(switchRttOptStr);
    this->m_node = node;
  }
  ~SnakeBolt(){}
  std::string createData(const Interest& interest, const Data& inData)
  {
    std::string originStr = std::string(reinterpret_cast<const char*>(inData.getContent().value()),
                                       inData.getContent().value_size());
		return originStr + "[" + nodeId + "'s data]";
  }
  
  uint64_t functionInvoke(ns3::Ptr<ns3::Node> &node, Data& data, const std::string functionName, const std::string functionParasJSONStr)
  {
    return snake_util::functionInvoke(node, data, functionName, functionParasJSONStr);
  }
  std::string extractFunctionName(Data & data)
  {
    std::string uri = data.getName().toUri();
    uri = snake_util::unescape(uri);
    return snake_util::extractFunctionName(uri);
  }
  std::string extractFunctionName(Interest & interest)
  {
    std::string uri = interest.getName().toUri();
    uri = snake_util::unescape(uri);
    return snake_util::extractFunctionName(uri);
  }
  std::string extractFunctionParas(const Interest & interest)
  {
    Block block = interest.getApplicationParameters();
    return ndn::encoding::readString(block);
  }
  void
  turnbackAfterProcessingData(const Interest& inInterest, const Data &inData, shared_ptr<Data> metadata) override
  {
    // std::string outDataName = outData->getName().toUri();
    // BOOST_ASSERT(outDataName == Reqs[interest.getNonce()].Oname);
    ndn::Interest minCostNotifyInterest;
    
		turnBackPreprocessingInterest(inInterest, minCostNotifyInterest);
    // Quest quest = getQuest(inInterest.getNonce());
    // minCostNotifyInterest.setName(inInterest.getName());
    auto minCostMarkerTagPtr = metadata->getTag<lp::MinCostMarkerTag>();
    minCostNotifyInterest.setTag(minCostMarkerTagPtr);
    snake_util::tryToMarkAsFunction(minCostNotifyInterest);
    const ndn::Block* funcParasBlock = metadata->getMetaInfo().findAppMetaInfo(::ndn::lp::tlv::AppParasInData);
    if(nullptr != funcParasBlock){
      std::string parasStr = ::ndn::encoding::readString(*funcParasBlock);
      ::ndn::snake::Parameters paras;
      paras.Initialize(parasStr);
      minCostNotifyInterest.setApplicationParameters(
        ::ndn::encoding::makeStringBlock(::ndn::tlv::ApplicationParameters, paras.Serialize()));
      NS_LOG_INFO("> With function parameters: " << paras.Serialize());
    }
		afterProcessingInterest(inInterest, minCostNotifyInterest);
    NS_LOG_INFO("> Interest for Request Result: " << minCostNotifyInterest.getName().toUri());

  }



  FORWARDING_ACTION processingData(const Interest& interest, Quest& quest, const Data &inData, shared_ptr<Data> outData) override
	{
	
    // outData->wireDecode(inData.wireEncode());

    NS_LOG_DEBUG("Data: " << inData.getName());
    // snake_util::cloneTags(inData, *outData);

    boost::uint64_t contextVal = m_node->GetId();
    if(contextVal != -1){
      if(nullptr == inData.getTag<lp::HopDelayTag>()){
        boost::uint64_t now = time::duration_cast<time::microseconds>(time::steady_clock::now().time_since_epoch()).count();
        boost::uint64_t idWithTime = (boost::uint64_t(contextVal) << 56) | (now & 0x00ffffffffffffff);
        outData->setTag(make_shared<lp::HopDelayTag>(idWithTime));
      } else {
        // auto timeP = inData.getTag<lp::HopDelayTag>();
        // boost::uint64_t time = (*timeP) & 0x00ffffffffffffff;
        // boost::uint64_t idWithTime = (boost::uint64_t(contextVal) << 56) | time;
      }
      ns3::Ptr<ns3::Node> node = m_node;
        ns3::Ptr<ns3::ComputationModel> cm = node->GetObject<ns3::ComputationModel>();
        if(snake_util::isBelong2SnakeSystem(*outData) &&
          !(snake_util::isFunctionExecuted(*outData))){
          if(cm != 0){
            //Get current node available resources
            ns3::SysInfo sysinfo = cm->GetSystemStateInfo();
            //Extract function name
            std::string functionName = extractFunctionName(*outData);
            //Extract function parameters
            std::string functionParameters = extractFunctionParas(interest);

            auto metaDataTag = outData->getTag<lp::MetaDataTag>();
            //Function execution cost estimating and updating.
            if(nullptr != metaDataTag){
              auto minCost = outData->getTag<lp::MinCostTag>();
              if(nullptr == minCost){
                minCost = make_shared<lp::MinCostTag>(ULLONG_MAX);
                outData->setTag(minCost);
              }
              std::tuple<uint64_t, uint64_t> costEtaTuple = snake_util::costEstimator(interest, *outData, node);
              uint64_t costEta = std::get<0>(costEtaTuple);
              NS_LOG_DEBUG("Estimatting cost on current node: " << costEta );

              if(*minCost > costEta){
                NS_LOG_DEBUG("Original cost: " << *minCost <<", cost on current node is smaller: " << costEta);
                // std::cout << node->GetId() << ", Original cost: " << *minCost <<", cost on current node is smaller: " << costEta << std::endl;
                outData->setTag(make_shared<lp::MinCostTag>(costEta));
                uint64_t marker = snake_util::hashing(functionName, functionParameters, sysinfo.getUuid());
                NS_LOG_DEBUG("Marking new node to run " << functionName << ", paras: " 
                              << functionParameters << " with marker: " << marker);
                outData->setTag(make_shared<lp::MinCostMarkerTag>(marker));
              }
              if(rttOpt == RTT_OPT::ON) {
                auto maxCost = outData->getTag<lp::MaxCostTag>();
                if(nullptr == maxCost) {//init
                  maxCost = make_shared<lp::MaxCostTag>(costEta);
                  outData->setTag(maxCost);
                }
                uint64_t delay = std::get<1>(costEtaTuple);
                if(delay >= *maxCost){// 1st compare
                  return FORWARDING_ACTION::TURN_BACK_ACTION;
                }
                if(*maxCost < costEta) {//2nd update
                  outData->setTag(make_shared<lp::MaxCostTag>(costEta));
                }
              }

            } else if ( snake_util::canExecuteFunction(*outData)) {//Function invoking
              uint64_t marker = snake_util::hashing(functionName, functionParameters, sysinfo.getUuid());
              auto choosenMarker = outData->getTag<lp::MinCostMarkerTag>();
              if(nullptr != choosenMarker && marker == *choosenMarker){
                NS_LOG_DEBUG("Trying to execute function on Node " << node->GetId()
                              << ", Function name: " << functionName << ", Function parameters: " 
                              << functionParameters);
                NS_LOG_DEBUG("Data name: " << outData->getName().toUri());
                NS_LOG_DEBUG("Node current avaliable resources: " << sysinfo.Serialize());
                functionInvoke(node, *outData, functionName, functionParameters);
                NS_LOG_DEBUG("End executing function: " << functionName);
              } else {
                if(nullptr == choosenMarker) {
                  NS_LOG_ERROR("MinCostMarker is nullptr for: " << outData->getName().toUri());

                } else {
                  NS_LOG_DEBUG("MinCostMarker orign > "<< *choosenMarker << ", current node > " << marker
                             << outData->getName().toUri());
                }
              }
            }
          } else {
            NS_LOG_ERROR("Computation Model not installed!");
          }
        }
    }
    return FORWARDING_ACTION::GO_FORWARD_ACTION;
  }
  /** \brief processing the incoming \c inInterest.
   * 
   */
  void processingInterest(const InterestFilter& filter, const Interest& inInterest, 
                                 Interest& outInterest) override
  {
    NS_LOG_DEBUG("Interest: " << inInterest.getName());

  //------hard code for consumer/producer execution
  ns3::StringValue switchRttOpt;
  GlobalValue::GetValueByName("RttOpt", switchRttOpt);
  RTT_OPT rttOpt = getRttOptStrategy(switchRttOpt.Get());
  Ptr<Node> consumer = Names::Find<Node>("leaf-224");
  Ptr<Node> producer = Names::Find<Node>("leaf-41");
  if(rttOpt == RTT_OPT::LOCAL && m_node->GetId() == consumer->GetId()){//just send an result request interest.
      ns3::Ptr<ns3::ComputationModel> cm = m_node->GetObject<ns3::ComputationModel>();
      ns3::SysInfo sysinfo = cm->GetSystemStateInfo();
      Block block = inInterest.getApplicationParameters();
      std::string functionParameters =  ::ndn::encoding::readString(block);
      uint64_t marker = snake_util::hashing("functionName", functionParameters, sysinfo.getUuid());
      inInterest.setTag(make_shared<lp::MinCostMarkerTag>(marker));
      outInterest.setTag(make_shared<lp::MinCostMarkerTag>(marker));
      NS_LOG_DEBUG("Marking consumer marker: " << marker);
  } else if(rttOpt == RTT_OPT::PRODUCER && m_node->GetId() == producer->GetId()){
      ns3::Ptr<ns3::ComputationModel> cm = m_node->GetObject<ns3::ComputationModel>();
      ns3::SysInfo sysinfo = cm->GetSystemStateInfo();
      Block block = inInterest.getApplicationParameters();
      std::string functionParameters =  ::ndn::encoding::readString(block);
      uint64_t marker = snake_util::hashing("functionName", functionParameters, sysinfo.getUuid());
      inInterest.setTag(make_shared<lp::MinCostMarkerTag>(marker));
      outInterest.setTag(make_shared<lp::MinCostMarkerTag>(marker));
      NS_LOG_DEBUG("Marking producer marker: " << marker);
  } else {
  }
  //------end
    //do nothing
		outInterest.setApplicationParameters(inInterest.getApplicationParameters());
    
    // if(rttOpt == RTT_OPT::LOCAL ) {
    //   ns3::Ptr<ns3::ComputationModel> cm = m_node->GetObject<ns3::ComputationModel>();
    //   ns3::SysInfo sysinfo = cm->GetSystemStateInfo();
    //   std::string functionParameters = extractFunctionParas(inInterest);
    //   std::string functionName = extractFunctionName(inInterest);
    //   uint64_t marker = snake_util::hashing(functionName, functionParameters, sysinfo.getUuid());
    //   outInterest->setTag(make_shared<lp::MinCostMarkerTag>(marker));
    // }
  }
private:
  Ptr<Node>       m_node;
};

}//namspace ndn
}//namespace ns3

#endif