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

// #include "ns3/ndnSIM/model/ndn-common.hpp"
#include "data-bolt.hpp"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"

namespace ns3 {

namespace ndn {

/**
 * \ingroup ns3
 * \defgroup ndn-snake-apps NDN applications
 */
/**
 * @ingroup ndn-snake-apps
 * @brief Base class that all Snake bolt NDN applications should be derived from.
 */
class SnakeBolt : public Application {
public:
 
  static TypeId
  GetTypeId();

  /**
   * @brief Default constructor
   */
  SnakeBolt();
  ~SnakeBolt();

  // inherited from Application base class. Originally they were private
  void
  StartApplication(); ///< @brief Called at time specified by Start
  void
  StopApplication(); ///< @brief Called at time specified by Stop
private:
	std::unique_ptr<DataBolt> m_instance;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_SNAKE_BOLT_H
