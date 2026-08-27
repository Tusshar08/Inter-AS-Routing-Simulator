#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterASRoutingLite");

struct FlowSnapshot
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint32_t lostPackets = 0;

  Time delaySum = Seconds(0);
  Time jitterSum = Seconds(0);
};

FlowSnapshot snapshot6;
FlowSnapshot snapshot9;
FlowSnapshot snapshot13;

static void Fail(Ptr<Node> as1, Ptr<Node> as4)
{
  Ptr<Ipv4> ipv4As1 = as1->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4As4 = as4->GetObject<Ipv4>();

  ipv4As1->SetDown(2);
  ipv4As4->SetDown(2);

  Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

  std::cout << "TIME " << Simulator::Now().GetSeconds() << " s: AS1-AS4 LINK FAILED\n";
  std::cout << "Traffic should reroute via AS1-AS2-AS3-AS4\n";
}

static void Restore(Ptr<Node> as1, Ptr<Node> as4)
{
  Ptr<Ipv4> ipv4As1 = as1->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4As4 = as4->GetObject<Ipv4>();

  ipv4As1->SetUp(2);
  ipv4As4->SetUp(2);

  Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

  std::cout << "TIME " << Simulator::Now().GetSeconds() << " s: AS1-AS4 LINK RESTORED\n";
  std::cout << "Traffic should return to AS1-AS4\n";
}

