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

#include "ns3/log.h"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"

#include "ns3/ndnSIM/apps/snake/ndn-snake-app.hpp"
#include "snake-bolt.hpp"
// NS_LOG_COMPONENT_DEFINE("ndn.BoltApp");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(SnakeBoltApp);

SnakeBoltApp::SnakeBoltApp(){}
SnakeBoltApp::~SnakeBoltApp(){}
TypeId
SnakeBoltApp::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ndn::SnakeBoltApp")
    .SetParent<BoltApp>()
    .AddConstructor<SnakeBoltApp>();
  return tid;
}
void
SnakeBoltApp::StartApplication(){ ///< @brief Called at time specified by Start
    m_instance.reset(new ::ns3::ndn::SnakeBolt(GetNode()));
    m_instance.get()->nodeId = std::to_string(GetNode()->GetId());
    m_instance->runp();
}
void
SnakeBoltApp::StopApplication(){ ///< @brief Called at time specified by Stop
  m_instance.reset();
}
} // namespace ndn
} // namespace ns3
