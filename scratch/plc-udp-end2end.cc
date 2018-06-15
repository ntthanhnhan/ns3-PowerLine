/* -*- 
Topology:

         n0-----------------------n1
-*- */

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

//using namespace  std;
using namespace ns3;

//Prototype functions
//void ReceivePacket(Ptr<Socket> socket);
void SendPacket(Ptr<Socket> socket, Ptr<Packet> packet);

// Global variables
//static const uint32_t totalTxBytes = 2000000;


int main (int argc, char *argv[])
{

    LogComponentEnableAll(LOG_PREFIX_TIME);
    //LogComponentEnable("PLC_Mac", LOG_LEVEL_INFO);
    Packet::EnablePrinting();

    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
    sm = smHelper.GetSpectrumModel(0, 10e6, 100);

    // Define transmit power spectral density
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // Create cable types
    Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

    // Create nodes
    Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();
    n1->SetPosition(0,0,0);
    n2->SetPosition(1000,0,0);
    n1->SetName("Node1");
    n2->SetName("Node2");

    PLC_NodeList nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);

    // Link nodes
    CreateObject<PLC_Line> (cable, n1, n2);

    // Set up channel
    PLC_ChannelHelper channelHelper(sm);
    channelHelper.Install(nodes);
    Ptr<PLC_Channel> channel = channelHelper.GetChannel();

    // Create outlets
    Ptr<PLC_Outlet> o1 = CreateObject<PLC_Outlet> (n1);
    Ptr<PLC_Outlet> o2 = CreateObject<PLC_Outlet> (n2);

    // Define background noise
    Ptr<SpectrumValue> noiseFloor = CreateWorstCaseBgNoise(sm)->GetNoisePsd();

    // Create net devices
    PLC_NetDeviceHelper deviceHelper(sm, txPsd, nodes);
    deviceHelper.SetNoiseFloor(noiseFloor);
    deviceHelper.Setup();
    PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();

    // Calculate channels
    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();

        //Get ns-3 nodes
        NodeContainer n = deviceHelper.GetNS3Nodes();

        // Get Netdevice
        NetDeviceContainer d = deviceHelper.GetNetDevices();

    // Install internet stack on nodes
        InternetStackHelper internet;
        internet.Install(n);

    // Assign IPv4 addresses.
    Ipv4AddressHelper IPv4;
    IPv4.SetBase("1.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = IPv4.Assign(d);

        //Create UDP sockets///
        // \brief tid
        //   socketTid = ns.core.TypeId.LookupByName ('ns3::UdpSocketFactory')
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        // create socket on receiver n1
        Ptr<Socket> recvSink = Socket::CreateSocket(n.Get(1), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 4477);
        recvSink->Bind(local);
        recvSink->Listen();
//        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

        // create socket on sender n0
        Ptr<Socket> source = Socket::CreateSocket(n.Get(0), tid);
        InetSocketAddress remote = InetSocketAddress(i.GetAddress(1), 4477);
        source->Connect(remote);

        // Create packet to send
        Ptr<Packet> p = Create<Packet>(2048);


        Simulator::Schedule(MilliSeconds(1000), &SendPacket, source, p);


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


//void ReceivePacket(Ptr<Socket> socket)
//{
//        int numPkt = 1;

//        Ptr<Packet> packet = socket->Recv();
//        numPkt = numPkt + 1;
//        NS_LOG_UNCOND("Received "<< numPkt <<" packet!") ;

//}

void SendPacket(Ptr<Socket> socket, Ptr<Packet> packet)
{
        socket->Send(packet);
}


