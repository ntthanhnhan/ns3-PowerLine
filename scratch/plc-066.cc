﻿
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
uint32_t IniPkt = 2000;
//using namespace std;
using namespace ns3;

int main (int argc, char *argv[])
{

    /*Step:1..............................Init at the begining................................................................*/
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));


    /*Step:2..............................This part is set up PLC nodes (PLC_Router_1 and PLC_Router_2).........................*/
    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
//    sm = smHelper.GetG3SpectrumModel();
    sm = smHelper.GetSpectrumModel(0, 500e3, 5);

    // Define txPsd
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // Define PLC cable types
    Ptr<PLC_Cable> plc_cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

    // Create PLC_Router nodes
    Ptr<PLC_Node> plc_router_node_1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> plc_router_node_2 = CreateObject<PLC_Node> ();
    plc_router_node_1->SetPosition(0,0,0);
    plc_router_node_2->SetPosition(10,0,0);
    plc_router_node_1->SetName("PLC_Router_1");
    plc_router_node_2->SetName("PLC_Router_2");

    // put nodes to the list
    PLC_NodeList plc_nodelist;
    plc_nodelist.push_back(plc_router_node_1);
    plc_nodelist.push_back(plc_router_node_2);

    // Link PLC nodes
    CreateObject<PLC_Line> (plc_cable, plc_router_node_1, plc_router_node_2);

    // Set up channel
    PLC_ChannelHelper channelHelper(sm);
    channelHelper.Install(plc_nodelist);
    Ptr<PLC_Channel> channel = channelHelper.GetChannel();

    // Add net devices
    PLC_NetDeviceHelper plc_deviceHelper(sm, txPsd, plc_nodelist);
    plc_deviceHelper.Setup();
    PLC_NetdeviceMap devMap = plc_deviceHelper.GetNetdeviceMap();
    NetDeviceContainer DEV_Router1_Router2 = plc_deviceHelper.GetNetDevices();

    //Link Netdevice with channel
    channel->AddDevice(DEV_Router1_Router2.Get(0));
    channel->AddDevice(DEV_Router1_Router2.Get(1));



    // Calculate channels
    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();

    //Get ns-3 nodes from netdevices
    NodeContainer n = plc_deviceHelper.GetNS3Nodes();
    Ptr<Node> router_node_1 = n.Get(0);
    Ptr<Node> router_node_2 = n.Get(1);



    /*Step:3..............................This part is to set up PCs node.........................*/
    // Create PC nodes
    NodeContainer PC1_NODE_CONTAINER;
    PC1_NODE_CONTAINER.Create(1);
    Ptr<Node> PC1_NODE = PC1_NODE_CONTAINER.Get (0);

    NodeContainer PC2_NODE_CONTAINER;
    PC2_NODE_CONTAINER.Create(1);
    Ptr<Node> PC2_NODE = PC2_NODE_CONTAINER.Get (0);

    // Add links
    PointToPointHelper PC1_Router1_LINK;
    PC1_Router1_LINK.SetDeviceAttribute ("DataRate", StringValue("100Mbps"));
    PC1_Router1_LINK.SetChannelAttribute ("Delay", StringValue("1ms"));
    PC1_Router1_LINK.SetDeviceAttribute ("Mtu", UintegerValue(1500));
    PointToPointHelper PC2_Router2_LINK;
    PC2_Router2_LINK.SetDeviceAttribute ("DataRate", StringValue("100Mbps"));
    PC2_Router2_LINK.SetChannelAttribute ("Delay", StringValue("1ms"));
    PC2_Router2_LINK.SetDeviceAttribute ("Mtu", UintegerValue(1500));

    // Add netdevices to link nodes
    NetDeviceContainer DEV_PC1_Router1 = PC1_Router1_LINK.Install(PC1_NODE,router_node_1);
    NetDeviceContainer DEV_PC2_Router2 = PC2_Router2_LINK.Install(router_node_2,PC2_NODE);



    /*Step:4..............................This part is to add internet stack and route nodes.........................*/
    //Add internet stack to all nodes.
    InternetStackHelper internet;
    internet.InstallAll ();

    // Assign IPv4 addresse.
    Ipv4AddressHelper IPv4;
    IPv4.SetBase("10.10.10.0", "255.255.255.0");
    Ipv4InterfaceContainer network10 = IPv4.Assign(DEV_PC1_Router1);
    IPv4.SetBase("20.20.20.0", "255.255.255.0");
    Ipv4InterfaceContainer network20 = IPv4.Assign(DEV_Router1_Router2);
    IPv4.SetBase("30.30.30.0", "255.255.255.0");
    Ipv4InterfaceContainer network30 = IPv4.Assign(DEV_PC2_Router2);

    // Get IPv4 Object for nodes
    Ptr<Ipv4> pc1_ipv4_object = PC1_NODE->GetObject<Ipv4>();
    Ptr<Ipv4> pc2_ipv4_object = PC2_NODE->GetObject<Ipv4>();
    Ptr<Ipv4> router1_ipv4_object = router_node_1->GetObject<Ipv4>();
    Ptr<Ipv4> router2_ipv4_object = router_node_2->GetObject<Ipv4>();

    // Use Globalroutinghelper to route nodes
    //Ipv4GlobalRoutingHelper GlobalRouting;

    //STATIC ROUTE
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingr1 = ipv4RoutingHelper.GetStaticRouting (router1_ipv4_object);
    staticRoutingr1->SetDefaultRoute(Ipv4Address ("20.20.20.2"), 2);

    Ptr<Ipv4StaticRouting> staticRoutingr2 = ipv4RoutingHelper.GetStaticRouting (router2_ipv4_object);
    staticRoutingr2->AddHostRouteTo(Ipv4Address ("30.30.30.2"), Ipv4Address ("20.20.20.2"), 2);


    NS_LOG_UNCOND("Done Route from PC1 to PC2");

    /*Step:5..............................This part is to install UDP appication for sending packets........................*/
    // Create one udpServer applications on node PC1.
    uint16_t serverport = 4000;
    UdpServerHelper server (serverport);
    ApplicationContainer apps_server = server.Install (PC2_NODE);
    apps_server.Start (Seconds (1.0));
