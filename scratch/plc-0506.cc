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
#include <string>

//#include <QApplication>


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


#define SIGNALSPREAD 28000000
//#define SINRFILE "Test/Sinr250M.txt"
#define udpBufferSize 32780
#define packets 100

using namespace ns3;

int POSITION = 10;

static void sendPacket (Ptr<Socket> socket, Ptr<Packet> p )
{
   uint32_t numSendPkt = socket->Send(p);
    NS_LOG_UNCOND ("Packet send: " <<numSendPkt);
}

int main (int argc, char *argv[])
{

        PLC_Time::SetTimeModel(50, MicroSeconds(735)); // 50Hz main frequency, symbol length 735us (G3) (defines time granularity)

	// Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
//    sm = smHelper.GetSpectrumModel(2e6, 30e6, 1146);
    sm = smHelper.GetSpectrumModel(0, 500e3, 5);

    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-6; // -60dBm/Hz

    // Create cable types
	Ptr<PLC_Cable> mainCable = CreateObject<PLC_NAYY150SE_Cable> (sm);

	// Node shunt impedance
	Ptr<PLC_ConstImpedance> shuntImp = Create<PLC_ConstImpedance> (sm, PLC_Value(50, 0));

	Ptr<PLC_Graph> graph = CreateObject<PLC_Graph> ();

	Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
	Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();

        PLC_NodeList nodes;
        nodes.push_back(n1);
        nodes.push_back(n2);

        //Give the nodes a position in the network :: measurements are in m (Max 10000)
	n1->SetPosition(0,0,0);
        n2->SetPosition(0,POSITION,0);

        n1->SetName("Node1");
        n2->SetName("Node2");

	graph->AddNode(n1);
	graph->AddNode(n2);

        //Link nodes
	CreateObject<PLC_Line> (mainCable, n1, n2);

        // Set up channel
        PLC_ChannelHelper channelHelper(sm);
        channelHelper.Install(nodes);
        Ptr<PLC_Channel> channel = channelHelper.GetChannel();

        //Setting the impedance sources with outlets
        //  Default:   Ptr<PLC_Outlet> outletx = CreateObject<PLC_Outlet> (nx); //No impedance
        // Constant:   Ptr<PLC_Outlet>outletx = CreateObject<PLC_Outlet> (nx,Create<PLC_ConstImpedance>(sm,PLC_Value(Re,Im)));
        // Frequency:  Ptr<PLC_Outlet> outletx = CreateObject<PLC_Outlet>(nx, Create<PLC_FreqSelectiveValue>(sm, R, Q, omega_0));
        Ptr<PLC_Outlet> outlet1 = CreateObject<PLC_Outlet> (n1, Create<PLC_ConstImpedance> (sm, PLC_Value(100,0)));
        Ptr<PLC_Outlet> outlet2 = CreateObject<PLC_Outlet> (n2, Create<PLC_ConstImpedance> (sm, PLC_Value(40,0)));

        //Setting up the interface
        Ptr<PLC_TxInterface> txIf = CreateObject<PLC_TxInterface>(n1,sm);
        Ptr<PLC_RxInterface> rxIf = CreateObject<PLC_RxInterface>(n2,sm);

        //Setting up the transmitter and receiver
        channel->AddTxInterface(txIf);
        channel->AddRxInterface(rxIf);

        //Magic transfer function is calculated
	channel->InitTransmissionChannels();
	channel->CalcTransmissionChannels();

        //The receive power spectral density computation is done by the channel
        //transfer implementation from TX interface to Rx interface
        Ptr<PLC_ChannelTransferImpl> chImpl = txIf->GetChannelTransferImpl(PeekPointer(rxIf));
        NS_ASSERT(chImpl);
        Ptr<SpectrumValue> rxPsd = chImpl->CalculateRxPowerSpectralDensity(txPsd);
        PLC_Interference interference;

        Ptr<SpectrumValue> noiseFloor = Create<SpectrumValue>(sm);
        (*noiseFloor) = 15e-9;

        // Add netdevice
        PLC_NetDeviceHelper deviceHelper(sm, txPsd, nodes);
        deviceHelper.SetNoiseFloor(noiseFloor);
        deviceHelper.Setup();
        PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();

        interference.SetNoiseFloor(noiseFloor);
        interference.InitializeRx(rxPsd);
        Ptr<const SpectrumValue> sinr = interference.GetSinr(); //gia tri nay duoc luu tru de su dung sau nay

        //Get ns-3 nodes
       NodeContainer n = deviceHelper.GetNS3Nodes();

        //Install ipv4 stack on nodes
        InternetStackHelper inetHelper;
        inetHelper.SetIpv4StackInstall(1); // cai nay la sao ???
        inetHelper.Install (n);

        // Get Netdevice
        NetDeviceContainer d = deviceHelper.GetNetDevices();
        //+++ Assign IP address+++//
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0","255.255.255.0");

        Ipv4InterfaceContainer i = address.Assign(d);

        //+++Create UDP sockets+++//
//        NS_LOG_UNCOND("Doing ... UDP");
       TypeId socketTid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//       source = ns.network.Socket.CreateSocket (n.Get(0), socketTid);
       Ptr<Socket> source = Socket::CreateSocket(n.Get(0),socketTid);
       Ptr<Socket> sink = Socket::CreateSocket(n.Get(1),socketTid);

       InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(),4711);
       InetSocketAddress remote = InetSocketAddress (i.GetAddress(1),4711);
       sink->Bind(local);
       sink->Listen();

       source->Connect(remote);
//NS_LOG_UNCOND("Done...");
       //Create packet to send
       Ptr<Packet> p = Create<Packet>(1024);

//	Simulator::Schedule(Seconds(0.5), &PLC_Outlet::SetImpedance, outlet7, Create<PLC_TimeVariantFreqSelectiveImpedance> (PLC_ConstImpedance(sm, PLC_Value(5,0))), true);
//	Simulator::Schedule(Seconds(1), &PLC_Outlet::SetImpedance, outlet3, Create<PLC_ConstImpedance> (sm, PLC_Value(5,0)), true);
//	Simulator::Schedule(Seconds(2), &PLC_Outlet::SetImpedance, outlet6, Create<PLC_ConstImpedance> (sm, PLC_Value(5,0)), true);
        Simulator::Schedule(Seconds(1), &PLC_NetDevice::Send, devMap["Node1"], p, devMap["Node2"]->GetAddress(),0);
//        ns.core.Simulator.Schedule(ns.core.Seconds(0), sendPacket, source, packet)

        Simulator::Schedule(Seconds(0),&sendPacket,source,p);
//        Simulator::Schedule(Seconds(10),&ReceivePacket,sink);


	Simulator::Run();
        NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);

	Simulator::Destroy();

	return EXIT_SUCCESS;
}
