
/*--------------------------------------------
 * Project TCP NewReno -----------------------
 * Creator: Nguyen Thi My Linh----------------
 * Use Tow State Markov Chain Model Loss------
 * Release date: 28/03/2018  -----------------
 *--------------------------------------------*/

#include "../build/ns3/netanim-module.h"
#include "../build/ns3/core-module.h"
#include "../build/ns3/network-module.h"
#include "../build/ns3/internet-module.h"
#include "../build/ns3/point-to-point-module.h"
#include "../build/ns3/csma-module.h"
#include "../build/ns3/wifi-module.h"
#include "../build/ns3/mobility-module.h"
#include "../build/ns3/applications-module.h"
#include "../build/ns3/packet-sink.h"
#include "../build/ns3/config-store-module.h"
#include "../build/ns3/flow-monitor-module.h"
#include "../build/ns3/random-variable-stream.h"


/*Network Topology:
 * [Source 1]--<1Mbps;1ms>--                                              --<1Mbps;1ms>--[Sink 1]
 *                          |                                            |
 *                         [R1]--<1Mbps;10ms>--[R2]--<1Mbps;10ms>--[R3]--
 *                          |                                            |
 * [Source 2]--<1Mbps;1ms>--                                              --<1Mbps;1ms>--[Sink 2]
 */


using namespace ns3;

void StartFlow (Ptr<Socket>, Ipv4Address, uint16_t);
void WriteUntilBufferFull (Ptr<Socket>, uint32_t);
static void CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd);
static void EnDeQueue_Trace (std::string context, Ptr<const Packet> packet);
void Counter_Data_TX (std::string context, Ptr<const Packet> packet, const Address & from);
int Str2Int (std::string STRING);

uint32_t BUFFER_R1 = 0;
double Time_session_stop[2]={0,0};
uint64_t count_RX[2] = {0,0};
double Goodput[2] = {0.0,0.0};

std::ofstream LOG_FILE_0;   // Log Buffer
std::ofstream LOG_FILE_1;   // Log Cwnd
std::ofstream LOG_FILE_3;   // Log Goodput

Ptr<Socket>  ns3TcpSocket1;
Ptr<Socket>  ns3TcpSocket2;
int LOG_User_count = 0;
uint64_t DATA_SIZE = 1e6;
static const uint32_t writeSize = 1040;
uint8_t DATA[writeSize];
uint32_t currentTxBytes[2]={0,0};
double delay[2]={0,100};
int seed_Number = 1;

float errorRate= 0.01;

//=================================================== S T A R T - M A I N ============================================================

