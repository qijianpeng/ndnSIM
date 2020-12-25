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

#ifndef NDN_TRACE_BOLT_H
#define NDN_TRACE_BOLT_H

// #include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-cxx/name.hpp"
#include <ndn-cxx/mgmt/nfd/controller.hpp>
#include <ndn-cxx/mgmt/nfd/fib-entry.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <map>
#include <chrono>
#include <ctime>
#include <stdio.h>
#include <boost/thread/thread.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <cstdio>
#include <set>
#include <random>
namespace ns3 {

namespace ndn {
using namespace std::chrono;
using ::ndn::nfd::Controller;
using namespace rapidjson;
using namespace ::ndn;
using namespace ::ndn::nfd;

typedef std::chrono::high_resolution_clock Clock;
/**
 * \ingroup ns3
 * \defgroup ndn-snake-apps NDN applications
 */
/**
 * @ingroup ndn-snake-apps
 * @brief Base class that all Snake bolt NDN applications should be derived from.
 */
class TraceBolt{
public:
  void
  runp()
  {
    a_face.processEvents();
  }
  /**
   * @brief Default constructor
   */
  TraceBolt(KeyChain& keyChain): a_face(nullptr, keyChain), m_keyChain(keyChain), 
															 m_scheduler(a_face.getIoService()){
		a_face.setInterestFilter("/Trace",
                             bind(&TraceBolt::onInterest, this, _1, _2),
                             RegisterPrefixSuccessCallback(), // RegisterPrefixSuccessCallback is optional
                             bind(&TraceBolt::onRegisterFailed, this, _1, _2));
  }
  ~TraceBolt(){}

public:
	float estimate(){
		Ts = steady_clock::now(); // to estimate name creation and all
		shared_ptr<Data> data = make_shared<Data>();
		Name n = "/Test";
		data->setName(n);
		std::string stupid = "Hello Kitty";
		data->setFreshnessPeriod(time::seconds(10));
		data->setContent(reinterpret_cast<const uint8_t*>(stupid.c_str()), stupid.size());
		m_keyChain.sign(*data);
		To = steady_clock::now();
		steady_clock::duration time_span = To - Ts;
		float nseconds = float(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
		return nseconds;
	}

	double
	average(){
		float sum =0;
		for (int i = 0; i <100; i++){
			sum = sum +estimate();
		}
		return sum/100;
	}
	
	double mean =0;
	std::string Dname;
protected:
	int number(){
		static std::uniform_int_distribution<uint32_t> distribution;
		return distribution(getRandomGenerator());
	}

	static std::mt19937&
	getRandomGenerator()
	{
		static std::mt19937 rng{std::random_device{}()};
		return rng;
	}

  std::string
	getcomp(const Interest& interest, int i){
		return interest.getName().at(-i).toUri();;
	}

	void
	onRegisterFailed(const Name& prefix, const std::string& reason)
	{
		std::cerr << "ERROR: Failed to register prefix \""
				<< prefix << "\" in local hub's daemon (" << reason << ")"
				<< std::endl;
		a_face.shutdown();
	}
	void
	clean(uint32_t key)
	{
		auto it = loop.find(key);
    loop.erase(it);
		auto itr = Reqs.find(key);
		Reqs.erase(itr);
	}

    /// loop check
	bool
	Islooped(const Interest& interest){
		bool verdict = false;
		if(loop.find(interest.getNonce()) != loop.end()){
			verdict = true;
		} else {
			loop.insert(interest.getNonce());
		}
		return verdict;
	}

	void
	onInterest(const InterestFilter& filter, const Interest& interest)
	{
		if (Islooped(interest)){
	// std::cerr<< "Looped Interest for Nonce: " << interest.getNonce()<< std::endl;
		  return;
		}
		struct Quest r;
		r.First_Arr = steady_clock::now();    // Record arrival time of this request
		r.Oname = interest.getName().toUri(); // Record original name of this request
		Reqs[interest.getNonce()]=r;

		// looped trace interest?


		int k = (-1)*(interest.getName().size());
		int i;
		Reqs[interest.getNonce()].p1 = interest.getName().at(k+1).toUri();
		Reqs[interest.getNonce()].p2 = interest.getName().at(k+2).toUri();

		if (Reqs[interest.getNonce()].p1=="M"){ // Could be from User (no face_id) or from another daemon
			// Get the name to trace
			const ndn::Name c = interest.getName().at(k+3).toUri();
			const ndn::Name nx = c.toUri() ;
			ndn::Name v = nx.toUri();
			// Different names according to where the interest is coming from UID or TID
			std::size_t found4 = interest.getName().at(-1).toUri().find("Key-UID");
			if (found4!=std::string::npos){// user interest
				for(i=k+4; i< -1; i++){
					v = v.toUri() + "/" + interest.getName().at(i).toUri();
				}
			}else{// another daemon's interest
				for(i=k+4; i< -2; i++){
					v = v.toUri() + "/" + interest.getName().at(i).toUri();
				}
			}
			//adding the name to the map
			Reqs[interest.getNonce()].Lname = v.toUri();
			// Query the Fib manager
			KeyChain x_keyChain;
			::ndn::nfd::Controller cont(a_face, x_keyChain);
			const ::ndn::nfd::CommandOptions& options = {};
			const ::ndn::Name& name = Reqs[interest.getNonce()].Lname;
			query(cont, name, interest.getNonce(), options);
		}else{
			// Enter single path section
			const ::ndn::Name c = interest.getName().at(k+3).toUri();
			const ::ndn::Name nx = c.toUri() ;
			::ndn::Name v = nx.toUri();
			// Getting the name to lookup
			for(i=k+4; i< -1; i++){
				v = v.toUri() + "/" + interest.getName().at(i).toUri();  // The name to lookup
			}
			Reqs[interest.getNonce()].Lname = v.toUri();
			ndn::Interest I;
			::ndn::Name n = "/Trace/S/"+Reqs[interest.getNonce()].p2+ v.toUri()+ "/Key-TID" + std::to_string(number()) ;
			I.setName(n);
			I.setNonce(interest.getNonce());
			I.setCanBePrefix(interest.getCanBePrefix());
			I.setInterestLifetime(interest.getInterestLifetime());
			// I.setMustBeFresh(true);
		
			Reqs[interest.getNonce()].Express_time = steady_clock::now();
      m_scheduler.schedule(ndn::time::seconds(0), [I, this] {
                a_face.expressInterest(I,
                                       bind(&TraceBolt::onData, this,  _1, _2),
                                       bind(&TraceBolt::onNack, this, _1, _2),
                                       bind(&TraceBolt::onTimeout, this, _1));
      });
			return;
		}
	}

	void
	onData(const Interest& interest, const Data& data)
	{
		double wait =0;
		if (Reqs[interest.getNonce()].p1 == "M"){  // Trace/M/name/Key-Tid/face_id

			Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)]=steady_clock::now();
			steady_clock::duration time_span = Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)] - Reqs[interest.getNonce()].m_exptime[getcomp(interest, 2)];

