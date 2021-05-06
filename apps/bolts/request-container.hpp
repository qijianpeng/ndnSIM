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
#include "ndn-cxx/util/time.hpp"

#include <map>
#include <chrono>
#include <ctime>
#include <stdio.h>
#include <cstdio>
#include <set>

namespace ns3 {

namespace ndn {
using namespace std::chrono;
 // Request Information
  struct Quest{
    steady_clock::time_point First_Arr;
    steady_clock::time_point Express_time;
    steady_clock::time_point Rep_time;
    steady_clock::time_point E;
    steady_clock::time_point Ef;
    std::string p1;
    std::string p2;
    std::string Oname;
    std::string Lname;
    std::string LFname;
    std::string Reply;
    std::vector<uint64_t> mhops; // multipath's possible next hops
    std::map<std::string, std::string> m_reply; //multi path replies
    std::map<std::string, steady_clock::time_point> m_exptime; //multi path express
    std::map<std::string, steady_clock::time_point> m_reptime; //multi path replies
    std::map<std::string, double> m_over; //multi path replies
    Quest* Oquest = 0;
  };
}
}