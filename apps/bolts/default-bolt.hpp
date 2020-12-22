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
#ifndef NDN_DEFAULT_BOLT_H
#define NDN_DEFAULT_BOLT_H

#include "bolt.hpp"

namespace ns3 {

namespace ndn {
class SimpleTraceBolt: public Bolt {

public:
  SimpleTraceBolt():Bolt(ndn::StackHelper::getKeyChain())
  {

  }
  ~SimpleTraceBolt(){}
  std::string createData(const Interest& interest, const Data& inData)
  {
    std::string originStr = std::string(reinterpret_cast<const char*>(inData.getContent().value()),
                                       inData.getContent().value_size());
		return originStr + "[" + nodeId + "'s data]";
  }

  void processingData(const Interest& interest, Quest& quest, const Data &inData, shared_ptr<Data> outData) override
	{
		std::string newDataStr = createData(interest, inData);
    outData->setContent(reinterpret_cast<const uint8_t*>(newDataStr.c_str()), newDataStr.size());
  }
  /** \brief processing the incoming \c inInterest.
   * 
   */
  void processingInterest(const InterestFilter& filter, const Interest& inInterest, 
                                 Interest& outInterest) override
  {
    //do nothing
		
  }

};

}//namspace ndn
}//namespace ns3

#endif