//    apps_server.Stop (Seconds (12.0));

    // Create one UdpClient application to send UDP datagrams from node PC1 to node PC2.
    uint32_t MaxPacketSize = 1470;
    Time interPacketInterval = Seconds (1.0); // 200 packets per seconds
    uint32_t maxPacketCount = IniPkt;
    UdpClientHelper client (network30.GetAddress(1), serverport);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    ApplicationContainer apps_client = client.Install (PC1_NODE);
    apps_client.Start (Seconds (2.0));
//    apps_client.Stop (Seconds (12.0));
// NS3@teamwork

    /*Step:6..............................This part is for tracing purpose........................*/                                                 //                                                                       //

//    // Trace routing tables
//    Ipv4GlobalRoutingHelper GlobalRouting;
//    GlobalRouting.PopulateRoutingTables ();
//    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("routing-table4", std::ios::out);
//    GlobalRouting.PrintRoutingTableAllAt (Seconds (12.0), routingStream);

//    // Ascii tracing
//    AsciiTraceHelper ascii;
//    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("plc-route.tr");
//    PC1_Router1_LINK.EnableAsciiAll (stream);
//    PC2_Router2_LINK.EnableAsciiAll (stream);
//    //internet.EnableAsciiIpv4All (stream);

//    // Enable PCAP capturing
//    PC1_Router1_LINK.EnablePcapAll("plcroute_pc1r1",false);
//    PC2_Router2_LINK.EnablePcapAll("plcroute_pc2r2",false);

//    Generate the animation XML trace file
//    AnimationInterface anim ("anim_plc-route1.xml");

//    Setting the location of nodes
//    anim.SetConstantPosition(PC1_NODE,25.0,50.0);
//    anim.SetConstantPosition(router_node_1,50.0,50.0);
//    anim.SetConstantPosition(router_node_2,75.0,50.0);
//    anim.SetConstantPosition(PC2_NODE,100.0,50.0);

    Simulator::Run ();
    // Trace packet received
    Ptr <UdpServer> serverUDP =  server.GetServer();
    uint64_t numPktRev  = serverUDP->GetReceived();
    NS_LOG_UNCOND("Number of received packet is: " << numPktRev <<"\n");
    uint64_t numPktLos  = serverUDP->GetLost();
    NS_LOG_UNCOND("Number of lost packet is: " << numPktLos <<"\n");


    Simulator::Destroy ();
      return 0;







}