int main (int argc, char *argv[])
{

    //    srand (time (0));
    //    int seedNumber = rand () % 1000;
    //    SeedManager::SetSeed (seedNumber);

    //    int runNumber = rand () % 1000;
    //    SeedManager::SetRun (runNumber);

    CommandLine cmd;
    cmd.AddValue("seed_Number","Seed num là: ",seed_Number);
    cmd.AddValue("err","Seed num là: ",errorRate);
    cmd.Parse (argc,argv);

    SeedManager::SetSeed (seed_Number);
    SeedManager::SetRun (seed_Number);

    //--------------------------------

    char temp[200] = "Goodput_";

    char *temp_1 = new char[200];
    strcpy(temp_1,temp);

    char temp_2[200]="";
    sprintf(temp_2,"%f", errorRate);
    temp_1 = strcat(temp_1,temp_2);
    temp_1 = strcat(temp_1,".csv");

    //NS_LOG_UNCOND("file:    " << temp_1);

    LOG_FILE_3.open (temp_1);
    LOG_FILE_0.open ("Buffer_R1.txt");
    LOG_FILE_1.open ("Cwnd.txt");

    //Create Node

    //NS_LOG_UNCOND ("\n\nCreating Nodes...");

    NodeContainer NODES_INTERMEDIATE;
    NODES_INTERMEDIATE.Create (3);

    NodeContainer NODES_SOURCE;
    NODES_SOURCE.Create (2);

    NodeContainer NODES_SINK;
    NODES_SINK.Create (2);

    //NS_LOG_UNCOND ("Done\n");

    //Create Medium
    //NS_LOG_UNCOND ("Creating Medium...");

    // Intermediate Nodes
    PointToPointHelper MEDIUM_INTER;
    MEDIUM_INTER.SetDeviceAttribute ("DataRate", StringValue("1Mbps"));
    MEDIUM_INTER.SetChannelAttribute ("Delay", StringValue("10ms"));
    MEDIUM_INTER.SetDeviceAttribute ("Mtu", UintegerValue(1500));

    // Source Nodes
    PointToPointHelper MEDIUM_SOURCE;
    MEDIUM_SOURCE.SetDeviceAttribute ("DataRate", StringValue("1Mbps"));
    MEDIUM_SOURCE.SetChannelAttribute ("Delay", StringValue("1ms"));
    MEDIUM_SOURCE.SetDeviceAttribute ("Mtu", UintegerValue(1500));

    // Sink Nodes
    PointToPointHelper MEDIUM_SINK;
    MEDIUM_SINK.SetDeviceAttribute ("DataRate", StringValue("1Mbps"));
    MEDIUM_SINK.SetChannelAttribute ("Delay", StringValue("1ms"));
    MEDIUM_SINK.SetDeviceAttribute ("Mtu", UintegerValue(1500));

    //NS_LOG_UNCOND ("Done\n");
    //=============================================================================================

    //Install Hardware on nodes

    // Set buffer size for Intermediate device
    Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (100));
    NetDeviceContainer c;
    //NetDeviceContainer đã được khai báo trong namespace ns3
    // create empty NetDeviceContainer

    NetDeviceContainer HW_NODES_INTER;
    NetDeviceContainer HW_NODES_SOURCE;
    NetDeviceContainer HW_NODES_SINK;

    c = MEDIUM_SOURCE.Install (NODES_SOURCE.Get(0), NODES_INTERMEDIATE.Get(0));
    c.Add (MEDIUM_SOURCE.Install (NODES_SOURCE.Get(1), NODES_INTERMEDIATE.Get(0)));
    c.Add (MEDIUM_INTER.Install (NODES_INTERMEDIATE.Get(0), NODES_INTERMEDIATE.Get(1)));
    c.Add (MEDIUM_INTER.Install (NODES_INTERMEDIATE.Get(1), NODES_INTERMEDIATE.Get(2)));
    c.Add (MEDIUM_SINK.Install (NODES_INTERMEDIATE.Get(2),NODES_SINK.Get(0)));
    c.Add (MEDIUM_SINK.Install (NODES_INTERMEDIATE.Get(2),NODES_SINK.Get(1)));

    HW_NODES_SOURCE.Add (c.Get (0));
    HW_NODES_SOURCE.Add (c.Get (2));
    HW_NODES_INTER.Add (c.Get (1));
    HW_NODES_INTER.Add (c.Get (3));
    HW_NODES_INTER.Add (c.Get (4));
    HW_NODES_INTER.Add (c.Get (5));
    HW_NODES_INTER.Add (c.Get (6));
    HW_NODES_INTER.Add (c.Get (7));
    HW_NODES_INTER.Add (c.Get (8));
    HW_NODES_INTER.Add (c.Get (10));
    HW_NODES_SINK.Add (c.Get (9));
    HW_NODES_SINK.Add (c.Get (11));

    //NS_LOG_UNCOND ("Done\n");
    //=============================================================================================


    //Install the internet stack on nodes
    //NS_LOG_UNCOND ("Installing Internet stack on Nodes...");
    InternetStackHelper internet_stack;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId()));
    internet_stack.Install(NODES_INTERMEDIATE);
    internet_stack.Install(NODES_SOURCE);
    internet_stack.Install(NODES_SINK);

    //NS_LOG_UNCOND (" Done\n");
    //=============================================================================================


    //Add IP to devices
    //NS_LOG_UNCOND ("Adding IP address to devices...");
    Ipv4AddressHelper IPV4;

    IPV4.SetBase("192.168.1.0","255.255.255.0");
    IPV4.Assign (c.Get(0));
    IPV4.Assign (c.Get(1));

    IPV4.SetBase("192.168.2.0","255.255.255.0");
    IPV4.Assign (c.Get(2));
    IPV4.Assign (c.Get(3));

    IPV4.SetBase("10.0.1.0","255.255.255.0");
    IPV4.Assign (c.Get(4));
    IPV4.Assign (c.Get(5));

    IPV4.SetBase("10.0.2.0","255.255.255.0");
    IPV4.Assign (c.Get(6));
    IPV4.Assign (c.Get(7));


    IPV4.SetBase("172.16.1.0","255.255.255.0");
    IPV4.Assign (c.Get(8));
    IPV4.Assign (c.Get(9));

    IPV4.SetBase("172.16.2.0","255.255.255.0");
    IPV4.Assign (c.Get(10));
    IPV4.Assign (c.Get(11));

    //NS_LOG_UNCOND (" Done\n");
    //=============================================================================================

    // Install Routing Table
    //NS_LOG_UNCOND ("Adding routing table...");

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    //NS_LOG_UNCOND (" Done\n");
    //=============================================================================================

    // Install Apllication to nodes

    //Sink
    //NS_LOG_UNCOND ("Installing TCP Sink apllication to node...");
    ApplicationContainer TCP_Sink_APP_1,TCP_Sink_APP_2;

    // Sink 1: Get IP Address
    Ptr<Node> node = NODES_SINK.Get (0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal ();

    Address Sink_ADDR (InetSocketAddress(addr,1024));
    PacketSinkHelper TCP_Sink ("ns3::TcpSocketFactory",Sink_ADDR);
    TCP_Sink_APP_1 = (TCP_Sink.Install(NODES_SINK.Get(0)));
    TCP_Sink_APP_1.Start(Seconds(0.0));
    //TCP_Sink_APP_1.Stop (Seconds(128.0));

    //Sink 2: Get IP Address
    node = NODES_SINK.Get (1);
    ipv4 = node->GetObject<Ipv4>();
    addr = ipv4->GetAddress (1, 0).GetLocal ();

    Sink_ADDR = (InetSocketAddress(addr,1024));
    PacketSinkHelper TCP_Sink2 ("ns3::TcpSocketFactory",Sink_ADDR);
    TCP_Sink_APP_2 = (TCP_Sink2.Install(NODES_SINK.Get(1)));
    TCP_Sink_APP_2.Start(Seconds(0.0));
    //TCP_Sink_APP_2.Stop (Seconds(128.0));

    //NS_LOG_UNCOND (" Done\n");

    //Source
    //NS_LOG_UNCOND ("Installing TCP Source apllication to node...");

    //Source 1: Get IP Address
    Ptr<Node> node_right = NODES_SINK.Get(0);
    Ptr<Node> node_left  = NODES_SOURCE.Get(0);
    Ptr<Ipv4> ipv4_right = node_right->GetObject<Ipv4>();
    Ipv4Address addr_right = ipv4_right->GetAddress (1, 0).GetLocal ();

    ns3TcpSocket1 = (Socket::CreateSocket (node_left, TcpSocketFactory::GetTypeId ())); // Read more in Examples/tcp/tcp-large-trasfer
    ns3TcpSocket1->Bind();
    Simulator::Schedule (MilliSeconds(delay[0]), &StartFlow, ns3TcpSocket1, addr_right, 1024);


    //Source 2: Get IP Address
    node_right = NODES_SINK.Get(1);
    node_left  = NODES_SOURCE.Get(1);
    ipv4_right = node_right->GetObject<Ipv4>();
    addr_right = ipv4_right->GetAddress (1, 0).GetLocal ();

    ns3TcpSocket2 = (Socket::CreateSocket (node_left, TcpSocketFactory::GetTypeId ())); // Read more in Examples/tcp/tcp-large-trasfer
    ns3TcpSocket2->Bind ();
    Simulator::Schedule (MilliSeconds(delay[1]), &StartFlow, ns3TcpSocket2, addr_right, 1024);

    //NS_LOG_UNCOND (" Done\n");
    //------------------------------------------------------------------------------------------------------------

    //error

    Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (errorRate));

