/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University of British Columbia, Vancouver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,e
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

    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
    sm = smHelper.GetSpectrumModel(0, 10e6, 100);

    // Define transmit power spectral density
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz


    // Create cable types
    Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY150SE_Cable> (sm);
    //        Ptr<PLC_Cable> cable = CreateObject<PLC_AL3x95XLPE_Cable> (sm); //thay đổi loại cable xem có ảnh hưởng gì không
    //        Ptr<PLC_Cable> cable = CreateObject<PLC_NYCY70SM35_Cable> (sm);
    //        Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY50SE_Cable> (sm);
    //        Ptr<PLC_Cable> cable = CreateObject<PLC_MV_Overhead_Cable> (sm);

    // Create nodes
    Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();
    n1->SetPosition(0,0,0);
    n2->SetPosition(1000,0,0); //~470m thi van con nhan dc packet
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

    // Create outlets: can thiet khong???
    Ptr<PLC_Outlet> o1 = CreateObject<PLC_Outlet> (n1);
    Ptr<PLC_Outlet> o2 = CreateObject<PLC_Outlet> (n2);
    //Tam thoi khong xet toi
    // Define background noise
    //    Ptr<SpectrumValue> noiseFloor = CreateWorstCaseBgNoise(sm)->GetNoisePsd();
    Ptr<SpectrumValue> noiseFloor = CreateBestCaseBgNoise(sm)->GetNoisePsd();

    // Create net devices
    PLC_NetDeviceHelper deviceHelper(sm, txPsd, nodes);
    deviceHelper.SetNoiseFloor(noiseFloor);
    deviceHelper.Setup();
    //	PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();
    //Define Phy Type
    //Set Payload Modulation and coding scheme

    // Create interfaces (usually done by the device helper)
    Ptr<PLC_TxInterface> txIf = CreateObject<PLC_TxInterface> (n1, sm);
    Ptr<PLC_RxInterface> rxIf = CreateObject<PLC_RxInterface> (n2, sm);

    // Add interfaces to the channel (usually done by the device helper)
    channel->AddTxInterface(txIf);
    channel->AddRxInterface(rxIf);


    // Calculate channels
    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();

    Ptr<PLC_ChannelTransferImpl> chImpl = txIf->GetChannelTransferImpl(PeekPointer(rxIf));
    NS_ASSERT(chImpl);
    Ptr<SpectrumValue> rxPsd = chImpl->CalculateRxPowerSpectralDensity(txPsd);

    PLC_Interference interference;
    //sm1 = smHelper.GetSpectrumModel(0, 10e6, 100);
    //PLC_ColoredNoiseFloor nf;
    //nf.PLC_ColoredNoiseFloor(2,3,4,sm1);
//    Ptr<SpectrumValue> noiseFloor= Create<SpectrumValue> (sm);
//    (*noiseFloor) = 1e-9;
    interference.SetNoiseFloor(noiseFloor);
    interference.InitializeRx(rxPsd);

    Ptr<const SpectrumValue> sinr = interference.GetSinr();



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


    // Trace packet received
    Ptr <UdpServer> serverUDP =  server.GetServer();
    uint64_t numPktRev  = serverUDP->GetReceived();
    NS_LOG_UNCOND("Number of received packet is: " << numPktRev <<"\n");

//    NS_LOG_UNCOND("Transmit power spectral density:\n" << *txPsd << "\n");
//    NS_LOG_UNCOND("Receive power spectral density:\n" << *rxPsd << "\n");
//    NS_LOG_UNCOND("Noise power spectral density:\n" << *noiseFloor << "\n");
//    NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);
//  NS_LOG_UNCOND("CTF (dB):\n" << 10.0 * Log10(*absSqrCtf));
    // Cleanup simulation
    Simulator::Destroy();

    return EXIT_SUCCESS;
}
