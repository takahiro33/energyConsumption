// ndn-grid-topo-plugin.cpp

#include "ns3-dev/ns3/core-module.h"
#include "ns3-dev/ns3/network-module.h"
#include "ns3-dev/ns3/ndnSIM-module.h"
#include <iostream>

// Random modules
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "ns3-dev/ns3/nstime.h"
//#include <ns3-dev/ns3/applications-module.h>
//#include <ns3-dev/ns3/bridge-helper.h>
//#include <ns3-dev/ns3/csma-module.h>
//#include <ns3-dev/ns3/core-module.h>
#include <ns3-dev/ns3/mobility-module.h>
//#include <ns3-dev/ns3/point-to-point-module.h>
#include <ns3-dev/ns3/wifi-module.h>

char scenario[250] = "proactiveCaching";

NS_LOG_COMPONENT_DEFINE (scenario);
using namespace boost;
using namespace std;

namespace ns3 {

int
main(int argc, char* argv[])
{

	uint32_t NumberOfContents = 1;				// Number of Contents
	char results[250] = "results";              // Directory to place results
	double endTime = 300;                       // Number of seconds to run the simulation
	double MBps = 0.30;                         // MB/s data rate desired for applications
	int contentSize = 1000;                     // Size of content to be retrieved [MB]
	char traffic[250] = "NormalTraffic";		// "NormalTraffic","HighTraffic", "LowTraffic"
	int intFreq = 297;							// Number of Intere Packet per second (High Traffic or Low Traffic)
	double retxtime = 0.05;                     // How frequent Interest retransmission timeouts should be checked (seconds)
	int maxSeq = -1;                            // Maximum number of Data packets to request
	int csSize = 0;                      		// How big the Content Store should be (0/64/128/256)
	std::string nsTFile;    	                // Name of the NS Trace file to use
	char nsTDir[250] = "./Waypoints";           // Directory for the waypoint files

	  uint32_t nStas = 1;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("scenarios/topo-grid-3x3.txt");
  topologyReader.Read();

  int servers = 2;
	NodeContainer serverNodes;
	serverNodes.Create (servers);
  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
  ndnHelper.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", "256");	//trial
  ndnHelper.InstallAll();

  // Set BestRoute strategy
//  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  // Getting containers for the consumer/producer
  Ptr<Node> producer = Names::Find<Node>("Producer");
  NodeContainer consumerNodes;
  consumerNodes.Add(Names::Find<Node>("Consumer"));

  // Install NDN applications
	std::string prefix = "/waseda/sato";

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerZipfMandelbrot");
  consumerHelper.SetPrefix(prefix);
  consumerHelper.SetAttribute("Frequency", StringValue("100")); // 100 interests a second
  	consumerHelper.SetAttribute ("Frequency", DoubleValue (intFreq));
  	consumerHelper.SetAttribute ("Randomize", StringValue("exponential"));
  	consumerHelper.SetAttribute ("q", DoubleValue (0));
  	consumerHelper.SetAttribute ("NumberOfContents", UintegerValue(NumberOfContents));
  	consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds(1)));
  	consumerHelper.SetAttribute ("StopTime", TimeValue (Seconds(endTime)));
  	consumerHelper.SetAttribute ("RetxTimer", TimeValue (Seconds(retxtime)));
  	if (maxSeq > 0)		consumerHelper.SetAttribute ("MaxSeq", IntegerValue(maxSeq));
  consumerHelper.Install(consumerNodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue ("Time"),
                                  "Time", StringValue ("2s"),
                                  "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                  "Bounds", RectangleValue (Rectangle (0, 5.0,0.0, (nStas+1)*5.0)));
       mobility.Install (consumerNodes);

  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix(prefix);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(producer);

  // Add /prefix origins to ndn::GlobalRouter
  ndnGlobalRoutingHelper.AddOrigins(prefix, producer);

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();


  int wnodes = 2;
	NodeContainer wirelessContainer;
//	wirelessContainer.Add(Names::Find<Node>("Consumer"));

	for(int i = 0; i <wnodes; i++){
		char node[10];
		sprintf(node, "Node%d",i);
		cout << node << endl;
		wirelessContainer.Add(Names::Find<Node>(node));
	}
  // Use the Wifi Helper to define the wireless interfaces for APs
  	WifiHelper wifi;
  	wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
//  	 The N standard is apparently not completely supported in NS-3
//  	wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
//  	 The ConstantRateWifiManager only works with one rate, making issues
//  	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
//  	 The MinstrelWifiManager isn't working on the current version of NS-3
//  	wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");
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

//	// We store the Wifi AP mobility models in a map, ordered by the ssid string. Will be easier to manage when
//	// calling the modified StaMApWifiMac
//	std::map<std::string, Ptr<MobilityModel> > apTerminalMobility;

	for (int i = 0; i < wnodes; i++)
	{
		// Temporary string containing our SSID
		std::string ssidtmp("ap-" + boost::lexical_cast<std::string>(i));

		// Push the newly created SSID into a vector
		ssidV.push_back (Ssid (ssidtmp));

		// Get the mobility model for wnode i
//		Ptr<MobilityModel> tmp = (wirelessContainer.Get (i))->GetObject<MobilityModel> ();

		// Store the information into our map
//		apTerminalMobility[ssidtmp] = tmp;
	}

	NS_LOG_INFO ("------Assigning mobile terminal wireless cards------");
	NS_LOG_INFO ("------Assigning AP wireless cards------");

	// Create a Wifi station with a modified Station MAC.
	wifiMacHelper.SetType ("ns3::StaWifiMac",
							"Ssid", SsidValue (ssidV[0]),
							"ActiveProbing", BooleanValue (true));

	NetDeviceContainer wifiMTNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, Names::Find<Node>("Consumer"));

	std::vector<NetDeviceContainer> wifiAPNetDevices;
	for (int i = 0; i < wnodes; i++)
	{
		wifiMacHelper.SetType ("ns3::ApWifiMac",
						   	   "Ssid", SsidValue (ssidV[i]),
							   "BeaconGeneration", BooleanValue (true),
							   "BeaconInterval", TimeValue (Seconds (0.102)));

		wifiAPNetDevices.push_back (wifi.Install (wifiPhyHelper, wifiMacHelper, wirelessContainer.Get (i)));
	}
//	 wifiMTNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, Names::Find<Node>("Node1"));

	// Using the same calculation from the Yans-wifi-Channel, we obtain the Mobility Models for the
	// mobile node as well as all the Wifi capable nodes


  Simulator::Stop(Seconds(20.0));

  NodeContainer global = NodeContainer::GetGlobal ();
  // The failure of the link connecting consumer and router will start from seconds 10.0 to 15.0
  Simulator::Schedule(Seconds(10.0), ndn::LinkControlHelper::FailLink, global.Get(0), global.Get(1));
  Simulator::Schedule(Seconds(15.0), ndn::LinkControlHelper::UpLink, global.Get(0), global.Get(1));
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
