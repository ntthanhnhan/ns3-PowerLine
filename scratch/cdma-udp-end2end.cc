#include "../build/ns3/core-module.h"
#include "../build/ns3/applications-module.h"
#include "../build/ns3/network-module.h"
#include "../build/ns3/internet-module.h"
#include "../build/ns3/point-to-point-module.h"
#include "../build/ns3/ipv4-global-routing-helper.h"
#include "../build/ns3/netanim-module.h"

using namespace ns3;


int main(){


// Create nodes
NodeContainer SRC_NODE_CONTAINER; // this is just a empty container object to contain SRC nodes, it is not a node
SRC_NODE_CONTAINER.Create(1);     // append node object into container object, use get() to get the appended node
Ptr<Node> SRC_NODE_1 = SRC_NODE_CONTAINER.Get (0);

NodeContainer DST_NODE_CONTAINER;
DST_NODE_CONTAINER.Create(1);
Ptr<Node> DST_NODE_1 = DST_NODE_CONTAINER.Get (0);


// Create links with type Point to Point
PointToPointHelper SRC_DST_LINK;
SRC_DST_LINK.SetDeviceAttribute ("DataRate", StringValue("100Mbps"));
SRC_DST_LINK.SetDeviceAttribute ("Mtu", UintegerValue(1500));

// Use Netdevice to connect links and nodes
NetDeviceContainer DEV_SRC_DST = SRC_DST_LINK.Install(SRC_NODE_1,DST_NODE_1);


//Add ip/tcp stack to all nodes.
InternetStackHelper internet;
internet.InstallAll ();

// Assign IPv4 addresses.
Ipv4AddressHelper IPv4;
IPv4.SetBase("10.10.10.0", "255.255.255.0");
IPv4.Assign(DEV_SRC_DST);
Ipv4InterfaceContainer i = IPv4.Assign(DEV_SRC_DST);


// Create one udpServer applications on node one.

   uint16_t serverport = 4000;
   UdpServerHelper server (serverport);
   ApplicationContainer apps_server = server.Install (DST_NODE_1);
   apps_server.Start (Seconds (1.0));
   apps_server.Stop (Seconds (12.0));


// Create one UdpClient application to send UDP datagrams from node zero to node one.

   uint32_t MaxPacketSize = 64;
   Time interPacketInterval = Seconds (0.005); // 200 packets per seconds
   uint32_t maxPacketCount = 2008;
   UdpClientHelper client (i.GetAddress(1), serverport);
   client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
   client.SetAttribute ("Interval", TimeValue (interPacketInterval));
   client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
   ApplicationContainer apps_client = client.Install (SRC_NODE_1);
   apps_client.Start (Seconds (2.0));
   apps_client.Stop (Seconds (12.0));


  Simulator::Run ();
  // Trace packet received
 Ptr <UdpServer> serverUDP =  server.GetServer();
 uint64_t numPktRev  = serverUDP->GetReceived();
 NS_LOG_UNCOND("Number of received packet is: " << numPktRev <<"\n");


Simulator::Destroy ();

}


