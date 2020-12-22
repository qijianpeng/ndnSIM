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

#include "ndn-trace-app.hpp"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/packet.h"
#include "model/ndn-l3-protocol.hpp"

// NS_LOG_COMPONENT_DEFINE("ndn.SnakeBolt");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(TraceApp);
TraceApp::TraceApp(){}
TraceApp::~TraceApp(){}
TypeId
TraceApp::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ndn::TraceApp")
    .SetParent<Application>()
    .AddConstructor<TraceApp>();
  return tid;
}
void
TraceApp::StartApplication(){ ///< @brief Called at time specified by Start
    m_instance.reset(new TraceBolt(ndn::StackHelper::getKeyChain()));
    m_instance.get()->Dname = std::to_string(GetNode()->GetId());
    m_instance->runp();
}
void
TraceApp::StopApplication(){ ///< @brief Called at time specified by Stop
  m_instance.reset();
}
} // namespace ndn
} // namespace ns3
