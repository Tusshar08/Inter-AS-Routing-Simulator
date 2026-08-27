#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterASRoutingLite");

bool failurePathObserved = false;
bool recoveryPathObserved = false;
bool defaultPathObserved = false;

double failureConvergenceTime = -1.0;
double recoveryConvergenceTime = -1.0;

uint32_t failureTx = 0;
uint32_t failureRx = 0;
uint32_t recoveryTx = 0;
uint32_t recoveryRx = 0;

static void Fail(Ptr<Node> as1, Ptr<Node> as4)
{
  Ptr<Ipv4> ipv4As1 = as1->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4As4 = as4->GetObject<Ipv4>();

  ipv4As1->SetDown(2);
  ipv4As4->SetDown(1);

  Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

  std::cout << "TIME " << Simulator::Now().GetSeconds() << " s: AS1-AS4 LINK FAILED\n";
  std::cout << "Traffic should reroute via AS1-AS2-AS3-AS5\n";
}

static void Restore(Ptr<Node> as1, Ptr<Node> as4)
{
  Ptr<Ipv4> ipv4As1 = as1->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4As4 = as4->GetObject<Ipv4>();

  ipv4As1->SetUp(2);
  ipv4As4->SetUp(1);

  Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

  std::cout << "TIME " << Simulator::Now().GetSeconds() << " s: AS1-AS4 LINK RESTORED\n";
  std::cout << "Traffic should return to AS1-AS4-AS5\n";
}

static void OnClientTx(Ptr<const Packet> packet)
{
  double now = Simulator::Now().GetSeconds();
  if (now >= 6.0 && now < 9.0) failureTx++;
  if (now >= 9.0 && now < 13.0) recoveryTx++;
}

static void OnServerRx (Ptr<const Packet> packet, const Address &from)
{
  InetSocketAddress sender = InetSocketAddress::ConvertFrom (from);
  Ipv4Address source = sender.GetIpv4 ();
  double now = Simulator::Now ().GetSeconds ();

  if (!defaultPathObserved && now < 6.0 && source == Ipv4Address ("192.168.4.1"))
  {
    defaultPathObserved = true;
    std::cout << "Default path observed: R1-R4-R5\n";
    std::cout << "Reason: 2 hops versus 3 hops for R1-R2-R3-R5\n";
  }

  if (now >= 6.0 && !failurePathObserved && source == Ipv4Address ("192.168.1.1"))
    {
      failurePathObserved = true;
      failureConvergenceTime = now - 6.0;
      std::cout << "First packet on alternate path at t=" << now << " s\n";
      std::cout << "Failure convergence time = " << failureConvergenceTime * 1000.0 << " ms\n";
    }

  if (now >= 9.0 && !recoveryPathObserved && source == Ipv4Address ("192.168.4.1"))
    {
      recoveryPathObserved = true;
      recoveryConvergenceTime = now - 9.0;
      std::cout << "First packet on restored path at t=" << now << " s\n";
      std::cout << "Recovery convergence time = " << recoveryConvergenceTime * 1000.0 << " ms\n";
    }
    if (now >= 6.0 && now < 9.0) failureRx++;
    if (now >= 9.0 && now < 13.0) recoveryRx++;
}
  

