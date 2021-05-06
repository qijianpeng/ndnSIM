/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2016  Regents of the University of California.
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

#include "ndn-l3-hop-delay-tracer.hpp"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/hash.h"

#include "daemon/table/pit-entry.hpp"

#include <fstream>
#include <boost/lexical_cast.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.L3HopDelayTracer");

namespace ns3 {
namespace ndn {

static std::list<std::tuple<shared_ptr<std::ostream>, std::list<Ptr<L3HopDelayTracer>>>>
  g_hopDelayTracers;

void
L3HopDelayTracer::Destroy()
{
  g_hopDelayTracers.clear();
}

void
L3HopDelayTracer::InstallAll(const std::string& file)
{
  std::list<Ptr<L3HopDelayTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
    Ptr<L3HopDelayTracer> trace = Install(*node, outputStream);
    tracers.push_back(trace);
  }

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_hopDelayTracers.push_back(std::make_tuple(outputStream, tracers));
}

void
L3HopDelayTracer::Install(const NodeContainer& nodes, const std::string& file)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<L3HopDelayTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  for (NodeContainer::Iterator node = nodes.Begin(); node != nodes.End(); node++) {
    Ptr<L3HopDelayTracer> trace = Install(*node, outputStream);
    tracers.push_back(trace);
  }

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_hopDelayTracers.push_back(std::make_tuple(outputStream, tracers));
}

void
L3HopDelayTracer::Install(Ptr<Node> node, const std::string& file)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<L3HopDelayTracer>> tracers;
  shared_ptr<std::ostream> outputStream;
  if (file != "-") {
    shared_ptr<std::ofstream> os(new std::ofstream());
    os->open(file.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      NS_LOG_ERROR("File " << file << " cannot be opened for writing. Tracing disabled");
      return;
    }

    outputStream = os;
  }
  else {
    outputStream = shared_ptr<std::ostream>(&std::cout, std::bind([]{}));
  }

  Ptr<L3HopDelayTracer> trace = Install(node, outputStream);
  tracers.push_back(trace);

  if (tracers.size() > 0) {
    // *m_l3RateTrace << "# "; // not necessary for R's read.table
    tracers.front()->PrintHeader(*outputStream);
    *outputStream << "\n";
  }

  g_hopDelayTracers.push_back(std::make_tuple(outputStream, tracers));
}

Ptr<L3HopDelayTracer>
L3HopDelayTracer::Install(Ptr<Node> node, shared_ptr<std::ostream> outputStream)
{
  NS_LOG_DEBUG("Node: " << node->GetId());

  Ptr<L3HopDelayTracer> trace = Create<L3HopDelayTracer>(outputStream, node);
  return trace;
}

L3HopDelayTracer::L3HopDelayTracer(shared_ptr<std::ostream> os, Ptr<Node> node)
  : L3Tracer(node)
  , m_os(os)
{
  // SetAveragingPeriod(Seconds(1.0));
}

L3HopDelayTracer::L3HopDelayTracer(shared_ptr<std::ostream> os, const std::string& node)
  : L3Tracer(node)
  , m_os(os)
{
  // SetAveragingPeriod(Seconds(1.0));
}

L3HopDelayTracer::~L3HopDelayTracer()
{
  m_printEvent.Cancel();
}

void
L3HopDelayTracer::PrintHeader(std::ostream& os) const
{
  os << "Time"
     << "\t"

     << "Node"
     << "\t"
     << "FaceId"
     << "\t"
     << "FaceDescr"
     << "\t"

     << "Type"
     << "\t"
     << "SeqNo";
}

void
L3HopDelayTracer::Reset()
{
  for (auto& stats : m_stats) {
    std::get<0>(stats.second).Reset();
    std::get<1>(stats.second).Reset();
  }
}

const double alpha = 0.8;

#define STATS(INDEX) std::get<INDEX>(stats.second)

