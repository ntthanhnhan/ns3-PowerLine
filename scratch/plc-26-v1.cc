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

//static void CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd);

//void sendPacket (Ptr<Socket>, uint32_t);

using namespace ns3;

int POSITION = 10;
//uint64_t totalBytesReceived[1] = {0};
uint32_t totalBytesReceived[1]={0};// throughput(0);
//double throughput;
std::ofstream rdTrace;
uint64_t DATA_SIZE = 1e2;
double delay[1] = {0};

int Str2Int (std::string STRING){
    int INT_t = atoi(STRING.c_str());
    return INT_t;
}
//--Callback function is called whenever a packet is received successfully.
//--This function add the size of packet to totalByteReceived counter.
void ReceivePacket(std::string context, Ptr<const Packet> p)
{
    int context_i= Str2Int(context);
    totalBytesReceived[context_i] += p->GetSize();
    NS_LOG_UNCOND("Received: "<<totalBytesReceived[context_i]);
}
//--Throughput calculating function.
//--This function calculate the throughput every 0.1s using totalBytesReceived counter.
//--and write the results into a throughput tracefile
//void CalculateThroughput()
//{
//    int context_i_c = Str2Int(context_c);
//    totalBytesReceived[context_i] += p->GetSize();
//    NS_LOG_UNCOND("Received Packet: "<<totalBytesReceived[context_i]);

//    double throughput =((totalBytesReceived*8.0)/100000);
//    NS_LOG_UNCOND ("Throughput: "<<throughput);
//    totalBytesReceived = 0;
//    rdTrace << Simulator::Now().GetSeconds() << "\t"<<throughput <<"\n";
//    Simulator::Schedule(Seconds(0.1), &CalculateThroughput);

//}


int main (int argc, char *argv[])
{
    uint32_t n_plc = 2;
    CommandLine cmd;
    cmd.AddValue ("n_plc", "Number of node PLC", n_plc);
    cmd.Parse(argc,argv);
    rdTrace.open("udpPlcThroughput.csv", std::ios::out);
    rdTrace <<"# Time \t Throughput \n";

    Packet::EnablePrinting();
        PLC_Time::SetTimeModel(50, MicroSeconds(735)); // 60Hz main frequency, symbol length 735us (G3) (defines time granularity)

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

        //Setting up the channel
//	Ptr<PLC_Channel> channel = CreateObject<PLC_Channel> ();
//	channel->SetGraph(graph);
//        graph->CreatePLCGraph();


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


//        ## Create net devices
//        deviceHelper = ns.plc.PLC_NetDeviceHelper(sm, txPsd, nodes)
//        deviceHelper.DefinePhyType(ns.core.TypeId.LookupByName ('ns3::PLC_InformationRatePhy'))
//        deviceHelper.DefineMacType(ns.core.TypeId.LookupByName ('ns3::PLC_ArqMac'))
//        deviceHelper.SetHeaderModulationAndCodingScheme(ns.plc.ModulationAndCodingScheme(ns.plc.BPSK_1_4,0))
//        deviceHelper.SetPayloadModulationAndCodingScheme(ns.plc.ModulationAndCodingScheme(ns.plc.BPSK_1_2,0))
//        deviceHelper.Setup()

//        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        // Add netdevice
        PLC_NetDeviceHelper deviceHelper(sm, txPsd, nodes);
//        deviceHelper.DefinePhyType()
        deviceHelper.SetNoiseFloor(noiseFloor);
        deviceHelper.Setup();
        PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();

        interference.SetNoiseFloor(noiseFloor);
        interference.InitializeRx(rxPsd);
        Ptr<const SpectrumValue> sinr = interference.GetSinr(); //gia tri nay duoc luu tru de su dung sau nay

        //Get ns-3 nodes
       NodeContainer n = deviceHelper.GetNS3Nodes();
       Ptr<Node> n_plc1 = n.Get(0);
       Ptr<Node> n_plc2 = n.Get(1);

        //Install ipv4 stack on nodes
        InternetStackHelper inetHelper;
//        inetHelper.SetIpv4StackInstall(1); // cai nay la sao ???
//        inetHelper.InstallAll();
        inetHelper.Install(n);

//        ipv4AddrHelper = ns.internet.Ipv4AddressHelper()
//            ipv4AddrHelper.SetBase(ns.network.Ipv4Address('10.10.0.0'), ns.network.Ipv4Mask('255.255.255.0'))
//            ifCntr = ipv4AddrHelper.Assign(d) # Ipv4InterfaceContainer
        // Get Netdevice
        NetDeviceContainer d = deviceHelper.GetNetDevices();
        //+++ Assign IP address+++//
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0","255.255.255.0");

        Ipv4InterfaceContainer i = address.Assign(d);

        OnOffHelper onoff ("ns3::UdpSocketFactory",
                           Address (InetSocketAddress (i.GetAddress(1),9)));
        onoff.SetAttribute("DataRate", StringValue ("10000kb/s"));

        ApplicationContainer apps = onoff.Install(n_plc1);
        apps.Start(Seconds(2.0));
        apps.Stop(Seconds(10.0));

        PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(),9));
        ApplicationContainer apps_sink = sink.Install(n_plc2);
        apps_sink.Start(Seconds(2.0));
        apps_sink.Stop(Seconds(11.0));
        NS_LOG_UNCOND("******");

 //        Config::Connect("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&ReceivePacket));
        Ptr<PacketSink> PACKET_SINK;

        PACKET_SINK = (DynamicCast<PacketSink> (apps_sink.Get(0)));
         PACKET_SINK->TraceConnect ("Sink", "1", MakeCallback (&ReceivePacket));
//        PACKET_SINK->TraceConnect ("Sink","1",MakeCallback(&CalculateThroughput));
//        NS_LOG_UNCOND("+++++");

//        Simulator::Schedule(Seconds(0.0),&CalculateThroughput);
//        Simulator::Stop(Seconds(11));
        Simulator::Run();
        Simulator::Destroy();

        //+++Create UDP sockets+++//
//       TypeId socketTid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//       source = ns.network.Socket.CreateSocket (n.Get(0), socketTid);
//       Ptr<Socket> source = Socket::CreateSocket(n.Get(0),socketTid);
//       Ptr<Socket> sink = Socket::CreateSocket(n.Get(1),socketTid);


       //Setup socket
//       InetSocketAddress local = (Ipv4Address.GetAny(),4711);
//       InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(),4711);
//       InetSocketAddress remote = InetSocketAddress (i.GetAddress(1),4711);
//       sink->Bind(local);
//       sink->Listen();



//       source->Connect(remote);

//	Simulator::Run();
//       Ptr<PacketSink> Packet_Sink;

//        NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);

//sink->SetRecvCallback (MakeCallback (&ReceivePacket));


//	Simulator::Destroy();

//	return EXIT_SUCCESS;
        return 0;
}