			double nseconds = float(time_span.count()) * steady_clock::period::num / steady_clock::period::den;

			std::string temp = std::string(reinterpret_cast<const char*>(data.getContent().value()), data.getContent().value_size());

			// Computing wait time for each interest
			for(std::map<std::string,steady_clock::time_point>::iterator it=Reqs[interest.getNonce()].m_reptime.begin(); it!=Reqs[interest.getNonce()].m_reptime.end(); ++it){

				steady_clock::duration time_span =Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)]-it->second;;

				double nseconds = float(time_span.count()) * steady_clock::period::num / steady_clock::period::den;

				if ((nseconds)>wait){
					wait = nseconds;
				}
			}
			// delay between receiving the request for the first time and sending each multipath request
			steady_clock::duration T =Reqs[interest.getNonce()].m_exptime[getcomp(interest, 2)]- Reqs[interest.getNonce()].First_Arr;
			double DelayToSend = float(T.count()) * steady_clock::period::num / steady_clock::period::den;

			Reqs[interest.getNonce()].m_reply[getcomp(interest, 2)]=createData(interest, Dname ,nseconds,wait+DelayToSend, temp);
			if(Reqs[interest.getNonce()].mhops.size()==Reqs[interest.getNonce()].m_reply.size()){
				createmulti(Reqs[interest.getNonce()].m_reply, interest);
			}
			clean(interest.getNonce());
		}else{

			Reqs[interest.getNonce()].Reply = std::string(reinterpret_cast<const char*>(data.getContent().value()), data.getContent().value_size());
			Reqs[interest.getNonce()].Rep_time = steady_clock::now();
			shared_ptr<Data> outData = make_shared<Data>();
			dataprocessing(interest, outData);
		}
	}

	void
	dataprocessing(const Interest& interest, shared_ptr<Data> outData){
		// Delay time
		steady_clock::duration time_span = Reqs[interest.getNonce()].Rep_time-Reqs[interest.getNonce()].Express_time;
		double elapsedd_secs = double(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
		// First overhead time
		steady_clock::duration time_span2 = Reqs[interest.getNonce()].Express_time-Reqs[interest.getNonce()].First_Arr;	
		double extra = double(time_span2.count()) * steady_clock::period::num / steady_clock::period::den;		
		double d;
		d =mean+extra;
		std:: string h = Reqs[interest.getNonce()].Reply;
		std::string data_x = createData(interest, Dname, elapsedd_secs, d, h);
		
		Name dataName(Reqs[interest.getNonce()].Oname);
		outData->setName(dataName);
		outData->setFreshnessPeriod(time::seconds(10)); // to be adjusted to the parameter in the user
		outData->setContent(reinterpret_cast<const uint8_t*>(data_x.c_str()), data_x.size());
    
    clean(interest.getNonce());

		m_keyChain.sign(*outData);
		// a_face.put(*outData);
		m_scheduler.schedule(ndn::time::seconds(0), [outData, this] {
                  a_face.put(*outData);
							  }
							);
	}
	void
	onNack(const Interest& interest, const lp::Nack& nack)
	{
		double wait =0;
		if (Reqs[interest.getNonce()].p1 == "M"){
			Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)]=steady_clock::now();
			steady_clock::duration time_span = Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)] - Reqs[interest.getNonce()].m_exptime[getcomp(interest, 2)];
			double nseconds = float(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
			if ((nack.getReason()== lp::NackReason::PRODUCER_LOCAL)||(nack.getReason()== lp::NackReason::CACHE_LOCAL)){
				///// checking wait time
				for(std::map<std::string,steady_clock::time_point>::iterator it=Reqs[interest.getNonce()].m_reptime.begin(); it!=Reqs[interest.getNonce()].m_reptime.end(); ++it){
					steady_clock::duration time_span =Reqs[interest.getNonce()].m_reptime[getcomp(interest, 2)]-it->second;;
					double nseconds = float(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
					if ((nseconds)>wait){
						wait = nseconds;
					}
				}
				Reqs[interest.getNonce()].m_reply[getcomp(interest, 2)]= createNack(interest,Dname, nseconds, wait);
				if(Reqs[interest.getNonce()].mhops.size()==Reqs[interest.getNonce()].m_reply.size()){
					createmulti(Reqs[interest.getNonce()].m_reply, interest);
				}
        clean(interest.getNonce());
			}
		}else{
			if ((nack.getReason()== lp::NackReason::PRODUCER_LOCAL)||(nack.getReason()== lp::NackReason::CACHE_LOCAL)){ // expected 					nack could alsp be No route / prohibited / not supported ...
				Reqs[interest.getNonce()].Rep_time = steady_clock::now();
				nackprocessing(interest);
			}else{
				std::cout << "Other non supported Nacks for now" << interest << std::endl;
				return;
			}

		}
	}

	void
	nackprocessing(const Interest& interest){
		// delay time to get a nack (<>>strategy)
		steady_clock::duration time_span = Reqs[interest.getNonce()].Rep_time-Reqs[interest.getNonce()].Express_time;
		double elapsedn_secs = double(time_span.count()) * steady_clock::period::num / steady_clock::period::den;
		// First overhead time
		steady_clock::duration time_span2 = Reqs[interest.getNonce()].Express_time-Reqs[interest.getNonce()].First_Arr;
		double extra = double(time_span2.count()) * steady_clock::period::num / steady_clock::period::den;
		// Create new name, based on Interest's name
		Name dataName(Reqs[interest.getNonce()].Oname);
		double d = mean+extra;
		std::string idsp = createNack(interest, Dname, elapsedn_secs, d);
		// Create Data packet
		shared_ptr<Data> data = make_shared<Data>();
		data->setName(dataName);
		data->setFreshnessPeriod(time::seconds(0));
		data->setContent(reinterpret_cast<const uint8_t*>(idsp.c_str()), idsp.size());
    clean(interest.getNonce());
		m_keyChain.sign(*data);
		a_face.put(*data);
	}

	void createmulti(std::map<std::string, std::string> m, const Interest& interest){

		shared_ptr<Data> data = make_shared<Data>();
		data->setName(Reqs[interest.getNonce()].Oname);
		data->setFreshnessPeriod(time::seconds(0));
		std::string temp = format(m);
		data->setContent(reinterpret_cast<const uint8_t*>(temp.c_str()), temp.size());
		m_keyChain.sign(*data);
		a_face.put(*data);
	}
	std::string createData(const Interest& interest, std::string id, double delay, double over, std::string s)
	{
		rapidjson::Document document;
		document.SetObject();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
		Value i;
		char b[100];
		int len = sprintf(b, "%s", id.c_str());
		i.SetString(b, len, document.GetAllocator());
		Value d(delay);
		Value o(over);
		document.AddMember("Id", i , allocator);
		document.AddMember("delay", d, allocator);
		
		Reqs[interest.getNonce()].E = steady_clock::now();
		steady_clock::duration time_span2 = Reqs[interest.getNonce()].E - Reqs[interest.getNonce()].Rep_time;
		double extra = double(time_span2.count()) * steady_clock::period::num / steady_clock::period::den;
	        
	        double ov = over+extra;
	        Value o2(ov);
		
		document.AddMember("overhead", o2, allocator);
		
		rapidjson::Document document2;
		if (document2.Parse<0>( s.c_str() ).HasParseError() ) {
			// std::cout << "Parsing error: " << std::endl;
		}else{
			assert(document2.IsObject());
			document.AddMember("next", document2.GetObject(), allocator);
		}
		StringBuffer strbuf;
		Writer<StringBuffer> writer(strbuf);
		document.Accept(writer);
		return strbuf.GetString();
	}

	std::string format(std::map<std::string, std::string> m){
		rapidjson::Document document;
		document.SetObject();
		rapidjson::Value array(rapidjson::kArrayType);
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
		// here
		for (std::map<std::string,std::string>::iterator it=m.begin(); it!=m.end(); ++it){
			Value v;
			Value vid;
			Value vdelay;
			Value vover;

			// Split the string
			std::size_t found = it->second.find(",");
			std::string tmpstr = it->second.substr (0, found);
			std::size_t found2 = tmpstr.find (":");
			vid.SetString (tmpstr.substr (found2+2, tmpstr.size () - found2 - 3).c_str (), allocator);

			std::size_t found3 = it->second.find (",",found + 1);
			tmpstr = it->second.substr (found + 1, found3 - found);
			found2 = tmpstr.find (":");
			vdelay.SetDouble (std::atof ((tmpstr.substr (found2+1)).c_str ()));

			std::size_t found4 = it->second.find (",",found3 + 1);
			tmpstr = it->second.substr (found3 + 1, found4 - found3);
			found2 = tmpstr.find (":");
			vover.SetDouble (std::atof ((tmpstr.substr (found2+1)).c_str ()));


			v.SetObject ().AddMember ("Id", vid, allocator).AddMember ("delay", vdelay, allocator).AddMember ("overhead", vover, allocator);
			array.PushBack (v, allocator);

		}
		document.AddMember("next", array, document.GetAllocator());
		StringBuffer strbuf;
		Writer<StringBuffer> writer(strbuf);
		document.Accept(writer);
		std::string temp = strbuf.GetString();
		//return (reinterpret_cast<const uint8_t*>(temp.c_str()), temp.size());
		return temp;
	}

	void
	onTimeout(const Interest& interest)
	{
		std::cout << "Timeout " << interest << std::endl;
	}


	std::string createNack(const Interest& interest, std::string id, double delay, double over)
	{
		rapidjson::Document document;
		document.SetObject();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
		//nfd-id
		Value i;
		char b[100];
		int len = sprintf(b, "%s", id.c_str());
		i.SetString(b, len, document.GetAllocator());
		//delay
		Value d(delay);
		
		document.AddMember("Id", i , allocator);
		document.AddMember("delay", d, allocator);
		
		
		Reqs[interest.getNonce()].E = steady_clock::now();
		steady_clock::duration time_span2 = Reqs[interest.getNonce()].E - Reqs[interest.getNonce()].Rep_time;
		double extra = double(time_span2.count()) * steady_clock::period::num / steady_clock::period::den;
	        
	        double ov = over+extra;
	        Value o(ov);
		document.AddMember("overhead", o, allocator);

		StringBuffer strbuf;
		Writer<StringBuffer> writer(strbuf);
		document.Accept(writer);
		//std::cout << " Created nack " << strbuf.GetString() << '\n';
		return strbuf.GetString();
	}

	void
	query(Controller& controller, const ndn::Name& name, uint32_t nonce, const ::ndn::nfd::CommandOptions& options = {})
	{
		parameters param;
		param.nonce = nonce;
		param.name = name;

		controller.fetch<::ndn::nfd::FibDataset>([this, param] (const std::vector<::ndn::nfd::FibEntry>& result) {
			for (size_t prefixLen = param.name.size(); prefixLen > 0; --prefixLen) {
				for (const ::ndn::nfd::FibEntry& item : result) {
					if (item.getPrefix().size() == prefixLen && item.getPrefix().isPrefixOf(param.name)) {
						Reqs[param.nonce].mhops.clear();
						for (const ::ndn::nfd::NextHopRecord& nh : item.getNextHopRecords()) {
							Reqs[param.nonce].mhops.push_back(nh.getFaceId());
						};
					}

				}
			}
			std::string aa;
			// Create N corresponding interests
			for(unsigned int i=0; i<Reqs[param.nonce].mhops.size(); i++){
				::ndn::Interest I;
				aa = "Key-TID" + std::to_string(number());
				::ndn::Name n = "/Trace/M/"+ Reqs[param.nonce].p2 + Reqs[param.nonce].Lname+ "/" + aa + "/" + std::to_string(Reqs[param.nonce].mhops[i]) ;  // attaching the face id as x
				I.setName(n);
				I.setNonce(param.nonce);
				I.setInterestLifetime(::ndn::time::milliseconds(10000));
				I.setMustBeFresh(true);
				Reqs[param.nonce].m_exptime[getcomp(I, 2)]=steady_clock::now();
				a_face.expressInterest(I,
						bind(&TraceBolt::onData, this,  _1, _2),
						bind(&TraceBolt::onNack, this, _1, _2),
						bind(&TraceBolt::onTimeout, this, _1));
			}
		},

		bind([]{ std::cout << "failure\n";}),
		options);

	}

protected:
	struct parameters{
		ndn::Name name;
		uint32_t nonce;
	};

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
    std::string Reply;
    std::vector<uint64_t> mhops; // multipath's possible next hops
    std::map<std::string, std::string> m_reply; //multi path replies
    std::map<std::string, steady_clock::time_point> m_exptime; //multi path express
    std::map<std::string, steady_clock::time_point> m_reptime; //multi path replies
    std::map<std::string, double> m_over; //multi path replies
  };
	// Request Information
  struct mprocess{
    steady_clock::time_point Express_time;
    steady_clock::time_point Rep_time;
    double overhead;
    std::string Reply;
  };
  // map to match a request (nonce) with an object of struct
  std::map<uint32_t, Quest> Reqs;
  ::ndn::Face a_face;
  ::ndn::KeyChain& m_keyChain;
  std::string data_a;
  std::string data_d;
  std::set<uint32_t> loop;
  steady_clock::time_point Ts;
  steady_clock::time_point To;
  ::ndn::Scheduler m_scheduler;

};

} // namespace ndn
} // namespace ns3

#endif // NDN_DATA_BOLT_H
