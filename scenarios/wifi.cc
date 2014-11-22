/* -*- Mode:C++; c-file-style:"gnu"; -*- */
/*
 * icc-scenario.cc
 *  Sector walk for ndnSIM
 *
 * Copyright (c) 2014 Waseda University, Sato Laboratory
 * Author: Jairo Eduardo Lopez <jairo@ruri.waseda.jp>
 *         Takahiro Miyamoto <mt3.mos@gmail.com>
 *
 * Special thanks to University of Washington for initial templates
 *
 *  icc-scenario.cc is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  icc-scenario.cc is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero Public License for more details.
 *
 *  You should have received a copy of the GNU Affero Public License
 *  along with icc-scenario.cc.  If not, see <http://www.gnu.org/licenses/>.
 */

// Standard C++ modules
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <iostream>
#include <map>
#include <string>
#include <sys/time.h>
#include <vector>

// Random modules
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/variate_generator.hpp>

// ns3 modules
#include <ns3-dev/ns3/applications-module.h>
#include <ns3-dev/ns3/bridge-helper.h>
#include <ns3-dev/ns3/csma-module.h>
#include <ns3-dev/ns3/core-module.h>
#include <ns3-dev/ns3/mobility-module.h>
#include <ns3-dev/ns3/network-module.h>
#include <ns3-dev/ns3/point-to-point-module.h>
#include <ns3-dev/ns3/wifi-module.h>
#include <ns3-dev/ns3/ipv4-address-helper.h>

// ndnSIM modules
#include <ns3-dev/ns3/ndnSIM-module.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-rate-l3-tracer.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-seqs-app-tracer.h>

using namespace ns3;
using namespace boost;
using namespace std;
using namespace ndn;
using namespace fib;

namespace br = boost::random;

// Pit Entries
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-impl.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-impl.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-incoming-face.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-outgoing-face.h>


typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec*1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1)-TIMER_SECONDS (_t2))

//char scenario[250] = "ICCScenario";
char scenario[250] = "proactiveCaching";

NS_LOG_COMPONENT_DEFINE (scenario);

// Number generator
br::mt19937_64 gen;

// Adding Vectors
inline Vector3D operator+(const Vector3D& u,const Vector3D& v){
	Vector3D w;
	w.x=u.x+v.x;
	w.y=u.y+v.y;
	w.z=u.z+v.z;
	return w;
}

// Obtains a random number from a uniform distribution between min and max.
// Must seed number generator to ensure randomness at runtime.
int obtain_Num(int min, int max)
{
	br::uniform_int_distribution<> dist(min, max);
	return dist(gen);
}

// Obtain a random double from a uniform distribution between min and max.
// Must seed number generator to ensure randomness at runtime.
double obtain_Num(double min, double max)
{
	br::uniform_real_distribution<> dist(min, max);
	return dist(gen);
}

// Function to change the SSID of a Node, depending on distance
void SetSSIDviaDistance(uint32_t mtId, Ptr<MobilityModel> node, std::map<std::string, Ptr<MobilityModel> > aps)
{
	char configbuf[250];
	char buffer[250];

	// This causes the device in mtId to change the SSID, forcing AP change
	sprintf(configbuf, "/NodeList/%d/DeviceList/0/$ns3::WifiNetDevice/Mac/Ssid", mtId);

	std::map<double, std::string> SsidDistance;

	// Iterate through the map of seen Ssids
	for (std::map<std::string, Ptr<MobilityModel> >::iterator ii=aps.begin(); ii!=aps.end(); ++ii)
	{
		// Calculate the distance from the AP to the node and save into the map
		SsidDistance[node->GetDistanceFrom((*ii).second)] = (*ii).first;
	}

	double distance = SsidDistance.begin()->first;
	std::string ssid(SsidDistance.begin()->second);

	sprintf(buffer, "Change to SSID %s at distance of %f", ssid.c_str(), distance);

	NS_LOG_INFO(buffer);

	// Because the map sorts by std:less, the first position has the lowest distance
	Config::Set(configbuf, SsidValue(ssid));

	// Empty the maps
	SsidDistance.clear();
}

