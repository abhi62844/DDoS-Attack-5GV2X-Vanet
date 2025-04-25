#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("DDoSAttack");

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("DDoSAttack", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer attackerNodes;
    attackerNodes.Create(5); // Create 5 attacker nodes

    NodeContainer victimNode;
    victimNode.Create(1); // Create 1 victim node

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < attackerNodes.GetN(); ++i)
    {
        devices.Add(pointToPoint.Install(attackerNodes.Get(i), victimNode.Get(0)));
    }

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(attackerNodes);
    stack.Install(victimNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install applications on attacker nodes
    uint16_t port = 9; // Discard port (RFC 863)
    for (uint32_t i = 0; i < attackerNodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(i + 1), port));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer app = onoff.Install(attackerNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));
    }

    // Install a packet sink on the victim node to receive packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(victimNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    AnimationInterface anim("scenario.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("scenario-5g.xml",Seconds(0.0), Seconds(30), Seconds(0.25));
    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    std::cout << "\n--- Node IP Addresses ---\n";
    for (uint32_t i = 0; i < attackerNodes.GetN(); i++)
    {
        Ptr<Ipv4> ipv4 = attackerNodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
        std::cout << "Vehicle " << i << " IP: " << ipAddr << std::endl;
    }

    Ptr<Ipv4> ipv4 = victimNode.Get(0)->GetObject<Ipv4>();
    Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
    std::cout << "Victim IP: " << ipAddr << std::endl;
    // Analyze packet loss
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::cout << "\n--- Simulation Results ---\n";
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Transmitted Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Received Packets: " << flow.second.rxPackets << "\n";

        uint32_t lostPackets = flow.second.txPackets - flow.second.rxPackets;
        double lossPercentage = (flow.second.txPackets > 0) ? (lostPackets * 100.0 / flow.second.txPackets) : 0.0;

        std::cout << "  Lost Packets: " << lostPackets << "\n";
        std::cout << "  Packet Loss Percentage: " << lossPercentage << "%\n\n";
    }

    Simulator::Destroy();
    return 0;
}
