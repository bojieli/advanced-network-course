#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("InterCluster");

int main (int argc, char *argv[])
{
    const int interval = 60;
    const int port = 50000;

    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

    // Use ECMP routing
    Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));

    NodeContainer nodes[11];
    Ipv4InterfaceContainer interfaces[11];
    PointToPointHelper p2p[11];
    CsmaHelper csma[11];
    NetDeviceContainer netdev[11];

    // ToR switch
    for (int i=1; i<=4; i++) {
        nodes[i].Create(3); // 0 is ToR switch, 1 and 2 are hosts
    }

    // aggregation switch
    for (int i=5; i<=6; i++) {
        nodes[i].Create(1); // 0 is agg. switch
        for (int j=1; j<=4; j++)
            nodes[i].Add(nodes[j].Get(0)); // ToR
    }
    
    // core switch (2 p2p links)
    // nodes[7]: 0 is core switch, 1 is a1
    nodes[7].Create(1); // create core switch
    nodes[7].Add(nodes[5].Get(0));
    // nodes[8]: 0 is core switch, 1 is a2
    nodes[8].Add(nodes[7].Get(0));
    nodes[8].Add(nodes[6].Get(0));

    // additional core switch (2 p2p links)
    // nodes[9]: 0 is core switch, 1 is a1
    nodes[9].Create(1);
    nodes[9].Add(nodes[5].Get(0));
    // nodes[10]: 0 is core switch, 1 is a2
    nodes[10].Add(nodes[9].Get(0));
    nodes[10].Add(nodes[6].Get(0));

    // initialize channel attributes and IP addresses for each subnet
    for (int i=1; i<=10; i++) {
        InternetStackHelper stack;
        if (i<=4) // install InternetStack for ToR and core switches
            stack.Install(nodes[i]);
        else if (i<=7 || i==9) // for aggregation switches and core switch, do not re-install InternetStack
            stack.Install(nodes[i].Get(0));

        if (i>=7) { // core switch: point-to-point network
            p2p[i].SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
            p2p[i].SetChannelAttribute("Delay", StringValue("500ns"));
            netdev[i] = p2p[i].Install(nodes[i]);
        }
        else { // other switches: Ethernet
            csma[i].SetChannelAttribute("DataRate", StringValue("1.0Mbps"));
            csma[i].SetChannelAttribute("Delay", StringValue("500ns"));
            netdev[i] = csma[i].Install(nodes[i]);
        }

        char base_addr[100];
        if (i<=4) // ToR
            sprintf(base_addr, "10.0.%d.0", i);
        else if (i<=6) // aggregation
            sprintf(base_addr, "10.%d.1.0", i-4);
        else // core
            sprintf(base_addr, "192.168.%d.0", i-6);

        Ipv4AddressHelper addr;
        addr.SetBase(base_addr, "255.255.255.0");
        interfaces[i] = addr.Assign(netdev[i]);
    }

    ApplicationContainer clientApp[5];
    ApplicationContainer sinkApp[5];

    // listen on h2, h4, h6, h8
    for (int i=1; i<=4; i++) {
        PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkApp[i] = packetSinkHelper.Install (nodes[i].Get(1));
        sinkApp[i].Start(Seconds (0));
        sinkApp[i].Stop(Seconds (1.0 + interval));
    }

    // initiate TCP flows on h1, h3, h5, h7
    for (int i=1; i<=4; i++) {
        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces[5-i].GetAddress(1), port + 5 - i));
        client.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=50]"));
        client.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        // buffer size (network layer will fragment it if necessary)
        client.SetAttribute ("PacketSize", UintegerValue(4096));

        clientApp[i] = client.Install (nodes[i].Get(0));
        clientApp[i].Start(Seconds (0));
        clientApp[i].Stop (Seconds (interval));
    }

    // initialize routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // capture on each receiver
    // do not enable promicious mode
    for (int i=1; i<=4; i++)
        csma[i].EnablePcap("two-core-inter-cluster", netdev[i].Get(1), false);

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
