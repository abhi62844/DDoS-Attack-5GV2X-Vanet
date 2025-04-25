#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_DDoS_Logging");

int
main()
{
    // Enable Logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 1. Create Nodes (Vehicles + FAP)
    NodeContainer vehicles, fap;
    vehicles.Create(5); // 5 Vehicles
    fap.Create(1);      // 1 Fixed Access Point (FAP)

    // 2. Set Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(20.0),
                                  "DeltaY",
                                  DoubleValue(20.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < vehicles.GetN(); i++)
    {
        Ptr<ConstantVelocityMobilityModel> mob =
            vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(10, 0, 0)); // Vehicles move at 10m/s
    }

    MobilityHelper staticMobility;
    staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    staticMobility.Install(fap); // FAP is stationary

    // 3. Setup Wi-Fi Network (No AODV)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for VANET

    WifiMacHelper mac;
    Ssid ssid = Ssid("VANET-5G-V2X");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer vehicleDevices = wifi.Install(phy, mac, vehicles);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer fapDevice = wifi.Install(phy, mac, fap);

    // 4. Install Internet Stack (No AODV, just basic routing)
    InternetStackHelper stack;
    stack.Install(vehicles);
    stack.Install(fap);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
    Ipv4InterfaceContainer fapInterface = address.Assign(fapDevice);

    // 6. Setup Normal UDP Traffic (Client-Server Model)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(fap.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient(fapInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
   for (uint32_t i = 0; i < 5; ++i){
    ApplicationContainer normalClient = echoClient.Install(vehicles.Get(i));
    normalClient.Start(Seconds(2.0));
    normalClient.Stop(Seconds(30.0));
   }
    // 7. Setup DDoS Attack (UDP Flood)
    OnOffHelper ddosAttack("ns3::UdpSocketFactory",
                           InetSocketAddress(fapInterface.GetAddress(0), 9));
    ddosAttack.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    ddosAttack.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ddosAttack.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ddosAttack.SetAttribute("PacketSize", UintegerValue(1024));

    // for (uint32_t i = 1; i < 5; ++i)
    // {
    //     ApplicationContainer attackApps = ddosAttack.Install(vehicles.Get(i));
    //     attackApps.Start(Seconds(1.0));
    //     attackApps.Stop(Seconds(30.0));
    // }

// 8. Setup FlowMonitor for Packet Loss & Delay Analysis
Ptr<FlowMonitor> flowMonitor;
FlowMonitorHelper flowHelper;
flowMonitor = flowHelper.InstallAll();

// 9. Setup NetAnim Visualization
AnimationInterface anim("vanet-ddos.xml");
anim.EnablePacketMetadata(true);
anim.EnableIpv4RouteTracking("vanet-ddos-route.xml", Seconds(0), Seconds(30), Seconds(1));

// 10. Run Simulation
Simulator::Stop(Seconds(31.0));
Simulator::Run();

// 11. Analyze Results (Packet Loss & Delay)
flowMonitor->CheckForLostPackets();
Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

std::cout << "\n--- Simulation Results ---\n";
for (auto& flow : stats)
{
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
    std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress
              << ")\n";
    std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
    std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << (flow.second.txPackets - flow.second.rxPackets) << "\n";
    std::cout << "  Packet Loss Ratio: "
              << ((flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets)
              << "%\n";
}

Simulator::Destroy();
return 0;
}