//    Config::SetDefault ("ns3::TwoStateMarkovChain::BurstRate", DoubleValue (errorRate));
//    Config::SetDefault ("ns3::TwoStateMarkovChain::AverageBurstSize", DoubleValue (7));   //Average burst size = 7

    ObjectFactory factory;
    factory.SetTypeId ("ns3::BurstErrorModel");

    Ptr<ErrorModel> CHANNEL;

    CHANNEL = factory.Create<BurstErrorModel> ();

    HW_NODES_INTER.Get(0) -> SetAttribute ("ReceiveErrorModel", PointerValue (CHANNEL));
    HW_NODES_INTER.Get(1) -> SetAttribute ("ReceiveErrorModel", PointerValue (CHANNEL));


    //--------end----------error------------end--------------error---------------------end-----------

    //Trace_Goodput
    Ptr<PacketSink> PACKET_SINK_1, PACKET_SINK_2;
    //session 1
    PACKET_SINK_1 = (DynamicCast<PacketSink> (TCP_Sink_APP_1.Get(0)));
    PACKET_SINK_1->TraceConnect ("Rx", "0", MakeCallback (&Counter_Data_TX));

    //session 2
    PACKET_SINK_2 = (DynamicCast<PacketSink> (TCP_Sink_APP_2.Get(0)));
    PACKET_SINK_2->TraceConnect ("Rx", "1", MakeCallback (&Counter_Data_TX));

    //Trace_Buffer
    DynamicCast<PointToPointNetDevice> (HW_NODES_INTER.Get(3)) -> GetQueue()->TraceConnect ("Enqueue", "0", MakeCallback(&EnDeQueue_Trace));
    DynamicCast<PointToPointNetDevice> (HW_NODES_INTER.Get(3)) -> GetQueue()->TraceConnect ("Dequeue", "1", MakeCallback(&EnDeQueue_Trace));

    //Trace_Cwnd
    ns3TcpSocket1->TraceConnect ("CongestionWindow", "0", MakeCallback (&CwndChange));
    ns3TcpSocket2->TraceConnect ("CongestionWindow", "1", MakeCallback (&CwndChange));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
    NS_LOG_UNCOND("DONE **************************************************************");

}
//=================================================== E N D - M A I N ============================================================