// Function to change the SSID of a node's Wifi netdevice
void
SetSSID (uint32_t mtId, uint32_t deviceId, Ssid ssidName)
{
	char configbuf[250];

	sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac/Ssid", mtId, deviceId);

	Config::Set(configbuf, SsidValue(ssidName));
}

void
PeriodicPitPrinter (Ptr<Node> node)
{
	Ptr<ndn::Pit> pit = node->GetObject<ndn::Pit> ();

	cout << "Node: " << node->GetId () << endl;

	for (Ptr<ndn::pit::Entry> entry = pit->Begin (); entry != pit->End (); entry = pit->Next (entry))
	{
		ndn::pit::Entry::in_container incoming = entry->GetIncoming ();
		ndn::pit::Entry::out_container outgoing = entry->GetOutgoing ();

		cout << "In: ";
		bool first = true;
		BOOST_FOREACH (const ndn::pit::IncomingFace &face, incoming)
		{
			if (!first)
				cout << ",";
			else
				first = false;

			cout << *face.m_face;
		}

		cout << "\nOut: ";
		first = true;
		BOOST_FOREACH (const ndn::pit::OutgoingFace &face, outgoing)
		{
			if (!first)
				cout << ",";
			else
				first = false;

			cout << *face.m_face;
		}

		cout << entry->GetPrefix () << "\t" << entry->GetExpireTime () << endl;
	}
}

