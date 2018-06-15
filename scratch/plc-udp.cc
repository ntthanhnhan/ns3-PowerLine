/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University of British Columbia, Vancouver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Schloegl <alexander.schloegl@gmx.de>
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>

#include "../build/ns3/nstime.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/packet.h"
#include "../build/ns3/output-stream-wrapper.h"
#include "../build/ns3/gnuplot.h"
#include "../build/ns3/trace-source-accessor.h"
#include "../build/ns3/plc.h"
#include "../build/ns3/log.h"
#include "../build/ns3/core-module.h"
#include "../build/ns3/applications-module.h"
#include "../build/ns3/network-module.h"
#include "../build/ns3/internet-module.h"
#include "../build/ns3/point-to-point-module.h"
#include "../build/ns3/ipv4-global-routing-helper.h"
#include "../build/ns3/netanim-module.h"
#include "../build/ns3/simple-net-device-helper.h"


using namespace ns3;

void
ReceivedACK(void)
{
    NS_LOG_UNCOND(Simulator::Now() << ": ACK received!");
}

int main (int argc, char *argv[])
{
    // Initialize physical environment
    PLC_Time::SetTimeModel(60, MicroSeconds(735)); // 60Hz main frequency, symbol length 735us (G3) (defines time granularity)

    // Define spectrum model
PLC_SpectrumModelHelper smHelper;
Ptr<const SpectrumModel> sm;
sm = smHelper.GetG3SpectrumModel();

Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
(*txPsd) = 1e-8; // -50dBm/Hz

// Create cable types
    Ptr<PLC_Cable> mainCable = CreateObject<PLC_NAYY150SE_Cable> (sm);
    Ptr<PLC_Cable> houseCable = CreateObject<PLC_NAYY50SE_Cable> (sm);

    // Node shunt impedance
    Ptr<PLC_ConstImpedance> shuntImp = Create<PLC_ConstImpedance> (sm, PLC_Value(50, 0));

    Ptr<PLC_Graph> graph = CreateObject<PLC_Graph> ();

    Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();
    PLC_NodeList nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);


    n1->SetPosition(0,0,0);
    n2->SetPosition(10,0,0);

    n1->SetName("Outlet1");
    n2->SetName("Outlet2");

    graph->AddNode(n1);
    graph->AddNode(n2);

    CreateObject<PLC_Line> (mainCable, n1, n2);

    Ptr<PLC_Channel> channel = CreateObject<PLC_Channel> ();
    channel->SetGraph(graph);

    Ptr<PLC_Outlet> outlet1 = CreateObject<PLC_Outlet> (n1, Create<PLC_ConstImpedance> (sm, PLC_Value(50,0)));
    Ptr<PLC_Outlet> outlet2 = CreateObject<PLC_Outlet> (n2, Create<PLC_ConstImpedance> (sm, PLC_Value(50,0)));

    channel->AddTxInterface(CreateObject<PLC_TxInterface> (n1, sm));
    channel->AddRxInterface(CreateObject<PLC_RxInterface> (n2, sm));

    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();
    // Add netdevice
    PLC_NetDeviceHelper deviceHelper(sm, txPsd, nodes);

    Simulator::Schedule(Seconds(0.5), &PLC_Outlet::SetImpedance, outlet1, Create<PLC_TimeVariantFreqSelectiveImpedance> (PLC_ConstImpedance(sm, PLC_Value(5,0))), true);
    //Get ns-3 nodes
    NodeContainer n = deviceHelper.GetNS3Nodes();

    // Get Netdevice
    NetDeviceContainer d = deviceHelper.GetNetDevices();

    //Install ipv4 stack on nodes
    InternetStackHelper inetHelper;
    inetHelper.SetIpv4StackInstall(1); // cai nay la sao ???
    inetHelper.Install (n);

    //+++ Assign IP address+++//
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = address.Assign(d);
    //Ipv4InterfaceContainer ???


    // Create one udpServer applications on node one.

    uint16_t serverport = 4000;
    UdpServerHelper server (serverport);
    ApplicationContainer apps_server = server.Install (n.Get(1));
    apps_server.Start (Seconds (1.0));
    //          apps_server.Stop (Seconds (12.0));


    // Create one UdpClient application to send UDP datagrams from node zero to node one.

    uint32_t MaxPacketSize = 64;
    Time interPacketInterval = Seconds (2.0); // 200 packets per seconds
    uint32_t maxPacketCount = 200000;
    UdpClientHelper client (i.GetAddress(1), serverport);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    ApplicationContainer apps_client = client.Install (n.Get(0));
    apps_client.Start (Seconds (2.0));
    //          apps_client.Stop (Seconds (12.0));



    Simulator::Run();
//    NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);


    // Trace packet received
    Ptr <UdpServer> serverUDP =  server.GetServer();
    uint64_t numPktRev  = serverUDP->GetReceived();
    NS_LOG_UNCOND("Number of received packet is: " << numPktRev <<"\n");
    // Cleanup simulation
    Simulator::Destroy();

    return EXIT_SUCCESS;
}
