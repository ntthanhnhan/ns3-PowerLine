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
#include "../build/ns3/plc-mac.h"
#include "../build/ns3/plc-device-helper.h"
#include "../build/ns3/plc-channel.h"
#include "../build/ns3/plc-net-device.h"
using namespace ns3;
void AddLoopbackInterface (Ptr<Node> node, Ptr<LoopbackNetDevice> device, Ipv4InterfaceAddress ip);
void SendPacket_ToLora (Ptr<const Packet> p);
bool ReceivePacket_FromLora (Ptr<NetDevice> nd, Ptr<const Packet> p, uint16_t pro, const Address& sender);

int main (int argc, char *argv[])
{
    // Define spectrum model
    PLC_SpectrumModelHelper smHelper;
    Ptr<const SpectrumModel> sm, sm1;

    bool narrowBand = false;
    std::string cableStr("NAYY150SE");

    CommandLine cmd;
    cmd.AddValue("nb", "Use narrow-band spectrum spacing?", narrowBand);
    cmd.AddValue("cable", "Cable name. Can be one of the following:\n\t"
                          "AL3x95XLPE, NYCY70SM35, NAYY150SE, NAYY50SE, MV_Overhead", cableStr);
    cmd.Parse (argc, argv);

    sm = smHelper.GetSpectrumModel(0, 500e3, 5);

    Ptr<PLC_Cable> cable = CreateObject<PLC_NAYY150SE_Cable> (sm);

    // Create nodes
    Ptr<PLC_Node> n1 = CreateObject<PLC_Node> ();
    Ptr<PLC_Node> n2 = CreateObject<PLC_Node> ();
    n1->SetPosition(0,0,0);
    n2->SetPosition(20,0,0);

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

    // Create interfaces (usually done by the device helper)
    Ptr<PLC_TxInterface> txIf = CreateObject<PLC_TxInterface> (n1, sm);
    Ptr<PLC_RxInterface> rxIf = CreateObject<PLC_RxInterface> (n2, sm);

    // Add interfaces to the channel (usually done by the device helper)
    channel->AddTxInterface(txIf);
    channel->AddRxInterface(rxIf);


    // Define transmit power spectral density
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue> (sm);
    (*txPsd) = 1e-8; // -50dBm/Hz

    // Create PHYs
    Ptr<PLC_InformationRatePhy> phy1 = CreateObject<PLC_InformationRatePhy> ();
    Ptr<PLC_InformationRatePhy> phy2 = CreateObject<PLC_InformationRatePhy> ();

    phy1->CreateInterfaces(o1, txPsd);
    phy2->CreateInterfaces(o2, txPsd);

    // Set background noise
    Ptr<SpectrumValue> noiseFloor = CreateWorstCaseBgNoise(sm)->GetNoisePsd();
    phy1->SetNoiseFloor(noiseFloor);
    phy2->SetNoiseFloor(noiseFloor);

    // Set modulation and coding scheme
    phy1->SetHeaderModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_4,0));
    phy2->SetHeaderModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_4,0));
    phy1->SetPayloadModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_2,0));
    phy2->SetPayloadModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_2,0));

    PLC_NetDeviceHelper deviceHelper (sm,txPsd,nodes);
    deviceHelper.DefinePhyType(TypeId::LookupByName("ns3::PLC_InformationRatePhy"));
    deviceHelper.DefineMacType(TypeId::LookupByName("ns3::PLC_ArqMac"));
    deviceHelper.SetHeaderModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_4,0));
    deviceHelper.SetPayloadModulationAndCodingScheme(ModulationAndCodingScheme(BPSK_1_2,0));
    deviceHelper.Setup();
    PLC_NetdeviceMap devMap = deviceHelper.GetNetdeviceMap();

    //Get ns-3 node
    NodeContainer n_n1 = deviceHelper.GetNS3Nodes();
    n_n1.Create(1);

    NodeContainer n_n2 = deviceHelper.GetNS3Nodes();
    n_n2.Create(1);



    // Aggregate RX-Interfaces to ns3 nodes
    phy1->GetRxInterface()->AggregateObject(CreateObject<Node> ());
    phy2->GetRxInterface()->AggregateObject(CreateObject<Node> ());

    // Create MAC layers
    Ptr<PLC_ArqMac> mac1 = CreateObject<PLC_ArqMac> ();
    Ptr<PLC_ArqMac> mac2 = CreateObject<PLC_ArqMac> ();
    mac1->SetPhy(phy1);
    mac2->SetPhy(phy2);
    mac1->SetAddress(Mac48Address("00:00:00:00:00:05"));
    mac2->SetAddress(Mac48Address("00:00:00:00:00:06"));