int main (int argc, char *argv[])
{
	// These are our scenario arguments
	uint32_t aggregator = 1;                    // Number of aggregation nodes
	uint32_t sectors = 3;                       // Number of wireless sectors
	uint32_t aps = 3;			     			// Number of wireless access nodes in a sector
	uint32_t trainAp = 2;			      		// Number of train wagons
	uint32_t users = 4;			      			// Number of mobile terminals
	uint32_t servers = 3;			      		// Number of servers in the network
	uint32_t xaxis = 300;                       // Size of the X axis
	uint32_t yaxis = 300;                       // Size of the Y axis
	uint32_t NumberOfContents = 1000;				// Number of Contents
	double sec = 0.0;							// Movement start
	double waitint = 1.0;						// Wait at AP
	double travelTime = 3.0;					// Travel time within APs
	bool traceFiles = false;                    // Tells to run the simulation with traceFiles
	bool smart = true;                          // Tells to run the simulation with SmartFlooding
	bool bestr = false;                         // Tells to run the simulation with BestRoute
	char results[250] = "results";              // Directory to place results
	double endTime = 300;                       // Number of seconds to run the simulation
	double MBps = 0.30;                         // MB/s data rate desired for applications
	int contentSize = 100;            // Size of content to be retrieved [MB]
	char traffic[250] = "NormalTraffic";		// "NormalTraffic","HighTraffic", "LowTraffic"
	int intFreq = 297;							// Number of Intere Packet per second (High Traffic or Low Traffic)
	int maxSeq = 1000;				    // Maximum number of Data packets to request
	double retxtime = 0.05;                     // How frequent Interest retransmission timeouts should be checked (seconds)
	int csSize = 64;                      		// How big the Content Store should be (0/64/128/256)
	std::string nsTFile;    	                // Name of the NS Trace file to use
	char nsTDir[250] = "./Waypoints";           // Directory for the waypoint files
	string prefix = "/waseda/sato";
	
	// Variable for buffer
	char buffer[250];

	CommandLine cmd;
	cmd.AddValue ("mobile", "Number of mobile terminals in simulation", users);
	cmd.AddValue ("servers", "Number of servers in the simulation", servers);
	cmd.AddValue ("results", "Directory to place results", results);
	cmd.AddValue ("contents", "Number of Contents", NumberOfContents);
	cmd.AddValue ("start", "Starting second", sec);
	cmd.AddValue ("waitint", "Wait interval between APs", waitint);
	cmd.AddValue ("travel", "Travel time between APs", travelTime);
	cmd.AddValue ("trace", "Enable trace files", traceFiles);
	cmd.AddValue ("smart", "Enable SmartFlooding forwarding", smart);
	cmd.AddValue ("bestr", "Enable BestRoute forwarding", bestr);
	cmd.AddValue ("contents", "Number of contents in simulation", NumberOfContents);
	cmd.AddValue ("csSize", "Number of Interests a Content Store can maintain (-1:IP)", csSize);
	cmd.AddValue ("endTime", "How long the simulation will last (Seconds)", endTime);
	cmd.AddValue ("mbps", "Data transmission rate for NDN App in MBps", MBps);
	cmd.AddValue ("size", "Content size in MB (-1 is for no limit)", contentSize);
	cmd.AddValue ("traffic", "\"NormalTraffic\",\"HighTraffic\", \"LowTraffic\"", traffic);
	cmd.AddValue ("retx", "How frequent Interest retransmission timeouts should be checked in seconds", retxtime);
	cmd.AddValue ("traceFile", "Directory coModentaining Ns2 movement trace files (Usually created by Bonnmotion)", nsTDir);
	cmd.Parse (argc,argv);

	uint32_t wnodes = aps * sectors;            // Number of nodes in the network
	int cacheSize;
	cacheSize = csSize * 1024 *1024;

	if(traffic == "HighTraffic") intFreq = 500;
	else if(traffic == "LowTraffic") intFreq = 150;
	else intFreq = 300;

	cout << "endtime=" << endTime << endl;

	// Allocate all Nodes
	vector<double> serverXpos;
	vector<double> serverYpos;
	for(int i = 0; i < servers; i++)
	{
		serverXpos.push_back(350 + i * 50);
		serverYpos.push_back(-130.0);
	}

	vector<double> centralXpos;
	vector<double> centralYpos;
	for(int i = 0; i < sectors; i++)
	{
		centralXpos.push_back(200 + i * 300);
		centralYpos.push_back(-50.0);
	}

	vector<double> wirelessXpos;
	vector<double> wirelessYpos;
	for(int i = 0; i < wnodes; i++)
	{
		wirelessXpos.push_back(100 + 100 * i);
		wirelessYpos.push_back(0);
	}

	vector<double> mobileXpos;
	vector<double> mobileYpos;
	for(int i = 0; i < users + trainAp*2; i++)
	{
		if(i < users){
			mobileXpos.push_back((50 * i / users));
			mobileYpos.push_back(100);
		} else if(i < users + trainAp){
			mobileXpos.push_back(30*(i-users));
			mobileYpos.push_back(50);
		} else {
			mobileXpos.push_back(30*(i-users-trainAp));
			mobileYpos.push_back(60);
		}

	}
	

	 // What the NDN Data packet payload size is fixed to 1024 bytes
	uint32_t payLoadsize = 1024;

	// Give the content size, find out how many sequence numbers are necessary
	if (contentSize > 0)	maxSeq = 1 + (((contentSize*1000000) - 1) / payLoadsize);

	// How many Interests/second a producer creates
//	int intFreq = int((MBps * 1000000) / payLoadsize);

	NS_LOG_INFO ("------Creating nodes------");

	// Node definitions for users terminals (consumers)
	// Container for server (producer) nodes
	NodeContainer serverNodes;
	serverNodes.Create (servers);

	// Aggregation Nodes
	NodeContainer aggregatorNode;
	aggregatorNode.Create (aggregator);

	// Central Nodes
	NodeContainer centralContainer;
	centralContainer.Create (sectors);

	// TainAP Nodes
	NodeContainer trainApNodes;
	trainApNodes.Create (trainAp);

	// TainAP Nodes
	NodeContainer trainApBridgeNodes;
	trainApBridgeNodes.Create (trainAp);

	// Wireless access Nodes
	NodeContainer wirelessContainer;
	wirelessContainer.Create (wnodes);

	// User NOdes
	NodeContainer userNodes;
	userNodes.Create(users);

	std::vector<uint32_t> mobileNodeIds;

	// Save all the users Node IDs
	for (int i = 0; i < users; i++)
	{
		mobileNodeIds.push_back(userNodes.Get (i)->GetId ());
	}

	std::vector<uint32_t> trainApNodeIds;
	std::vector<uint32_t> trainApBridgeNodeIds;

	// Save all the trainAp Node IDs
	for (int i = 0; i < trainAp; i++)
	{
		trainApNodeIds.push_back(trainApNodes.Get (i)->GetId ());
		trainApBridgeNodeIds.push_back(trainApBridgeNodes.Get (i)->GetId ());
	}


	// Separate the wireless nodes into sector specific containers
	std::vector<NodeContainer> sectorNodes;

	for (int i = 0; i < sectors; i++)
	{
		NodeContainer wireless;
		for (int j = i*aps; j < aps + i*aps; j++)
		{
			wireless.Add(wirelessContainer.Get (j));
		}
		sectorNodes.push_back(wireless);
	}

	// Find out how many first level nodes we will have
	// The +1 is for the server which will be attached to the first level nodes
	int first = (sectors / 3) + 1;


	// Container for all NDN capable nodes
	NodeContainer allNdnNodes;
	allNdnNodes.Add (centralContainer);
	allNdnNodes.Add (wirelessContainer);
	allNdnNodes.Add (aggregatorNode);
	allNdnNodes.Add (trainApNodes);
	allNdnNodes.Add (trainApBridgeNodes);

	// Container for all NDN capable nodes
	NodeContainer mobileNodes;
	mobileNodes.Add (userNodes);
	mobileNodes.Add (trainApNodes);
	mobileNodes.Add (trainApBridgeNodes);

	std::vector<uint32_t> serverNodeIds;

	// Save all the users Node IDs
	for (int i = 0; i < servers; i++)
	{
		serverNodeIds.push_back(serverNodes.Get (i)->GetId ());
	}

	// Container for all nodes without NDN specific capabilities
	NodeContainer allUserNodes;
	allUserNodes.Add (userNodes);
	//allUserNodes.Add (serverNodes);

	// Make sure to seed our random
	gen.seed (std::time (0) + (long long)getpid () << 32);


	NS_LOG_INFO ("------Placing Server nodes-------");
	MobilityHelper Server;
	Ptr<ListPositionAllocator> initialServer = CreateObject<ListPositionAllocator> ();
	for (int i = 0; i < sectors; i++)
	{
		Vector pos (serverXpos[i], serverYpos[i], 0.0);
		initialServer->Add (pos);
	}
	Server.SetPositionAllocator(initialServer);
	Server.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	Server.Install(serverNodes);


	NS_LOG_INFO ("------Placing Aggregation nodes-------");
	MobilityHelper aggregatorStations;
	Ptr<ListPositionAllocator> initialAggregator = CreateObject<ListPositionAllocator> ();
	Vector posAggregator (400, -100, 0.0);
	initialAggregator->Add (posAggregator);
	aggregatorStations.SetPositionAllocator(initialAggregator);
	aggregatorStations.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	aggregatorStations.Install(aggregatorNode);


	NS_LOG_INFO ("------Placing Central nodes-------");
	MobilityHelper centralStations;
	Ptr<ListPositionAllocator> initialCenter = CreateObject<ListPositionAllocator> ();
	for (int i = 0; i < sectors; i++)
	{
		Vector pos (centralXpos[i], centralYpos[i], 0.0);
		initialCenter->Add (pos);
	}
	centralStations.SetPositionAllocator(initialCenter);
	centralStations.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	centralStations.Install(centralContainer);


	NS_LOG_INFO ("------Placing wireless access nodes------");
	MobilityHelper wirelessStations;

	Ptr<ListPositionAllocator> initialWireless = CreateObject<ListPositionAllocator> ();

	for (int i = 0; i < wnodes; i++)
	{
		Vector pos (wirelessXpos[i], wirelessYpos[i], 0.0);
		initialWireless->Add (pos);
	}
	wirelessStations.SetPositionAllocator(initialWireless);
	wirelessStations.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	wirelessStations.Install(wirelessContainer);

	NS_LOG_INFO ("------Placing mobile terminals------");
	// Place the mobile terminal above the APs and use the waypoint model
	MobilityHelper mobilityTerminals;

	Ptr<MobilityModel> mob = wirelessContainer.Get (0)->GetObject<MobilityModel>();

	mobilityTerminals.SetMobilityModel("ns3::WaypointMobilityModel");
	mobilityTerminals.Install(mobileNodes);

	Vector diff ;
	double initialSec = sec;
	for(int i = 0; i < users + trainAp*2; i++)
	{
		Ptr<WaypointMobilityModel> staWaypointMobility = DynamicCast<WaypointMobilityModel>(mobileNodes.Get (i)->GetObject<MobilityModel> ());

		sprintf(buffer, "Assigning waypoints - start: %f, pause: %f, travel: %f", sec, waitint, travelTime);

		NS_LOG_INFO (buffer);

		diff = Vector (mobileXpos[i], mobileYpos[i], 0.0);

		// Assign the waypoints for the mobile terminal
		for (int j = 0; j < wnodes; j++)
		{
				mob = wirelessContainer.Get (j)->GetObject<MobilityModel>();

				Vector wayP = mob->GetPosition () + diff;

				staWaypointMobility->AddWaypoint (Waypoint(Seconds(sec), wayP));
				staWaypointMobility->AddWaypoint (Waypoint(Seconds(sec + waitint), wayP));

				sec += waitint + travelTime;
		}
		sec = initialSec;
	}


	// Connect Wireless Nodes to central nodes
	// Because the simulation is using Wifi, PtP connections are 100Mbps
	// with 5ms delay
	NS_LOG_INFO("------Connecting Central nodes to wireless access nodes------");

	vector <NetDeviceContainer> ptpWLANCenterDevices;

	PointToPointHelper p2p_1024mbps1ms;
	p2p_1024mbps1ms.SetDeviceAttribute ("DataRate", StringValue ("1024Mbps"));
	p2p_1024mbps1ms.SetChannelAttribute ("Delay", StringValue ("1ms"));

	// Connect the central nodes to APs and
	NetDeviceContainer ptpWirelessCenterDevices;
	for (int i = 0; i < sectors; i++)
	{

		for (int j = 0; j < aps; j++)
		{
			ptpWirelessCenterDevices.Add (p2p_1024mbps1ms.Install (centralContainer.Get (i), sectorNodes[i].Get (j) ));
		}
		ptpWirelessCenterDevices.Add (p2p_1024mbps1ms.Install (centralContainer.Get (i), aggregatorNode.Get(0) ));
		ptpWLANCenterDevices.push_back (ptpWirelessCenterDevices);
	}

	// Connect the aggregation nodes to central nodes
	NetDeviceContainer ptpServerlowerNdnDevices;
	for(int i =0; i < servers; i++)
	{
		ptpServerlowerNdnDevices.Add (p2p_1024mbps1ms.Install (serverNodes.Get (i), aggregatorNode.Get(0)));
	}

	// Connect the train APs by using Bridge
	NetDeviceContainer ptpTrainBridgeDevices;
	for(int i =0; i < trainAp; i++)
	{
		ptpTrainBridgeDevices.Add (p2p_1024mbps1ms.Install (trainApNodes.Get (i), trainApBridgeNodes.Get(i)));
	}


	NS_LOG_INFO ("------Creating Wireless cards------");

	// Use the Wifi Helper to define the wireless interfaces for APs
	WifiHelper wifi;
	wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
	// The N standard is apparently not completely supported in NS-3
	//wifi.setStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
	// The ConstantRateWifiManager only works with one rate, making issues
	//wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
	// The MinstrelWifiManager isn't working on the current version of NS-3
	//wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");
	wifi.SetRemoteStationManager ("ns3::ArfWifiManager");


	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel", "ReferenceLoss",DoubleValue(40.02));
	wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

	// All interfaces are placed on the same channel. Makes AP changes easy. Might
	// have to be reconsidered for multiple mobile nodes
	YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
	wifiPhyHelper.SetChannel (wifiChannel.Create ());
	wifiPhyHelper.Set("TxPowerStart", DoubleValue(16.0206));
	wifiPhyHelper.Set("TxPowerEnd", DoubleValue(1));


	// Add a simple no QoS based card to the Wifi interfaces
	NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();

	// Create SSIDs for all the APs
	std::vector<Ssid> ssidV;

	NS_LOG_INFO ("------Creating ssids for wireless cards------");

	// We store the Wifi AP mobility models in a map, ordered by the ssid string. Will be easier to manage when
	// calling the modified StaMApWifiMac
	std::map<std::string, Ptr<MobilityModel> > apTerminalMobility;

	for (int i = 0; i < wnodes; i++)
	{
		// Temporary string containing our SSID
		std::string ssidtmp("ap-" + boost::lexical_cast<std::string>(i));

		// Push the newly created SSID into a vector
		ssidV.push_back (Ssid (ssidtmp));

		Ptr<MobilityModel> tmp;
		// Get the mobility model for wnode i
		tmp = (wirelessContainer.Get (i))->GetObject<MobilityModel> ();

		// Store the information into our map
		apTerminalMobility[ssidtmp] = tmp;
	}


	// Create SSIDs for all the TrainAPs
	NS_LOG_INFO ("------Creating ssids for wireless cards in train APs------");

	// We store the Wifi AP mobility models in a map, ordered by the ssid string. Will be easier to manage when
	// calling the modified StaMApWifiMac
	std::map<std::string, Ptr<MobilityModel> > trainApTerminalMobility;

	for (int i = 0; i < trainAp*2; i++)
	{
		// Temporary string containing our SSID
		std::string ssidtmp("ap-" + boost::lexical_cast<std::string>(i + wnodes));

		// Push the newly created SSID into a vector
		ssidV.push_back (Ssid (ssidtmp));

		// Get the mobility model for wnode i
		Ptr<MobilityModel> tmp;
		if(i<trainAp)		tmp = (trainApNodes.Get (i))->GetObject<MobilityModel> ();
		else 				tmp = (trainApBridgeNodes.Get (i-trainAp))->GetObject<MobilityModel> ();

		// Store the information into our map
		trainApTerminalMobility[ssidtmp] = tmp;
	}

	NS_LOG_INFO ("------Assigning users terminal wireless cards------");
	NS_LOG_INFO ("Assigning AP wireless cards");
	std::vector<NetDeviceContainer> wifiAPNetDevices;
	for (int i = 0; i < wnodes + trainAp*2; i++)
	{
		if(i<wnodes){
		wifiMacHelper.SetType ("ns3::StaWifiMac",
						   "Ssid", SsidValue (ssidV[i]),
						   "ActiveProbing", BooleanValue (true));
		}else {
			wifiMacHelper.SetType ("ns3::ApWifiMac",
							   "Ssid", SsidValue (ssidV[i]),
							   "BeaconGeneration", BooleanValue (true),
							   "BeaconInterval", TimeValue (Seconds (0.102)));
		}

		if(i<wnodes){ 				 wifiAPNetDevices.push_back (wifi.Install (wifiPhyHelper, wifiMacHelper, wirelessContainer.Get (i)));
	}else if (i < wnodes+trainAp){	 wifiAPNetDevices.push_back (wifi.Install (wifiPhyHelper, wifiMacHelper, trainApNodes.Get (i-wnodes)));
	}else		 					 wifiAPNetDevices.push_back (wifi.Install (wifiPhyHelper, wifiMacHelper, trainApBridgeNodes.Get (i-wnodes-trainAp)));
	}

	// Create a Wifi station with a modified Station MAC.
	wifiMacHelper.SetType("ns3::StaWifiMac",
			"Ssid", SsidValue (ssidV[0]),
			"ActiveProbing", BooleanValue (true));

	NetDeviceContainer wifiMTNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, userNodes);
