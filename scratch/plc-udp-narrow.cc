
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
#include "../build/ns3/flow-monitor.h"
#include "../build/ns3/flow-monitor-helper.h"

uint16_t POSITION = 10;

//*******************co nhieu***************//

using namespace ns3;
std::ofstream LOG_FILE_0 ("LR.txt");

int main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.AddValue("distance","Khoang cach giua 2 node PLC la: ",POSITION);
    cmd.Parse (argc,argv);



//    LOG_FILE_0.open ("LossRate2.txt");

    LogComponentEnableAll(LOG_PREFIX_TIME);
    //        LogComponentEnable("PLC_Mac", LOG_LEVEL_INFO);
    Packet::EnablePrinting();

    PLC_Time::SetTimeModel(50,MicroSeconds(735));
    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm;
//    sm = smHelper.GetSpectrumModel(0, 10e6, 100); //Broadband
     sm = smHelper.GetSpectrumModel(0, 500e3, 5); //Narrowband

    // Define transmit power spectral density
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // Create cable types
    Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

    // Create nodes
    Ptr<PLC_Node> PLC1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> PLC2 = CreateObject<PLC_Node> ();


    PLC1->SetPosition(0,0,0);
    PLC2->SetPosition(POSITION,0,0);
    PLC1->SetName("PLC-1");
    PLC2->SetName("PLC-2");


    PLC_NodeList PLC;
    PLC.push_back(PLC1);
    PLC.push_back(PLC2);



    // Link nodes
    CreateObject<PLC_Line> (cable, PLC1, PLC2);

    // Set up channel
    PLC_ChannelHelper channelHelper(sm);
    channelHelper.Install(PLC);
    Ptr<PLC_Channel> channel = channelHelper.GetChannel();

    // Create outlets: can thiet khong???
    //    Ptr<PLC_Outlet> outlet1 = CreateObject<PLC_Outlet> (PLC1);
    //    Ptr<PLC_Outlet> outlet2 = CreateObject<PLC_Outlet> (PLC2);
    //Tam thoi khong xet toi
    // Define background noise
    //    Ptr<SpectrumValue> noiseFloor = CreateWorstCaseBgNoise(sm)->GetNoisePsd();

    // Create net devices
    PLC_NetDeviceHelper deviceHelper(sm, txPsd, PLC);
    //        deviceHelper.SetNoiseFloor(noiseFloor);
    deviceHelper.Setup();
    //	PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();
    //Define Phy Type
    //Set Payload Modulation and coding scheme


    //Get ns-3 nodes
    //    NodeContainer n = deviceHelper.GetNS3Nodes();
    NodeContainer n = deviceHelper.GetNS3Nodes();

    // Calculate channels
    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();




    // Get Netdevice
    NetDeviceContainer d = deviceHelper.GetNetDevices();


    //Install the internet stack on nodes
    //NS_LOG_UNCOND ("Installing Internet stack on Nodes...");
    InternetStackHelper internet_stack;

    //Install ipv4 stack on nodes
    //    InternetStackHelper inetHelper;
    internet_stack.SetIpv4StackInstall(1); // cai nay la sao ???
    internet_stack.Install (n);
//    internet_stack.Install (n);

    //Add IP to devices
    //NS_LOG_UNCOND ("Adding IP address to devices...");
    Ipv4AddressHelper addr;

    addr.SetBase("192.168.1.0","255.255.255.0");

    Ipv4InterfaceContainer i = addr.Assign(d);
//    i = IPV4.Assign(d.Get(1));

    // Create one udpServer applications on node one.

    uint16_t serverport = 9;
    UdpServerHelper server (serverport);
    ApplicationContainer apps_server = server.Install (n.Get(1));
//    apps_server = server.Install (n.Get(1));
    apps_server.Start (Seconds (0.0));
