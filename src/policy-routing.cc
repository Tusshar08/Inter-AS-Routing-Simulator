#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("InterASRoutingLite");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  std::string mode = "static";
  CommandLine cmd;
  cmd.AddValue("mode", "Policy mechanism: static or metric", mode);
  cmd.Parse (argc, argv);

  if (mode != "static" && mode != "metric")
    {
      std::cout << "Invalid mode. Use --mode=static or --mode=metric" << std::endl;
      return 1;
    }

  LogComponentEnable ("InterASRoutingLite", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Starting PartC: Policy mode = " << mode);

  NodeContainer routers;
  routers.Create (4);

  InternetStackHelper internet;
  internet.Install (routers);

  // AS1 -- AS2
  PointToPointHelper p2p12;
  p2p12.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
  p2p12.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer d12 = p2p12.Install (routers.Get (0), routers.Get (1));

  // AS2 -- AS3
  PointToPointHelper p2p23;
  p2p23.SetDeviceAttribute ("DataRate", StringValue ("6Mbps"));
  p2p23.SetChannelAttribute ("Delay", StringValue ("4ms"));
  NetDeviceContainer d23 = p2p23.Install (routers.Get (1), routers.Get (2));

  // AS3 -- AS4
  PointToPointHelper p2p34;
  p2p34.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
  p2p34.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer d34 = p2p34.Install (routers.Get (2), routers.Get (3));

  //AS1 -- AS4 (direct peering)
  PointToPointHelper p2p14;
  p2p14.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p14.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d14 = p2p14.Install (routers.Get (0), routers.Get (3));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = ipv4.Assign (d34);

  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i14 = ipv4.Assign (d14);

  if (mode == "metric")
    {
      Ptr<Ipv4> ipv4As1 = routers.Get (0)->GetObject<Ipv4> ();

      ipv4As1->SetMetric (1, 1);   // Prefer AS1 -> AS2
      ipv4As1->SetMetric (2, 10);  // Discourage AS1 -> AS4
				   
      Ptr<Ipv4> ipv4As4 = routers.Get (3)->GetObject<Ipv4> ();
      
      ipv4As4->SetMetric (1, 1);   // Prefer AS4 -> AS3
      ipv4As4->SetMetric (2, 10);  // Discourage AS4 -> AS1
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  if (mode == "static") {
	  Ipv4StaticRoutingHelper staticRoutingHelper;
	  
	  // AS1: force destination through AS2
	  Ptr<Ipv4> ipv4As1 = routers.Get (0)->GetObject<Ipv4> ();

          Ptr<Ipv4StaticRouting> staticRoutingAs1 = staticRoutingHelper.GetStaticRouting (ipv4As1);

          staticRoutingAs1->AddHostRouteTo (Ipv4Address ("192.168.3.2"), Ipv4Address ("192.168.1.2"), 1);

          // AS2: force the packet onward toward AS3
          Ptr<Ipv4> ipv4As2 = routers.Get (1)->GetObject<Ipv4> ();

          Ptr<Ipv4StaticRouting> staticRoutingAs2 = staticRoutingHelper.GetStaticRouting (ipv4As2);

          staticRoutingAs2->AddHostRouteTo (Ipv4Address ("192.168.3.2"), Ipv4Address ("192.168.2.2"), 2);
  }

  uint16_t port = 8080;
  UdpEchoServerHelper server (port);
  ApplicationContainer apps = server.Install (routers.Get (3));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (13.0));

  UdpEchoClientHelper client (i34.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (8));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.75)));
  client.SetAttribute ("PacketSize", UintegerValue (512));

  apps = client.Install (routers.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (13.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Optional: if you add ns-3's NetAnim module (AnimationInterface) here to
  // visualize packet flow across the topology, screenshots of it can
  // strengthen your report and are worth bringing to the viva -- see the
  // note in the assignment write-up. It's not required.

  Simulator::Stop (Seconds (14.0));
  Simulator::Run ();
  std::string xml;
  if (mode == "static"){
	  xml = "interas-partC-static-results.xml";
  }
  else {
	  xml = "interas-partC-metric-results.xml";
  }

  monitor->SerializeToXmlFile (xml, true, true);

  Simulator::Destroy ();
  return 0;
}
