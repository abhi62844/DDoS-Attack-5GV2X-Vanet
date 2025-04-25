#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include <ns3/antenna-module.h>
#include "ns3/udp-client-server-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include <ns3/buildings-helper.h>

#include <iostream>

using namespace ns3;

int main(int argc, char *argv[])
{
    //complete 5G
    // Enable logging for the animation interface
    LogComponentEnable("AnimationInterface", LOG_LEVEL_INFO);

    // Number of vehicles (nodes representing the vehicles)
   

    // Central frequency and bandwidth for NR (New Radio)
    double frequency = 28e9;      // 28 GHz (mmWave)
    double bandwidth = 100e6;     // 100 MHz bandwidth
    double hBS = 25;          // Base station height
    double hUT = 1.5;          // User terminal (UE) antenna height
    double speed = 1;             // Speed of UEs in m/s (walking)
    double txPower = 40;          // Transmission power in dBm
    bool mobility = false;         // Enable mobility
    BandwidthPartInfo::Scenario scenario = BandwidthPartInfo::UMa; // Urban Macro scenario

    // Create nodes representing the vehicles and base stations
    NodeContainer vehicles;
    NodeContainer base;
    vehicles.Create(7); // Create 2 vehicle nodes
    base.Create(2);     // Create 2 base station nodes

    // Define positions for base stations
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();

    enbPositionAlloc->Add(Vector(0.0, 0.0, hBS));  // Position of the first base station
    enbPositionAlloc->Add(Vector(0.0, 80.0, hBS)); // Position of the second base station
    MobilityHelper enbmobility;
    enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Constant position model
    enbmobility.SetPositionAllocator(enbPositionAlloc); // Assign positions
    enbmobility.Install(base); // Install mobility to the base stations

    // Position the mobile terminals and enable mobility
    MobilityHelper uemobility;
    uemobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel"); // Constant velocity model for UEs
    uemobility.Install(vehicles); // Install mobility to the vehicle nodes

    // Define the mobility behavior for the UEs based on the mobility flag
    if (mobility)
    {
        // Set initial position and velocity for the first UE
        vehicles.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(90, 15, hUT)); // Position (x, y, z)
        vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, speed, 0)); // Velocity (x, y, z)
   
        // Set initial position and velocity for the second UE
        vehicles.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(30, 50.0, hUT)); // Position (x, y, z)
        vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(-speed, 0, 0)); // Velocity (x, y, z)
   
    }
    else
    {
        // Set positions for UEs if mobility is disabled (no movement)
        vehicles.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(45, 0, hUT));
        vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

        vehicles.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(30, 50.0, hUT));
        vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

    }


    // 13.0.0.x, 14.0.0.x	Internal point-to-point interfaces (e.g., gNB ↔ SGW, SGW ↔ PGW)
    /*
     * Create NR (New Radio) helpers
     */
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>(); // EPC helper
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>(); // Beamforming helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>(); // Main NR helper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper); // Set the beamforming helper
    nrHelper->SetEpcHelper(epcHelper); // Set the EPC helper
    
    // NR configuration for communication
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator; // Bandwidth part creator
    const uint8_t numCcPerBand = 1;

    // Define the configuration of the operation band (frequency, bandwidth, and scenario)
    CcBwpCreator::SimpleOperationBandConf bandConf(frequency, bandwidth, numCcPerBand, scenario);

    // Create the operation band based on the configuration
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    nrHelper->InitializeOperationBand(&band); // Initialize the operation band with the helper
    allBwps = CcBwpCreator::GetAllBwps({band}); // Get all bandwidth parts
    
    // Set ideal beamforming method
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // Set scheduler type for NR
    nrHelper->SetSchedulerTypeId(NrMacSchedulerTdmaRR::GetTypeId());

    // Configure UEs antennas
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Configure gNBs antennas
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

                                     
    // Install NR devices (gNB and UE)
    NetDeviceContainer Uedevices = nrHelper->InstallUeDevice(vehicles, allBwps); // Install on vehicles (UEs)
    NetDeviceContainer Enbdevices = nrHelper->InstallGnbDevice(base, allBwps); // Install on base stations (gNBs)
       
    // Assign random streams to devices for randomization purposes
    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(Enbdevices, randomStream);
    randomStream += nrHelper->AssignStreams(Uedevices, randomStream);

    // Set transmission power for gNBs and UEs
    nrHelper->GetGnbPhy(Enbdevices.Get(0), 0)->SetTxPower(txPower);
    nrHelper->GetUePhy(Uedevices.Get(1), 0)->SetTxPower(txPower);
    std::cout<<"hello\n";
    // Update the configuration for gNB and UE devices
    for (auto it = Enbdevices.Begin(); it != Enbdevices.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = Uedevices.Begin(); it != Uedevices.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }

    // Create the internet stack and install on the UEs
    Ptr<Node> pgw = epcHelper->GetPgwNode(); // Get the PGW node (packet gateway)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1); // Create the remote host
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer); // Install internet stack on the remote host

    // Connect the remote host to the PGW and setup routing
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s"))); // Data rate of the point-to-point link
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500)); // MTU size
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010))); // Link delay
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost); // Install devices on PGW and remote host

    // Assign IP addresses to the remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0"); // Base IP address
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Setup routing for the remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Configure internet protocol on the UEs and gNBs
    internet.Install(vehicles);
    internet.Install(base);
    Ptr<ListPositionAllocator> fixedAlloc = CreateObject<ListPositionAllocator>();
    fixedAlloc->Add(Vector(100.0, 100.0, 0)); // Position for PGW
    fixedAlloc->Add(Vector(120.0, 100.0, 0)); // Position for RemoteHost

    MobilityHelper fixedMobility;
    fixedMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    fixedMobility.SetPositionAllocator(fixedAlloc);
    fixedMobility.Install(NodeContainer(pgw, remoteHost));
    // Setup IP addresses for the UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(Uedevices));

    // Attach UEs to the network (associate with gNB)
    nrHelper->AttachToClosestGnb(Uedevices, Enbdevices);

    // Setup routes on the UEs
    for (uint32_t u = 0; u < vehicles.GetN(); ++u)
    {
        Ptr<Node> ueNode = vehicles.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }
    std::cout<<"hello\n";
    // UDP server to receive packets
    for(uint16_t i=0;i<vehicles.GetN();++i){
     uint16_t dlPort = 10000;
     UdpServerHelper udpServer(dlPort);
     ApplicationContainer serverApp = udpServer.Install(vehicles.Get(i));
     serverApp.Start(Seconds(0.0));
     serverApp.Stop(Seconds(30.0));
    }
    // UDP client to send packets
    uint16_t dlPort = 10000; // Downlink port number
    for(uint16_t i=0;i<vehicles.GetN();++i){
    UdpClientHelper udpClient(ueIpIface.GetAddress(i), dlPort); // Create UDP client
    udpClient.SetAttribute("MaxPackets", UintegerValue(320)); // Maximum number of packets
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // Interval between packets
    udpClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size
    ApplicationContainer clientApp = udpClient.Install(remoteHost);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(30.0));
    }
    // Install animation interface for visualizing the simulation
    AnimationInterface anim("scenario-vanet-nr.xml");
    anim.EnablePacketMetadata(true); // Enable packet metadata in the animation
    anim.EnableIpv4RouteTracking("scenario-nr.xml", Seconds(0.0), Seconds(30), Seconds(0.25)); // Enable route tracking
    for (uint32_t i = 0; i < base.GetN (); ++i)
    {
        anim.UpdateNodeColor (base.Get (i), 255, 0, 0); // (Red, Green, Blue)
    }

    // Change color for UE nodes (e.g., Blue)
    for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
        anim.UpdateNodeColor (vehicles.Get (i), 0, 0, 255); // (Red, Green, Blue)
    }
     pgw = epcHelper->GetPgwNode ();
    anim.UpdateNodeColor (pgw, 128, 128, 128);
    // Ptr<Node> remoteHost = remoteHostContainer.Get (0);
    anim.UpdateNodeColor(remoteHost,0,0,0);
    anim.UpdateNodeDescription(remoteHost,"remotehost");
    anim.UpdateNodeDescription (base.Get (0), "gNB-1");
    anim.UpdateNodeDescription (base.Get (1), "gNB-2");
    anim.UpdateNodeDescription (vehicles.Get (0), "UE-1");
    anim.UpdateNodeDescription (vehicles.Get (1), "UE-2");
    anim.UpdateNodeDescription (pgw, "PGW");
    // Run the simulation
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
   
    Simulator::Stop(Seconds(100.0)); // Simulation duration
    Simulator::Run();
    
    std::cout << "\n--- Vehicle IP Addresses ---\n";
    for (uint32_t i = 0; i < vehicles.GetN(); i++)
    {
        Ptr<Ipv4> ipv4 = vehicles.Get(i)->GetObject<Ipv4>();
        Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
        std::cout << "Vehicle " << i << " IP: " << ipAddr << std::endl;
    }
    std::cout<<"\n-----------gNBs IP Addresses--------------\n";
    
    for(uint32_t i=0;i<base.GetN();++i){
        Ptr<Ipv4> ipv4 = base.Get(i)->GetObject<Ipv4>();
        Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
        std::cout << "gNB " << i << " IP: " << ipAddr << std::endl;

    }
    Ptr<Ipv4> ipv4 = remoteHost->GetObject<Ipv4>();
    Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
    std::cout << "Remotehost: " << ipAddr << std::endl;
    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

for (auto &flow : stats) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
  std::cout << "Flow ID: " << flow.first << " Src Addr: " << t.sourceAddress
            << " Dst Addr: " << t.destinationAddress << std::endl;
  std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
  std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
  std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
  std::cout << "  Delay Sum: " << flow.second.delaySum.GetSeconds() << " s" << std::endl;
  std::cout << "  Throughput: "
            << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() -
                flow.second.timeFirstTxPacket.GetSeconds()) / 1024)
            << " Kbps" << std::endl;
  std::cout << "------------------------------------------" << std::endl;
}

    Simulator::Destroy();
    return 0;
}