static FlowMonitor::FlowStats GetServerFlowStats(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, Ipv4Address serverAddress, uint16_t port)
{
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStats result;
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto &entry : stats){
      Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(entry.first);
      
      if (tuple.destinationAddress == serverAddress && tuple.destinationPort == port && tuple.protocol == 17){
        result = entry.second;
        break;
      }
    }
    return result;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponentEnable("InterASRoutingLite", LOG_LEVEL_INFO);

  NS_LOG_INFO("Starting PartE: Dual-path rerouting study...");

  NodeContainer routers;
  routers.Create(5);

  InternetStackHelper internet;
  internet.Install(routers);

  // AS1 -- AS2
  PointToPointHelper p2p12;
  p2p12.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
  p2p12.SetChannelAttribute("Delay", StringValue("3ms"));
  NetDeviceContainer d12 = p2p12.Install(routers.Get(0), routers.Get(1));

  // AS2 -- AS3
  PointToPointHelper p2p23;
  p2p23.SetDeviceAttribute("DataRate", StringValue("6Mbps"));
  p2p23.SetChannelAttribute("Delay", StringValue("4ms"));
  NetDeviceContainer d23 = p2p23.Install(routers.Get(1), routers.Get(2));

  // AS3 -- AS5
  PointToPointHelper p2p35;
  p2p35.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
  p2p35.SetChannelAttribute("Delay", StringValue("3ms"));
  NetDeviceContainer d35 = p2p35.Install(routers.Get(2), routers.Get(4));

  // AS1 -- AS4
  PointToPointHelper p2p14;
  p2p14.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p14.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d14 = p2p14.Install(routers.Get(0), routers.Get(3));

  // AS4 -- AS5
  PointToPointHelper p2p45;
  p2p45.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p45.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d45 = p2p45.Install(routers.Get(3), routers.Get(4));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

  ipv4.SetBase("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

  ipv4.SetBase("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i35 = ipv4.Assign(d35);

  ipv4.SetBase("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i14 = ipv4.Assign(d14);

  ipv4.SetBase("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i45 = ipv4.Assign(d45);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  UdpEchoServerHelper server(port);
  ApplicationContainer apps = server.Install(routers.Get(4));
  apps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&OnServerRx));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(13.0));

  UdpEchoClientHelper client(i35.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(55));
  client.SetAttribute("Interval", TimeValue(Seconds(0.2)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  apps = client.Install(routers.Get(0));
  apps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&OnClientTx));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(13.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  // Fail AS1-AS4 at t = 6 seconds
  Simulator::Schedule(Seconds(6.0), &Fail, routers.Get(0), routers.Get(3));
  // Restore AS1-AS4 at t = 9 seconds
  Simulator::Schedule(Seconds(9.0), &Restore, routers.Get(0), routers.Get(3));

  // Optional: if you add ns-3's NetAnim module (AnimationInterface) here to
  // visualize packet flow across the topology, screenshots of it can
  // strengthen your report and are worth bringing to the viva -- see the
  // note in the assignment write-up. It's not required.

  Simulator::Stop(Seconds(14.0));
  Simulator::Run();

  uint32_t failureLost = failureTx - failureRx;
  uint32_t recoveryLost = recoveryTx - recoveryRx;

  std::cout << "\nFailure interval (6-9 s):\n";
  std::cout << "Packets transmitted: " << failureTx << "\n";
  std::cout << "Packets delivered: " << failureRx << "\n";
  std::cout << "Packets lost: " << failureLost << "\n";

  std::cout << "\nRecovery interval (9-13 s):\n";
  std::cout << "Packets transmitted: " << recoveryTx << "\n";
  std::cout << "Packets delivered: " << recoveryRx << "\n";
  std::cout << "Packets lost: " << recoveryLost << "\n";

  monitor->SerializeToXmlFile("interas-partE-results.xml", true, true);
  
  FlowMonitor::FlowStats totalStats = GetServerFlowStats(monitor, classifier, i35.GetAddress(1), port);
  std::cout << "Total packets transmitted: " << totalStats.txPackets << "\n";
  std::cout << "Total packets delivered: " << totalStats.rxPackets << "\n";
  std::cout << "Total packets lost: " << totalStats.lostPackets << "\n\n";

  if (failurePathObserved)
    {
      std::cout << "Failure route change: " << "R1-R4-R5 -> R1-R2-R3-R5\n";
      std::cout << "Failure convergence time: " << failureConvergenceTime * 1000.0 << " ms\n";
    }

  if (recoveryPathObserved)
    {
      std::cout << "Recovery route change: " << "R1-R2-R3-R5 -> R1-R4-R5\n";
      std::cout << "Recovery convergence time: " << recoveryConvergenceTime * 1000.0<< " ms\n";
    }

  Simulator::Destroy();
  return 0;
}
