
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>

#include "../build/ns3/core-module.h"
#include "../build/ns3/nstime.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/packet.h"
#include "../build/ns3/output-stream-wrapper.h"
#include "../build/ns3/gnuplot.h"
#include "../build/ns3/trace-source-accessor.h"
#include "../build/ns3/nstime.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/packet.h"
#include "../build/ns3/output-stream-wrapper.h"
#include "../build/ns3/gnuplot.h"
#include "../build/ns3/trace-source-accessor.h"
#include "../build/ns3/plc.h"
#include "../build/ns3/plc-cable.h"
#include "../build/ns3/plc-noise.h"
#include "../build/ns3/log.h"
#include "../build/ns3/core-module.h"
#include "../build/ns3/applications-module.h"
#include "../build/ns3/network-module.h"
#include "../build/ns3/internet-module.h"
#include "../build/ns3/point-to-point-module.h"
#include "../build/ns3/ipv4-global-routing-helper.h"
#include "../build/ns3/netanim-module.h"
#include "../build/ns3/simple-net-device-helper.h"
#include "../build/ns3/ipv4-static-routing.h"
#include "../build/ns3/global-router-interface.h"
#include "../build/ns3/plc.h"
#include "../src/internet/model/ipv4-global-routing.h"

using namespace std;
using namespace ns3;

// Define global parameters
std::ofstream NumPktRevLogFile;   // Log of number received packets

uint64_t iNumPktSent = 30;
uint64_t iNumPktRev = 0;

void RxByteCouter( Ptr<const Packet> packet, const Address & from);
int Str2Int (std::string STRING);



int main (int argc, char *argv[])
{
    CommandLine cmd;
    //cmd.AddValue("seed_Number","Seed num là: ",seed_Number);

    cmd.Parse (argc,argv);


    /*Step:1..............................Init at the begining................................................................*/
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
    NumPktRevLogFile.open ("NumPktRevLogFile.csv");

    /*Step:2..............................This part is set up PLC nodes (PLC_Router_1 and PLC_Router_2).........................*/
    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
    sm = smHelper.GetG3SpectrumModel();

    // Define txPsd
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // Define PLC cable types
    Ptr<PLC_Cable> plc_cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

    // Create PLC_Router nodes
    Ptr<PLC_Node> plc_1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> plc_2 = CreateObject<PLC_Node> ();
    plc_1->SetPosition(0,0,0);
    plc_2->SetPosition(10,0,0);
    plc_1->SetName("PLC1");
    plc_2->SetName("PLC2");

    // put nodes to the list
    PLC_NodeList plc;
    plc.push_back(plc_1);
    plc.push_back(plc_2);

    // Link PLC nodes
    CreateObject<PLC_Line> (plc_cable, plc_1, plc_2);

    // Set up channel
    PLC_ChannelHelper channelHelper(sm);
    channelHelper.Install(plc);
    Ptr<PLC_Channel> channel = channelHelper.GetChannel();

    // Add net devices
    PLC_NetDeviceHelper plc_deviceHelper(sm, txPsd, plc);
    plc_deviceHelper.Setup();
    PLC_NetdeviceMap devMap = plc_deviceHelper.GetNetdeviceMap();

    // Calculate channels
    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();

    //Get ns-3 nodes from netdevices
    NodeContainer n = plc_deviceHelper.GetNS3Nodes();
    NetDeviceContainer d = plc_deviceHelper.GetNetDevices();
    // Add links
    PointToPointHelper link_PLC1_PLC2;
    link_PLC1_PLC2.SetDeviceAttribute ("DataRate", StringValue("1Mbps"));

    // Add netdevices to link nodes
//    NetDeviceContainer DEV_PLC1_PLC2 = link_PLC1_PLC2.Install(plc_1,plc_2);

    /*Step:4..............................This part is to add internet stack and route nodes.........................*/
    //Add internet stack to all nodes.
    InternetStackHelper internet;
    internet.InstallAll ();

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0","255.255.255.0");
     Ipv4InterfaceContainer i = address.Assign(d);

    /*Step:5..............................This part is to install UDP appication for sending packets........................*/
    // Create one udpServer applications on node PC1.
    uint16_t serverport = 4000;
    UdpServerHelper server (serverport);
    ApplicationContainer apps_server = server.Install(n.Get(1));
    apps_server.Start (Seconds (1.0));

    // Create one UdpClient application to send UDP datagrams from node PC1 to node PC2.
    uint32_t MaxPacketSize = 1024; // packet size 1024 bytes
    Time interPacketInterval = Seconds (0.5); // 1280 packet per second, equal to 10Mbps
    uint32_t maxPacketCount = iNumPktSent;
    UdpClientHelper client (i.GetAddress(1), serverport);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    ApplicationContainer apps_client = client.Install (n.Get(0));
    apps_client.Start (Seconds (2.0));
    apps_client.Stop (Seconds (10.0));

    Simulator::Run ();
    // Trace packet received
    Ptr <UdpServer> serverUDP =  server.GetServer();
    uint64_t numPktRev  = serverUDP->GetReceived();
//    uint64_t sendPkt = serverUDP->GetPacketWindowSize();


//    NS_LOG_UNCOND ("aaa "<<sendPkt);
    NumPktRevLogFile<< Simulator::Now ().GetMicroSeconds () << "," << numPktRev << '\n';
    NS_LOG_UNCOND("Number of received packet is: " << numPktRev <<"\n");
    Simulator::Destroy ();
      return 0;

}


void RxByteCouter( Ptr<const Packet> packet, const Address & from)
{
  iNumPktRev = iNumPktRev + 1;
  NumPktRevLogFile<< Simulator::Now ().GetMicroSeconds () << "," << packet->GetSize() << '\n';

  //NS_LOG_UNCOND(packet->GetSize());
  //NS_LOG_UNCOND("Numpkt:" <<iNumPktRev <<"\n");
}


int Str2Int (std::string STRING){
    int INT_t = atoi(STRING.c_str());
    return INT_t;

}

