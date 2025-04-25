#include "ns3/core-module.h"

#include "ns3/network-module.h"

#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"

#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DDoSAttack");

int main (int argc, char *argv[])

{

// Set up logging

LogComponentEnable ("DDoSAttack", LOG_LEVEL_INFO);

// Create nodes

NodeContainer attackerNodes;

attackerNodes.Create (5); // Create 5 attacker nodes

NodeContainer victimNode;

victimNode.Create (1); // Create 1 victim node

// Create point-to-point links

PointToPointHelper pointToPoint;

pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));

pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

NetDeviceContainer devices;

for (uint32_t i = 0; i < attackerNodes.GetN (); ++i)

{

devices.Add (pointToPoint.Install (attackerNodes.Get (i), victimNode.Get (0)));

}

// Install the internet stack

InternetStackHelper stack;

stack.Install (attackerNodes);

stack.Install (victimNode);

// Assign IP addresses

Ipv4AddressHelper address;

address.SetBase ("10.1.1.0", "255.255.255.0");

Ipv4InterfaceContainer interfaces = address.Assign (devices);

// Install applications on attacker nodes

uint16_t port = 9; // Discard port (RFC 863)

for (uint32_t i = 0; i < attackerNodes.GetN (); ++i)

{

OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (i + 1), port));

onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));

onoff.SetAttribute ("PacketSize", UintegerValue (1024));

onoff.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));

onoff.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));

ApplicationContainer app = onoff.Install (attackerNodes.Get (i));

app.Start (Seconds (1.0));

app.Stop (Seconds (10.0));

}

// Install a packet sink on the victim node to receive packets

PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));

ApplicationContainer sinkApp = sink.Install (victimNode.Get (0));

sinkApp.Start (Seconds (0.0));

sinkApp.Stop (Seconds (10.0));

// Run simulation

Simulator::Run ();

Simulator::Destroy ();

return 0;

}