//	NetDeviceContainer wifiRelayNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, trainApNodes);

	// Using the same calculation from the Yans-wifi-Channel, we obtain the Mobility Models for the
	// mobile node as well as all the Wifi capable nodes


	char routeType[250];

	// Now install content stores and the rest on the middle node. Leave
	// out clients and the users node
	NS_LOG_INFO ("------Installing NDN stack on routers------");
	ndn::StackHelper ndnHelperRouters;

	// Decide what Forwarding strategy to use depending on user command line input
	if (smart)
	{
		sprintf(routeType, "%s", "smart");
		NS_LOG_INFO ("NDN Utilizing SmartFlooding");
		ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
	}
	else if (bestr)
	{
		sprintf(routeType, "%s", "bestr");
		NS_LOG_INFO ("NDN Utilizing BestRoute");
		ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
	}
	else
	{
		sprintf(routeType, "%s", "flood");
		NS_LOG_INFO ("NDN Utilizing Flooding");
		ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::Flooding::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
	}

	// Set the Content Stores
	sprintf(buffer, "%d", cacheSize);
	if(csSize >= 0)
	{
		ndnHelperRouters.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", buffer);
	} else {
		ndnHelperRouters.SetContentStore ("ns3::ndn::cs::Nocache");
	}
	ndnHelperRouters.SetDefaultRoutes (true);
	// Install on ICN capable routers
	ndnHelperRouters.Install (allNdnNodes);
	ndnHelperRouters.Install (serverNodes);


	// Create a NDN stack for the clients and mobile node
	ndn::StackHelper ndnHelperUsers;
	// These nodes have only one interface, so BestRoute forwarding makes sense
	ndnHelperUsers.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
	// No Content Stores are installed on these machines
	ndnHelperUsers.SetContentStore ("ns3::ndn::cs::Nocache");
	ndnHelperUsers.SetDefaultRoutes (true);
	ndnHelperUsers.Install (allUserNodes);


	NS_LOG_INFO ("------Installing Producer Application------");

	sprintf(buffer, "Producer Payload size: %d", payLoadsize);
	NS_LOG_INFO (buffer);

	// Create the producer on the users node
	ndn::AppHelper producerHelper ("ns3::ndn::Producer");
	producerHelper.SetPrefix (prefix);
	producerHelper.SetAttribute ("StopTime", TimeValue (Seconds(endTime)));
	// Payload size is in bytes
	producerHelper.SetAttribute ("PayloadSize", UintegerValue(payLoadsize));
	producerHelper.Install (serverNodes);

	NS_LOG_INFO ("------Installing Consumer Application------");

	sprintf(buffer, "Consumer Interest/s frequency: %d", intFreq);
	NS_LOG_INFO (buffer);

	// Create the consumer on the randomly selected node
	ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerZipfMandelbrot");
	consumerHelper.SetPrefix (prefix);
	consumerHelper.SetAttribute ("Frequency", DoubleValue (intFreq));
	consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
	consumerHelper.SetAttribute ("q", DoubleValue (0));
	consumerHelper.SetAttribute ("NumberOfContents", UintegerValue(NumberOfContents));
	consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds(1)));
	consumerHelper.SetAttribute ("StopTime", TimeValue (Seconds(endTime)));
	consumerHelper.SetAttribute ("RetxTimer", TimeValue (Seconds(retxtime)));
	if (maxSeq > 0)		consumerHelper.SetAttribute ("MaxSeq", IntegerValue(maxSeq));
	consumerHelper.Install (userNodes);

	sprintf(buffer, "Ending time! %f", endTime+1);
	NS_LOG_INFO(buffer);

	// If the variable is set, print the trace files
	if (traceFiles) {
		char filename[250];		// Filename
		char fileId[250];		// File ID

		sprintf(results, "%s/%s/csSize_%d/MN_%d/endTime_%.0f", results, traffic, csSize ,users, endTime);
		sprintf(filename, "%s/clients", results);

		std::ofstream clientFile;

		clientFile.open (filename);
		for (int i = 0; i < mobileNodeIds.size(); i++)
		{
			clientFile << mobileNodeIds[i] << std::endl;
		}

		clientFile.close();

		// Print server nodes to file
		sprintf(filename, "%s/servers", results);

		std::ofstream serverFile;

		serverFile.open (filename);
		for (int i = 0; i < serverNodeIds.size(); i++)
		{
			serverFile << serverNodeIds[i] << std::endl;
		}

		serverFile.close();

		NS_LOG_INFO ("Installing tracers");
		
		// NDN Aggregate tracer
		printf ("Result files are being written at %s\n", results);
//		sprintf (filename, "%s/aggregate-trace", results);
//		ndn::L3AggregateTracer::InstallAll(filename, Seconds (1.0));

		// NDN L3 tracer
		sprintf (filename, "%s/rate-trace", results);
		ndn::L3RateTracer::InstallAll (filename, Seconds (1.0));

		// NDN App Tracer
		sprintf (filename, "%s/app-delays", results);
		ndn::AppDelayTracer::InstallAll (filename);
	}

	NS_LOG_INFO ("------Scheduling events - SSID changes------");

	// Schedule AP Changes
	double apsec = 0.0;
	// How often should the AP check it's distance
	double speed = 100/travelTime;
	double checkTime = 100.0/speed;
	double tmpT = 0;
	double j = apsec;
	int k = 0;

	// Settings for Bridge connection
	bool ip = false;
	bool b = false;

	if(ip){
		Ipv4AddressHelper ipAddress;
		ipAddress.SetBase("192.168.0.0", "255.255.255.0");
		ipAddress.Assign(ptpServerlowerNdnDevices);
		ipAddress.Assign(ptpWirelessCenterDevices);

		for(int i = 0; i < sectors; i++){
			ipAddress.Assign(ptpWLANCenterDevices.at(1));
		}
		for(int i = 0; i < wnodes+trainAp*2; i++){
			ipAddress.Assign(wifiAPNetDevices.at(i));
		}
		ipAddress.Assign(wifiMTNetDevices);
	}

		CsmaHelper csma;
		NetDeviceContainer csmaDevices;
		csmaDevices = csma.Install(trainApNodes);
		csmaDevices = csma.Install(trainApBridgeNodes);