#define PRINTER(printName, fieldName)                                                              \
  if("" != STATS(0).fieldName){                                                               \
    os << time.ToDouble(Time::S) << "\t" << m_node << "\t";                                        \
    if (stats.first != nfd::face::INVALID_FACEID) {                                                \
      os << stats.first << "\t";                                                                   \
      NS_ASSERT(m_faceInfos.find(stats.first) != m_faceInfos.end());                               \
      os << m_faceInfos.find(stats.first)->second << "\t";                                         \
    }                                                                                              \
    else {                                                                                         \
      os << "-1\tall\t";                                                                           \
    }                                                                                              \
    os << printName << "\t" << STATS(0).fieldName << "\n";                                         \
  }

void
L3HopDelayTracer::Print(std::ostream& os, std::pair<nfd::FaceId, std::tuple<HopsStats, HopsStats>> stats) const
{
  Time time = Simulator::Now();

  if (stats.first == nfd::face::INVALID_FACEID)
    return;

  PRINTER("InInterests", m_inInterests);
  PRINTER("OutInterests", m_outInterests);

  PRINTER("InData", m_inData);
  PRINTER("OutData", m_outData);

  PRINTER("InNacks", m_inNack);
  PRINTER("OutNacks", m_outNack);
}

void
L3HopDelayTracer::OutInterests(const Interest& interest, const Face& face)
{
  AddInfo(face);
  int endPos = interest.hasApplicationParameters() ? interest.getName().size() - 2 : interest.getName().size();
  std::get<0>(m_stats[face.getId()]).m_outInterests = interest.getName().getSubName(0, endPos).toUri();

  // PRINTER("InInterests", m_outInterests) ;
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();
}

void
L3HopDelayTracer::InInterests(const Interest& interest, const Face& face)
{
  AddInfo(face);
  int endPos = interest.hasApplicationParameters() ? interest.getName().size() - 2 : interest.getName().size();
  std::get<0>(m_stats[face.getId()]).m_inInterests = interest.getName().getSubName(0, endPos).toUri();
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();
}

void
L3HopDelayTracer::OutData(const Data& data, const Face& face)
{
  AddInfo(face);
  int endPos = data.getName().size() - 2;
  std::get<0>(m_stats[face.getId()]).m_outData = data.getName().getSubName(0, endPos).toUri();
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();

}

void
L3HopDelayTracer::InData(const Data& data, const Face& face)
{
  AddInfo(face);
  int endPos = data.getName().size() - 2;
  std::get<0>(m_stats[face.getId()]).m_inData = data.getName().getSubName(0, endPos).toUri();
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();
}

void
L3HopDelayTracer::OutNack(const lp::Nack& nack, const Face& face)
{
  AddInfo(face);
  int endPos = nack.getInterest().hasApplicationParameters() ? nack.getInterest().getName().size() - 2 : nack.getInterest().getName().size();
  std::get<0>(m_stats[face.getId()]).m_outNack = nack.getInterest().getName().getSubName(0, endPos).toUri();
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();
}

void
L3HopDelayTracer::InNack(const lp::Nack& nack, const Face& face)
{
  AddInfo(face);
  int endPos = nack.getInterest().hasApplicationParameters() ? nack.getInterest().getName().size() - 2 : nack.getInterest().getName().size();
  std::get<0>(m_stats[face.getId()]).m_inNack = nack.getInterest().getName().getSubName(0, endPos).toUri();
  Print(*m_os, *(m_stats.find(face.getId())));
  std::get<0>(m_stats[face.getId()]).Reset();
}


void
L3HopDelayTracer::AddInfo(const Face& face)
{
  if (m_faceInfos.find(face.getId()) == m_faceInfos.end()) {
    m_faceInfos.insert(make_pair(face.getId(), boost::lexical_cast<std::string>(face.getLocalUri())));
  }
}

} // namespace ndn
} // namespace ns3
