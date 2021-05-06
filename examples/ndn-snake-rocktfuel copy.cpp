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

// ndn-congestion-topo-plugin.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

#include "apps/snake/ndn-snake-app.hpp"
namespace ns3 {

/**
 * 
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn-cxx.nfd.Strategy:ndn-cxx.nfd.Forwarder:ndn.SnakeConsumer:ndn.SnakeProducer ./waf --run=ndn-snake-rocktfuel
 */

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/bw-delay-node-resource-productive-rand-1/1755.r1-conv-annotated.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.setPolicy("nfd::cs::lru");
  ndnHelper.setCsSize(10000);
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/snake", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();
  // Snake bolt
  ndn::AppHelper appHelper("ns3::ndn::SnakeBoltApp");
  NodeContainer nodes = NodeContainer::GetGlobal();
  for (NodeContainer::Iterator node = nodes.Begin(); node != nodes.End(); node++) {
    appHelper.Install(*node).Start(Seconds(0));
  }
  // Getting containers for the consumer/producer
  // Ptr<Node> consumer1 = Names::Find<Node>("Src1");
  // Ptr<Node> consumer2 = Names::Find<Node>("Src2");

  // Ptr<Node> producer1 = Names::Find<Node>("Dst1");
  // Ptr<Node> producer2 = Names::Find<Node>("Dst2");
  
  // Ptr<Node> consumer = Names::Find<Node>("leaf-224");
  Ptr<Node> consumer = Names::Find<Node>("leaf-224-pre");

  Ptr<Node> producer = Names::Find<Node>("leaf-41");
  
  std::string prefix = "/snake";
  std::string traceRoutePrefix = "/Trace/S/p";
  ndn::AppHelper consumerHelper("ns3::ndn::SnakeConsumerCbr");
  consumerHelper.SetAttribute("Frequency", StringValue("1")); // 100 interests a second
  // on the first consumer node install a Consumer application
  // that will express interests in /dst1 namespace
  consumerHelper.SetPrefix(traceRoutePrefix);
  consumerHelper.Install(consumer);

  ndn::AppHelper producerHelper("ns3::ndn::SnakeProducer");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetPrefix(prefix);
  producerHelper.Install(producer);
  ndnGlobalRoutingHelper.AddOrigins(prefix, producer);



  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();

  Simulator::Stop(Seconds(10.0));
  std::string timestamp = std::to_string(std::time(nullptr));
  ns3::StringValue oValue;
  GlobalValue::GetValueByName ("O", oValue);
  ns3::StringValue rttSwitchValue;
  GlobalValue::GetValueByName ("RttOpt", rttSwitchValue);
  ns3::DoubleValue alphaValue;
  GlobalValue::GetValueByName ("Alpha", alphaValue);
  ns3::UintegerValue dataSizeValue;
  GlobalValue::GetValueByName ("DataSize", dataSizeValue);
  std::string identity = "-alpha_" + std::to_string(alphaValue.Get()) + "-O_" + oValue.Get()
                         + "-RttOpt_" + rttSwitchValue.Get() + "-DataSize_" + std::to_string(dataSizeValue.Get());
  std::string l3rateTraceFileName = "../draft/analysis/throughput/rate-trace"+ identity + ".txt";
  ndn::L3RateTracer::InstallAll(l3rateTraceFileName, Seconds(0.05));
  // ndn::L3RateTracer::InstallAll(l3rateTraceFileName, MilliSeconds(5));
  std::string appDelatyTraceFileName = "../draft/analysis/throughput/app-delays-trace"+ identity + ".txt";
  ndn::AppDelayTracer::InstallAll(appDelatyTraceFileName);
  std::string hopsDelatyTraceFileName = "../draft/analysis/throughput/hops-delay-trace"+ identity + ".txt";
  ndn::L3HopDelayTracer::InstallAll(hopsDelatyTraceFileName);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