/*
		if(b){
		BridgeHelper bridge;
	    NetDeviceContainer bridgeDev;
	    bridgeDev = bridge.Install (trainApNodes.Get (0),
	    		NetDeviceContainer (wifiAPNetDevices[wnodes], csmaDevices.Get(0)));
	    bridgeDev = bridge.Install (trainApBridgeNodes.Get (0),
	    		NetDeviceContainer (wifiAPNetDevices[wnodes+1], csmaDevices.Get(1)));
//	    wifiPhyHelper.EnablePcap ("wifi-wired-bridging", bridgeDev);
	    // assign AP IP address to bridge, not wifi
//	    apInterface = ipAddress.Assign (bridgeDev);


	}
*/

	//trainApNodeIds
	while ( j < endTime && k < wnodes )
	{
		sprintf(buffer, "Running event at %f", j);
		NS_LOG_INFO(buffer);

		Time torun = Seconds(j);
		Time denPush = torun - MilliSeconds(10);
		Time ssi = torun;

		if(j == apsec){
			for (int i = 0; i < users; i++)
			{
				cout << "Scheduling SSID change for user node " << i << " to " << ssidV[floor(wnodes+trainAp+i/2)] << " at " << torun << endl;
				Simulator::Schedule (ssi, &SetSSID, mobileNodeIds[i], 0, ssidV[floor(wnodes+trainAp+i/2)]);
//				sprintf(buffer, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac", mobileNodeIds[i], 0);
//				Config::Set(buffer,  SsidValue(ssidTrain[0]));
			}
		}

		for (int i = 0; i < trainAp; i++)
		{
			cout << "Scheduling SSID change for trainAP node " << i  << " to " << ssidV[k] << " at " << torun << endl;
			Simulator::Schedule (ssi, &SetSSID, trainApNodeIds[i], 1, ssidV[k]);
		}
		j += checkTime;
		tmpT = 0;
		k++;
	}


	NS_LOG_INFO ("------Ready for execution!------");

	Simulator::Stop (Seconds (endTime+1));
	Simulator::Run ();
	Simulator::Destroy ();
}