static FlowSnapshot GetSnapshot (Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier)
{
  monitor->CheckForLostPackets ();

  std::map<FlowId, FlowMonitor::FlowStats> stats =
      monitor->GetFlowStats ();

  FlowSnapshot snapshot;

  for (auto const &entry : stats)
    {
      Ipv4FlowClassifier::FiveTuple tuple =
          classifier->FindFlow (entry.first);

      if (tuple.destinationAddress ==
          Ipv4Address ("192.168.3.2") &&
          tuple.destinationPort == 8080 &&
          tuple.protocol == 17)
        {
          snapshot.txPackets += entry.second.txPackets;
          snapshot.rxPackets += entry.second.rxPackets;
          snapshot.lostPackets += entry.second.lostPackets;

          snapshot.delaySum += entry.second.delaySum;
          snapshot.jitterSum += entry.second.jitterSum;
        }
    }

  return snapshot;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponentEnable("InterASRoutingLite", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO("Starting PartD: Failure and recovery...");

  NodeContainer routers;
  routers.Create(4);

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

  // AS3 -- AS4
  PointToPointHelper p2p34;
  p2p34.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
  p2p34.SetChannelAttribute("Delay", StringValue("3ms"));
  NetDeviceContainer d34 = p2p34.Install(routers.Get(2), routers.Get(3));

  // AS1 -- AS4 (direct peering)
  PointToPointHelper p2p14;
  p2p14.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p14.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d14 = p2p14.Install(routers.Get(0), routers.Get(3));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

  ipv4.SetBase("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

  ipv4.SetBase("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = ipv4.Assign(d34);

  ipv4.SetBase("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i14 = ipv4.Assign(d14);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 8080;
  UdpEchoServerHelper server(port);
  ApplicationContainer apps = server.Install(routers.Get(3));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(13.0));

  UdpEchoClientHelper client(i34.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(55));
  client.SetAttribute("Interval", TimeValue(Seconds(0.2)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  apps = client.Install(routers.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(13.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  // Fail AS1-AS4 at t = 6 seconds
  Simulator::Schedule(Seconds(6.0), &Fail, routers.Get(0), routers.Get(3));

  // FlowMonitor snapshot just after failure
  Simulator::Schedule(Seconds(6.000001), [&monitor,  &classifier]()
      { snapshot6 = GetSnapshot(monitor, classifier); 
      });

  // Restore AS1-AS4 at t = 9 seconds
  Simulator::Schedule(Seconds(9.0), &Restore, routers.Get(0), routers.Get(3));

  // FlowMonitor snapshot just after recovery
  Simulator::Schedule(Seconds(9.000001), [&monitor,  &classifier]()
  {
    snapshot9 = GetSnapshot(monitor, classifier); 
  });

  // Final snapshot at t = 13 seconds
  Simulator::Schedule(Seconds(13.0), [&monitor,  &classifier]()
  {
    snapshot13 = GetSnapshot(monitor, classifier); 
  });

  // Optional: if you add ns-3's NetAnim module (AnimationInterface) here to
  // visualize packet flow across the topology, screenshots of it can
  // strengthen your report and are worth bringing to the viva -- see the
  // note in the assignment write-up. It's not required.

  Simulator::Stop(Seconds(14.0));
  Simulator::Run();

  monitor->SerializeToXmlFile("interas-partD-results.xml", true, true);

  uint32_t rxBefore = snapshot6.rxPackets;
  uint32_t lostBefore = snapshot6.lostPackets;
  double delayBefore = (rxBefore > 0) ? snapshot6.delaySum.GetSeconds () / rxBefore * 1000.0 : 0.0;
  double jitterBefore = (rxBefore > 1) ? snapshot6.jitterSum.GetSeconds () / (rxBefore - 1) * 1000.0 : 0.0;
  uint32_t rxDuring = snapshot9.rxPackets - snapshot6.rxPackets;
  uint32_t lostDuring = snapshot9.lostPackets - snapshot6.lostPackets;
  double delayDuring = (rxDuring > 0) ? (snapshot9.delaySum - snapshot6.delaySum).GetSeconds () / rxDuring * 1000.0 : 0.0;
  double jitterDuring = (rxDuring > 1) ? (snapshot9.jitterSum - snapshot6.jitterSum).GetSeconds () / (rxDuring - 1) * 1000.0 : 0.0;

  uint32_t rxAfter = snapshot13.rxPackets - snapshot9.rxPackets;
  uint32_t lostAfter = snapshot13.lostPackets - snapshot9.lostPackets;
  double delayAfter = (rxAfter > 0) ? (snapshot13.delaySum - snapshot9.delaySum).GetSeconds () / rxAfter * 1000.0 : 0.0;
  double jitterAfter = (rxAfter > 1) ? (snapshot13.jitterSum - snapshot9.jitterSum).GetSeconds () / (rxAfter - 1) * 1000.0 : 0.0;

  //PRINT REQUIRED RESULTS

  std::cout << "\nBEFORE FAILURE (0-6 s)\n";
  std::cout << "Packets delivered : " << rxBefore << "\n";
  std::cout << "Packets lost      : " << lostBefore << "\n";
  std::cout << "Average delay     : " << delayBefore << " ms\n";
  std::cout << "Jitter            : " << jitterBefore << " ms\n";

  std::cout << "\nDURING FAILURE (6-9 s)\n";
  std::cout << "Packets delivered : " << rxDuring << "\n";
  std::cout << "Packets lost      : " << lostDuring << "\n";
  std::cout << "Average delay     : " << delayDuring << " ms\n";
  std::cout << "Jitter            : " << jitterDuring << " ms\n";

  std::cout << "\nAFTER RECOVERY (9-13 s)\n";
  std::cout << "Packets delivered : " << rxAfter << "\n";
  std::cout << "Packets lost      : " << lostAfter << "\n";
  std::cout << "Average delay     : " << delayAfter << " ms\n";
  std::cout << "Jitter            : " << jitterAfter << " ms\n";

  //SAVE GRAPH DATA

  std::ofstream csv ("partD-delay-plot.csv");

  csv << "Interval,AverageDelayMs\n";
  csv << "Before," << delayBefore << "\n";
  csv << "During," << delayDuring << "\n";
  csv << "After," << delayAfter << "\n";

  csv.close ();


  // SAVE TEXT RESULTS

  std::ofstream out ("partD-interval-stats.txt");

  out << "COL724 Part D - Failure and Recovery Study\n\n";

  out << "BEFORE FAILURE (0-6 s)\n";
  out << "Packets delivered : " << rxBefore << "\n";
  out << "Packets lost      : " << lostBefore << "\n";
  out << "Average delay     : " << delayBefore << " ms\n";
  out << "Jitter            : " << jitterBefore << " ms\n\n";

  out << "DURING FAILURE (6-9 s)\n";
  out << "Packets delivered : " << rxDuring << "\n";
  out << "Packets lost      : " << lostDuring << "\n";
  out << "Average delay     : " << delayDuring << " ms\n";
  out << "Jitter            : " << jitterDuring << " ms\n\n";

  out << "AFTER RECOVERY (9-13 s)\n";
  out << "Packets delivered : " << rxAfter << "\n";
  out << "Packets lost      : " << lostAfter << "\n";
  out << "Average delay     : " << delayAfter << " ms\n";
  out << "Jitter            : " << jitterAfter << " ms\n";

  out.close ();

  Simulator::Destroy();
  return 0;
}