//    ## Create net devices
//        deviceHelper = ns.plc.PLC_NetDeviceHelper(sm, txPsd, nodes)
//        deviceHelper.DefinePhyType(ns.core.TypeId.LookupByName ('ns3::PLC_InformationRatePhy'))
//        deviceHelper.DefineMacType(ns.core.TypeId.LookupByName ('ns3::PLC_ArqMac'))
//        deviceHelper.SetHeaderModulationAndCodingScheme(ns.plc.ModulationAndCodingScheme(ns.plc.BPSK_1_4,0))
//        deviceHelper.SetPayloadModulationAndCodingScheme(ns.plc.ModulationAndCodingScheme(ns.plc.BPSK_1_2,0))
//        deviceHelper.Setup()








    channel->InitTransmissionChannels();
    channel->CalcTransmissionChannels();


    //Install TCP/IP Stack on Nodes
    //Install Application on node n1, n2
//    InternetStackHelper internet_stack;
//    internet_stack.Install(n_n1);
//    internet_stack.Install(n_n2);
//    NetDeviceContainer d = deviceHelper.GetNetDevices();

//    Ptr<LoopbackNetDevice> clientLoIf = CreateObject<LoopbackNetDevice>();
//    Ptr<LoopbackNetDevice> serverLoIf = CreateObject<LoopbackNetDevice>();

//    Ipv4InterfaceAddress clientIpAddr = Ipv4InterfaceAddress(Ipv4Address("192.168.1.1"),Ipv4Mask("255.255.255.0"));
//    Ipv4InterfaceAddress serverIpAddr = Ipv4InterfaceAddress(Ipv4Address("192.168.1.2"),Ipv4Mask("255.255.255.0"));

//    Ipv4Address serverAddress = serverIpAddr.GetLocal();

//    AddLoopbackInterface(n_n1.Get(0),clientLoIf,clientIpAddr);
//    AddLoopbackInterface(n_n2.Get(0),serverLoIf,serverIpAddr);
    InternetStackHelper internet_stack;
    internet_stack.SetIpv4StackInstall(1);
    internet_stack.Install(n_n1);
    internet_stack.Install(n_n2);

//    Ipv4InterfaceAddress clientIpAddr = Ipv4InterfaceAddress(Ipv4Address("192.168.1.1"),Ipv4Mask("255.255.255.0"));
//    Ipv4InterfaceAddress serverIpAddr = Ipv4InterfaceAddress(Ipv4Address("192.168.1.2"),Ipv4Mask("255.255.255.0"));


    //+++ Assign IP address+++//
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
//    Ipv4InterfaceAddress serverIpAdrr = Ipv4InterfaceAddress(Ipv4Address("10.1.1.2"),Ipv4Mask("255.255.255.0"));
//    Ipv4Address serverAddress = serverIpAdrr.GetLocal();
    NetDeviceContainer d = deviceHelper.GetNetDevices();
    Ipv4InterfaceContainer i = address.Assign(d);


    //Install and Configure UDP Server application (Receiving UDP segment)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(n_n2.Get(0));
    apps.Start(Seconds(0.0));

    //Install and Configure UDP Client application (Sending UDP segment)
    uint32_t numsent = 100;
    UdpEchoClientHelper client(i.GetAddress(1),port);
    client.SetAttribute("MaxPacket",UintegerValue(numsent));
    client.SetAttribute("Interval",TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize",UintegerValue(100));

    apps = client.Install(n_n1.Get(0));
    apps.Start(Seconds(0.0));



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
//    Ptr<SpectrumValue> noiseFloor= Create<SpectrumValue> (sm);
    (*noiseFloor) = 1e-9;
    interference.SetNoiseFloor(noiseFloor);
    interference.InitializeRx(rxPsd);

    Ptr<const SpectrumValue> sinr = interference.GetSinr();

    NS_LOG_UNCOND("Transmit power spectral density:\n" << *txPsd << "\n");
    NS_LOG_UNCOND("Receive power spectral density:\n" << *rxPsd << "\n");
    NS_LOG_UNCOND("Noise power spectral density:\n" << *noiseFloor << "\n");
    NS_LOG_UNCOND("Signal to interference and noise ratio:\n" << *sinr);
    NS_LOG_UNCOND("Channel Transmit Function CTF (dB):\n" << 10.0 * Log10(*absSqrCtf));


    return EXIT_SUCCESS;
}

// Add loopback Interface to Nodes
//void AddLoopbackInterface (Ptr<Node> node, Ptr<LoopbackNetDevice> device, Ipv4InterfaceAddress ip) {
//    Ptr<Ipv4Interface>     interface = CreateObject<Ipv4Interface> ();

//    node->AddDevice (device);
//    interface->SetDevice (device);
//    interface->SetNode (node);
//    interface->AddAddress (ip);
//    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
//    int32_t iif = ipv4->GetInterfaceForDevice (device);
//    if (iif == -1)
//        iif = ipv4->AddInterface (device);

//    interface->SetUp ();
//    ipv4->AddAddress (iif, ip);
//    ipv4->SetMetric (iif, 1);
//    ipv4->SetUp (iif);
//}