void StartFlow (Ptr<Socket> localSocket, Ipv4Address servAddress, uint16_t servPort){
    localSocket->Connect (InetSocketAddress (servAddress, servPort));
    localSocket->SetSendCallback (MakeCallback (&WriteUntilBufferFull));
    WriteUntilBufferFull (localSocket, localSocket->GetTxAvailable ());
    LOG_User_count++;
}

void WriteUntilBufferFull (Ptr<Socket> localSocket, uint32_t txSpace){
    int j=0;
    if (localSocket == ns3TcpSocket1){
        j=0;
    }
    if (localSocket == ns3TcpSocket2){
        j=1;
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

static void EnDeQueue_Trace (std::string context, Ptr<const Packet> packet){
    int context_i= Str2Int(context);
    if (context_i == 0){
        BUFFER_R1++;
    }
    else
    {
        BUFFER_R1--;
    }
    LOG_FILE_0 << Simulator::Now().GetMilliSeconds() <<"," << BUFFER_R1 << "\n";

}

static void CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd){
    LOG_FILE_1 << Str2Int(context) << "," << Simulator::Now ().GetMicroSeconds () << "," << newCwnd << '\n';
}
// // Function: Display Process Percent & Goodput Trace ===============================================

void Counter_Data_TX (std::string context, Ptr<const Packet> packet, const Address & from) {
    int context_i= Str2Int(context);
    count_RX[context_i] = count_RX[context_i] + packet->GetSize();
    if(count_RX[context_i]==DATA_SIZE){
        Time_session_stop[context_i] = Simulator::Now().GetMilliSeconds();
        Goodput[context_i]= (((DATA_SIZE)*8) /1000)/ (Time_session_stop[context_i]-delay[context_i]);
        NS_LOG_UNCOND(delay[context_i] << "------delay");
        NS_LOG_UNCOND(DATA_SIZE << "------data size");
        NS_LOG_UNCOND(Time_session_stop[context_i] << "------Time_session_stop");
        NS_LOG_UNCOND(Goodput[context_i] << "------Goodput");
          LOG_FILE_3 << Goodput[context_i] << ",";
    }
}

int Str2Int (std::string STRING){
    int INT_t = atoi(STRING.c_str());
    return INT_t;
}

