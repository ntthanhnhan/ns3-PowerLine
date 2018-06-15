
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

int main (int argc, char *argv[])
{
//    uint32_t n_plc = 2;
    CommandLine cmd;
//    cmd.AddValue ("n_plc", "Number of node PLC", n_plc);
    cmd.Parse(argc,argv);
//    rdTrace.open("udpPlcThroughput.csv", std::ios::out);
//    rdTrace <<"# Time \t Throughput \n";

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
       Ptr<Node> n_plc1 = n.Get(0);
       Ptr<Node> n_plc2 = n.Get(1);

        //Install ipv4 stack on nodes

//        inetHelper.SetIpv4StackInstall(1); // cai nay la sao ???
//        inetHelper.InstallAll();

//        ipv4AddrHelper = ns.internet.Ipv4AddressHelper()
//            ipv4AddrHelper.SetBase(ns.network.Ipv4Address('10.10.0.0'), ns.network.Ipv4Mask('255.255.255.0'))
//            ifCntr = ipv4AddrHelper.Assign(d) # Ipv4InterfaceContainer
        // Get Netdevice

        //+++ Assign IP address+++//
        //Install ipv4 stack on nodes
        InternetStackHelper inetHelper;
//        inetHelper.SetIpv4StackInstall(); // cai nay la sao ???
        inetHelper.Install (n_plc1);
        inetHelper.Install(n_plc2);

        //+++ Assign IP address+++//
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        NetDeviceContainer d = deviceHelper.GetNetDevices();
        Ipv4InterfaceContainer i = address.Assign(d);
        //Ipv4InterfaceContainer

        // Create one udpServer applications on node one.

        uint16_t serverport = 4000;
        UdpServerHelper server (serverport);
        ApplicationContainer apps_server = server.Install (n.Get(1));
        apps_server.Start (Seconds (1.0));
        apps_server.Stop (Seconds (10.0));

        // Create one UdpClient application to send UDP datagrams from node zero to node one.

        uint32_t MaxPacketSize = 1024;
        Time interPacketInterval = Seconds (1.0); // 200 packets per seconds
                  uint32_t maxPacketCount = 2008;
        UdpClientHelper client (i.GetAddress(1), serverport);
                  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
        client.SetAttribute ("Interval", TimeValue (interPacketInterval));
        client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
        ApplicationContainer apps_client = client.Install (n.Get(0));
        apps_client.Start (Seconds (2.0));
        apps_client.Stop (Seconds (12.0));

        Simulator::Run();
        Ptr <UdpServer> serverUDP =  server.GetServer();
        uint64_t numPktRev  = serverUDP->GetReceived();
        NS_LOG_UNCOND("NumberPktRev : " << numPktRev <<"\n");


        Simulator::Destroy();

        return 0;
}
