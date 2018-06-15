
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
#include "../build/ns3/pointer.h"
#include "../src/internet/model/ipv4-global-routing.h"

using namespace ns3;

int POSITION = 10;

//double throughput;
std::ofstream rdTrace;
void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
void WriteUntilBufferFull (Ptr<Socket>, uint32_t);
void CalculateThroughput (std::string context, Ptr<const Packet> packet, const Address & from);
int Str2Int (std::string STRING);

int LOG_User_count = 0;
uint64_t DATA_SIZE = 1e6;
static const uint32_t writeSize = 1040;
uint8_t DATA[writeSize];
uint32_t currentTxBytes[1]={0};
uint64_t totalBytesReceived[1] = {0};
double delay[1]={0};
double Throughput[1] = {0.0};
double Goodput[1] = {0.0};
int seed_Number = 1;
double Time_session_stop[1]={0};
Ptr<Socket>  ns3TcpSocket;

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

//	Ptr<PLC_Graph> graph = CreateObject<PLC_Graph> ();

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

//	graph->AddNode(n1);
//	graph->AddNode(n2);

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
//        Ptr<PLC_Outlet> outlet1 = CreateObject<PLC_Outlet> (n1, Create<PLC_ConstImpedance> (sm, PLC_Value(100,0)));
//        Ptr<PLC_Outlet> outlet2 = CreateObject<PLC_Outlet> (n2, Create<PLC_ConstImpedance> (sm, PLC_Value(40,0)));

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
       n.Create(2);
//       Ptr<Node> n_plc1 = n.Get(0);
//       Ptr<Node> n_plc2 = n.Get(1);

       PointToPointHelper Medium_plc;
       Medium_plc.SetDeviceAttribute ("DataRate", StringValue("10Mbps"));
       Medium_plc.SetChannelAttribute ("Delay", StringValue("1ms"));
       Medium_plc.SetDeviceAttribute ("Mtu", UintegerValue(1500));


//        ipv4AddrHelper = ns.internet.Ipv4AddressHelper()
//            ipv4AddrHelper.SetBase(ns.network.Ipv4Address('10.10.0.0'), ns.network.Ipv4Mask('255.255.255.0'))
//            ifCntr = ipv4AddrHelper.Assign(d) # Ipv4InterfaceContainer
        // Get Netdevice
//        Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(100));
        NetDeviceContainer c;
        NetDeviceContainer d = deviceHelper.GetNetDevices();

        channel->AddDevice(d.Get(0));
        channel->AddDevice(d.Get(1));


        c = Medium_plc.Install(n.Get(0));
        c = Medium_plc.Install(n.Get(1));

        d.Add(c.Get(0));
        d.Add(c.Get(1));



        //Install ipv4 stack on nodes
         InternetStackHelper inetHelper;
         Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId()));

 //        inetHelper.SetIpv4StackInstall(1); // cai nay la sao ???
 //        inetHelper.InstallAll();
         inetHelper.Install(n);

        //+++ Assign IP address+++//
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0","255.255.255.0");
        address.Assign(c.Get(0));
        address.Assign(c.Get(1));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        ApplicationContainer TCP_Sink_App;
        //Sink

        Ptr<Node> node = n.Get(0);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1,0).GetLocal();

        Address Sink_Addr (InetSocketAddress(addr,1024));
        PacketSinkHelper TCP_Sink ("ns3::TcpSocketFactory",Sink_Addr);
        TCP_Sink_App = (TCP_Sink.Install(n.Get(0)));
        TCP_Sink_App.Start(Seconds(0.0));


        //Source
        Ptr<Node> node_sink = n.Get(0);
        Ptr<Node> node_source = n.Get(1);
        Ptr<Ipv4> ipv4_sink = node_sink->GetObject<Ipv4>();
        Ipv4Address addr_sink = ipv4_sink->GetAddress(1,0).GetLocal();

        ns3TcpSocket = (Socket::CreateSocket(node_source,TcpSocketFactory::GetTypeId()));
        ns3TcpSocket->Bind();
        Simulator::Schedule(MilliSeconds(delay[0]),&StartFlow,ns3TcpSocket,addr_sink,1024);

//Trace Throughput
 //        Config::Connect("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&ReceivePacket));
        Ptr<PacketSink> PACKET_SINK;

        PACKET_SINK = (DynamicCast<PacketSink> (TCP_Sink_App.Get(0)));
         PACKET_SINK->TraceConnect ("Sink", "0", MakeCallback (&CalculateThroughput));

        Simulator::Run();
        Simulator::Destroy();


        return 0;
}