//    apps_server.Stop (Seconds (12.0));


    // Create one UdpClient application to send UDP datagrams from node zero to node one.

    uint32_t MaxPacketSize = 100;
    Time interPacketInterval = Seconds (0.7); //
              uint32_t numsent = 5000;
    UdpClientHelper client (i.GetAddress(1), serverport);
              client.SetAttribute ("MaxPackets", UintegerValue (numsent));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    ApplicationContainer apps_client = client.Install (n.Get(0));
//    apps_client = client.Install (n.Get(0));
    apps_client.Start (Seconds (0.0));
    //apps_client.Stop (Seconds (12.0));



    // Create interfaces (usually done by the device helper)
    Ptr<PLC_TxInterface> txIf = CreateObject<PLC_TxInterface> (PLC1, sm);
    Ptr<PLC_RxInterface> rxIf = CreateObject<PLC_RxInterface> (PLC2, sm);

    // Add interfaces to the channel (usually done by the device helper)
    channel->AddTxInterface(txIf);
    channel->AddRxInterface(rxIf);

    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();

    // Define transmit power spectral density
//Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
//(*txPsd) = 1e-8; // -50dBm/Hz

// The receive power spectral density computation is done by the channel
// transfer implementation from TX interface to RX interface
    Ptr<PLC_ChannelTransferImpl> chImpl = txIf->GetChannelTransferImpl(PeekPointer(rxIf));
    NS_ASSERT(chImpl);
    Ptr<SpectrumValue> rxPsd = chImpl->CalculateRxPowerSpectralDensity(txPsd);
    Ptr<SpectrumValue> absSqrCtf = chImpl->GetAbsSqrCtf(0);

    // SINR is calculated by PLC_Interference (member of PLC_Phy)
    //PLC_NoiseSource N1;
    //N1.SetNoisePsd(sm1);
    PLC_Interference interference;
    //sm1 = smHelper.GetSpectrumModel(0, 10e6, 100);
    //PLC_ColoredNoiseFloor nf;
    //nf.PLC_ColoredNoiseFloor(2,3,4,sm1);
    Ptr<SpectrumValue> noiseFloor= Create<SpectrumValue> (sm);
    (*noiseFloor) = 15e-9;
    interference.SetNoiseFloor(noiseFloor);
    interference.InitializeRx(rxPsd);

    Ptr<const SpectrumValue> sinr = interference.GetSinr();

    NS_LOG_UNCOND("Transmit power spectral density:\n" << *txPsd << "\n");
    NS_LOG_UNCOND("Receive power spectral density:\n" << *rxPsd << "\n");
    NS_LOG_UNCOND("Noise power spectral density:\n" << *noiseFloor << "\n");
    NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);
    NS_LOG_UNCOND("Channel Transmit Function CTF (dB):\n" << 10.0 * Log10(*absSqrCtf));

    // Flow monitor
//    Ptr<FlowMonitor> flowMonitor;
//    FlowMonitorHelper flowHelper;
//    flowMonitor = flowHelper.InstallAll();

//NS3@teamwork
//       Simulator::Stop (Seconds(10.0)); //Stop dat sau Flow monitor va truoc save xml file


    Simulator::Run();
//    NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);
    // Save Monitor result to XML file
//    flowMonitor->SerializeToXmlFile("FlowMonitor1.xml", true, true); // Flowmonitor trace at layer-3


    uint32_t numReceive = server.GetServer()->GetReceived();
    uint32_t numLost    = numsent - numReceive;
    NS_LOG_UNCOND("Number of Received: " << numReceive <<"\n");
    NS_LOG_UNCOND ("Number of Lost = " << numLost);
    float LossRate = (float)numLost/(float)numsent;
    NS_LOG_UNCOND ("Loss Rate = " << LossRate);


    LOG_FILE_0 << POSITION << "," << LossRate << "\n" ;
    LOG_FILE_0.close();
//    LOG_FILE_0.open("LR.txt");


    Simulator::Destroy();

    return EXIT_SUCCESS;
}