void StartFlow (Ptr<Socket> localSocket, Ipv4Address servAddress, uint16_t servPort){
    localSocket->Connect (InetSocketAddress (servAddress, servPort));
    localSocket->SetSendCallback (MakeCallback (&WriteUntilBufferFull));
    WriteUntilBufferFull (localSocket, localSocket->GetTxAvailable ());
    LOG_User_count++;
}

void WriteUntilBufferFull (Ptr<Socket> localSocket, uint32_t txSpace){
    int j=0;
    if (localSocket == ns3TcpSocket){
        j=0;
    }

    Ptr<UniformRandomVariable> m_random = CreateObject<UniformRandomVariable> ();
    for(uint32_t ii = 0; ii < writeSize; ++ii){
        uint8_t m = (uint8_t)m_random->GetInteger(0,255);
        DATA[ii] = m;
    }

    while (currentTxBytes[j] < DATA_SIZE && localSocket->GetTxAvailable () > 0){
        uint32_t left = DATA_SIZE - currentTxBytes[j];
        uint32_t dataOffset = currentTxBytes[j] % writeSize;
        uint32_t toWrite = writeSize - dataOffset;
        toWrite = std::min (toWrite, left);
        toWrite = std::min (toWrite, localSocket->GetTxAvailable ());

        int amountSent = localSocket->Send (&DATA[dataOffset], toWrite, 0);
        if(amountSent < 0) {
            return;
        }
        currentTxBytes[j] += amountSent;
    }

    localSocket->Close ();
}


int Str2Int (std::string STRING){
    int INT_t = atoi(STRING.c_str());
    return INT_t;
}
                            //--Callback function is called whenever a packet is received successfully.
//--This function add the size of packet to totalByteReceived counter.

//--Throughput calculating function.
//--This function calculate the throughput every 0.1s using totalBytesReceived counter.
//--and write the results into a throughput tracefile
void CalculateThroughput(std::string context, Ptr<const Packet> packet,const Address &from)
{
    int context_i = Str2Int(context);
    totalBytesReceived[context_i] += packet->GetSize();
//    Throughput[context_i] = ((totalBytesReceived*8)/100000);
//    NS_LOG_UNCOND("throughput" <<Throughput);
//    NS_LOG_UNCOND("Received Packet: "<<totalBytesReceived[context_i]);
    if (totalBytesReceived[context_i]==DATA_SIZE)
    {
        Time_session_stop[context_i] = Simulator::Now().GetMilliSeconds();
        Goodput[context_i] = (((DATA_SIZE)*8)/1000)/(Time_session_stop[context_i] - delay[context_i]);
        NS_LOG_UNCOND("delay" <<delay[context_i]);
        NS_LOG_UNCOND("goodput" <<Goodput[context_i]);
    }
//    double throughput =((totalBytesReceived*8.0)/100000);
//    NS_LOG_UNCOND ("Throughput: "<<throughput);
//    totalBytesReceived = 0;
//    rdTrace << Simulator::Now().GetSeconds() << "\t"<<throughput <<"\n";
//    Simulator::Schedule(Seconds(0.1), &CalculateThroughput);

}


/**Timer setup*/
//struct timeval tv1, tv2, timeout; //dinh nghia cho timeout va tg de tinh RTT
//fd_set readfds; //check sockets ready for reading
//FD_ZERO(&readfds); //thiet lap cho chuc nang tg cho cua socket duoc thuc hien va dat thanh 100ms
//FD_SET(sockfd,&readfds);
//timeout.tv_sec = 0;
//timeout.tv_sec = 100000;
// khong can n = select (sockfd+1,&readfds,NULL,NULL,&timeout); //select duoc goi tren socket de socket san sang doc, gia tri tra ve duoc luu trong 1 bien n, n tra ve -1 khi co loi, tra ve 0 tg cho da dat duoc, lon hon 0 socket san sang doc

/*RTT - THROUGHPUT*/
//recvfrom(sockfd,recvline,strlen(recvline),0,NULL,NULL);
//gettimeofday(&tv2,NULL);
//time_spent[i] = (double)(tv2.tv_sec - tv1.tv_sec)/1000000 + (double)(tv2.tv_sec - tv1.tv_sec);
//roundTripTime[i] = time_spent[i];
//latency[i] = time_spent[i]/2;
//throughput[i] = (udpBufferSize*2/time_spent[i]);

//FILE * fp;
//fp = fopen (filename1, "a");
//fprintf(fp, "%f\n",roundTripTime[i]);
//fclose(fp);

//FILE *fp2;
//fp2 = fopen (filenam2, "a");
//fprintf(fp2, "%f\n",througput[i]/1000000);
//fclose(fp2);
//received_ack ++;
