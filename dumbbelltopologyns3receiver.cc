/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Apoorva Bhargava <apoorvabhargava13@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include <iostream>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dce-module.h"

using namespace ns3;
Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
std::string dir = "results/dumbbell-topology/";
double stopTime = 20;

// Functions to check queue length of Router 1 for Linux and ns-3 stack
void
LinuxCheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();

  // Check queue size in Linux stack every 1/100 of a second
  Simulator::Schedule (Seconds (0.001), &LinuxCheckQueueSize, queue);
  std::ofstream fPlotQueue (std::stringstream (dir + "linux-queue-size.plotme").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();
}

void
ns3CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();

  // Check queue size in ns-3 stack every 1/100 of a second
  Simulator::Schedule (Seconds (0.001), &ns3CheckQueueSize, queue);
  std::ofstream fPlotQueue (std::stringstream (dir + "ns3-queue-size.plotme").str ().c_str (), std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();
}

// Functions to trace change in cwnd for all the senders
static void
CwndChangeA (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::ofstream fPlotQueue (dir + "cwndTraces/A-ns3.plotme", std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << newCwnd / 524.0 << std::endl;
  fPlotQueue.close ();
}

// Function to calculate drops in a particular Queue
static void
DropAtQueue (Ptr<OutputStreamWrapper> stream, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " 1" << std::endl;
}

// Trace Function for cwnd
void
TraceCwnd (uint32_t node, uint32_t cwndWindow,
           Callback <void, uint32_t, uint32_t> CwndTrace)
{
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string (node) + "/$ns3::TcpL4Protocol/SocketList/" + std::to_string (cwndWindow) + "/CongestionWindow", CwndTrace);
}

// Function to install BulkSend application
void InstallBulkSend (Ptr<Node> node, Ipv4Address address, uint16_t port, std::string sock_factory,
                      uint32_t nodeId, uint32_t cwndWindow,
                      Callback <void, uint32_t, uint32_t> CwndTrace)
{
  BulkSendHelper source (sock_factory, InetSocketAddress (address, port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (node);
  sourceApps.Start (Seconds (10.0));
  Simulator::Schedule (Seconds (10.0) + Seconds (0.001), &TraceCwnd, nodeId, cwndWindow, CwndTrace);
  sourceApps.Stop (Seconds (stopTime));
}

void InstallBulkSend (Ptr<Node> node, Ipv4Address address, uint16_t port, std::string sock_factory)
{
  BulkSendHelper source (sock_factory, InetSocketAddress (address, port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (node);
  sourceApps.Start (Seconds (10.0));
  sourceApps.Stop (Seconds (stopTime));
}

// Function to install sink application
void InstallPacketSink (Ptr<Node> node, uint16_t port, std::string sock_factory)
{
  PacketSinkHelper sink (sock_factory, InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (node);
  sinkApps.Start (Seconds (10.0));
  sinkApps.Stop (Seconds (stopTime));
}

// Function to run "ss -a -e -i" command on a particular node having Linux stack
static void GetSSStats (Ptr<Node> node, Time at, std::string stack)
{
  if (stack == "linux")
    {
      DceApplicationHelper process;
      ApplicationContainer apps;
      process.SetBinary ("ss");
      process.SetStackSize (1 << 20);
      process.AddArgument ("-a");
      process.AddArgument ("-e");
      process.AddArgument ("-i");
      apps.Add (process.Install (node));
      apps.Start (at);
    }
}

int main (int argc, char *argv[])
{
  uint32_t stream = 1;
  std::string stack = "linux";
  std::string sock_factory = "ns3::TcpSocketFactory";
  std::string transport_prot = "TcpNewReno";
  std::string linux_prot = "reno";
  std::string queue_disc_type = "FifoQueueDisc";
  bool isSack = false;
  bool isWindowScale = false;
  bool isBql = false;
  uint32_t dataSize = 524;
  uint32_t delAckCount = 1;
  std::string recovery = "TcpClassicRecovery";

  // Enable checksum if Linux and ns-3 node communicate
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  time_t rawtime;
  struct tm * timeinfo;
  char buffer[80];

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (buffer,sizeof(buffer),"%d-%m-%Y-%I-%M-%S",timeinfo);
  std::string currentTime (buffer);

  CommandLine cmd;
  cmd.AddValue ("stream", "Seed value for random variable", stream);
  cmd.AddValue ("stack", "Set TCP/IP stack as ns3 or linux", stack);
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
                "TcpLp", transport_prot);
  cmd.AddValue ("linux_prot", "Linux network protocol to use: reno, "
                "hybla, highspeed, htcp, vegas, scalable, veno, "
                "bic, yeah, illinois, westwood, lp", linux_prot);
  cmd.AddValue ("queue_disc_type", "Queue disc type for gateway (e.g. ns3::CoDelQueueDisc)", queue_disc_type);
  cmd.AddValue ("dataSize", "Data packet size", dataSize);
  cmd.AddValue ("delAckCount", "Delayed ack count", delAckCount);
  cmd.AddValue ("Sack", "Flag to enable/disable sack in TCP", isSack);
  cmd.AddValue ("WindowScaling", "Flag to enable/disable window scaling in TCP", isWindowScale);
  cmd.AddValue ("stopTime", "Stop time for applications / simulation time will be stopTime", stopTime);
  cmd.AddValue ("recovery", "Recovery algorithm type to use (e.g., ns3::TcpPrrRecovery", recovery);
  cmd.AddValue ("BQL", "Flag to enable/disable BQL for ns-3 stack", isBql);
  cmd.Parse (argc,argv);

  uv->SetStream (stream);
  queue_disc_type = std::string ("ns3::") + queue_disc_type;

  transport_prot = std::string ("ns3::") + transport_prot;

  recovery = std::string ("ns3::") + recovery;

  TypeId qdTid;
  NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (queue_disc_type, &qdTid), "TypeId " << queue_disc_type << " not found");

  // Sets recovery algorithm and TCP variant in ns-3
  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType", TypeIdValue (TypeId::LookupByName (recovery)));
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transport_prot)));
    }

  // Create nodes
  NodeContainer leftNodes, rightNodes, routers;
  routers.Create (2);
  leftNodes.Create (1);
  rightNodes.Create (1);

  std::vector <NetDeviceContainer> leftToRouter;
  std::vector <NetDeviceContainer> routerToRight;

  // Create the point-to-point link helpers and connect two router nodes
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("1Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer r1r2ND = pointToPointRouter.Install (routers.Get (0), routers.Get (1));

  // Create the point-to-point link helpers and connect leaf nodes to router
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("1ms"));
  leftToRouter.push_back (pointToPointLeaf.Install (leftNodes.Get (0), routers.Get (0)));
  routerToRight.push_back (pointToPointLeaf.Install (routers.Get (1), rightNodes.Get (0)));

  DceManagerHelper dceManager;
  LinuxStackHelper linuxStack;
  InternetStackHelper internetStack;

  if (stack == "linux")
    {
      sock_factory = "ns3::LinuxTcpSocketFactory";
      dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));
      dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
      linuxStack.Install (leftNodes);
    }
  else
    {
      internetStack.Install (leftNodes);
    }
  internetStack.Install (rightNodes);
  internetStack.Install (routers);

  // Assign IP addresses to all the network devices
  Ipv4AddressHelper ipAddresses ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer r1r2IPAddress = ipAddresses.Assign (r1r2ND);
  ipAddresses.NewNetwork ();

  std::vector <Ipv4InterfaceContainer> leftToRouterIPAddress;
  leftToRouterIPAddress.push_back (ipAddresses.Assign (leftToRouter [0]));
  ipAddresses.NewNetwork ();

  std::vector <Ipv4InterfaceContainer> routerToRightIPAddress;
  routerToRightIPAddress.push_back (ipAddresses.Assign (routerToRight [0]));

  dceManager.Install (leftNodes);
  dceManager.Install (rightNodes);
  dceManager.Install (routers);

  // Set configuration for Linux stack and create routing table for each node
  if (stack == "linux")
    {
      // Enable IP forwarding in Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.conf.default.forwarding", "1");
      // Sets TCP Congestion Control algorithm in Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.tcp_congestion_control", linux_prot);
      // Enable/Disable Window Scaling in TCP for Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.tcp_window_scaling", ((isWindowScale)) ? "1" : "0");
      // Enable/Disable SACK in TCP for Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.tcp_sack", ((isSack) ? "1" : "0"));
      // Disable FACK in TCP for Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.tcp_fack", "0");
      // Disable DSACK in TCP for Linux stack
      linuxStack.SysctlSet (leftNodes, ".net.ipv4.tcp_dsack", "0");

      // Static Routing
      Ptr<Ipv4> ipv4Router1 = routers.Get (0)->GetObject<Ipv4> ();
      Ptr<Ipv4> ipv4Router2 = routers.Get (1)->GetObject<Ipv4> ();
      Ptr<Ipv4> ipv4Receiver = rightNodes.Get (0)->GetObject<Ipv4> ();

      Ipv4StaticRoutingHelper routingHelper;

      Ptr<Ipv4StaticRouting> staticRoutingRouter1 = routingHelper.GetStaticRouting (ipv4Router1);
      Ptr<Ipv4StaticRouting> staticRoutingRouter2 = routingHelper.GetStaticRouting (ipv4Router2);
      Ptr<Ipv4StaticRouting> staticRoutingReceiver = routingHelper.GetStaticRouting (ipv4Receiver);

      // Routing for Router 1
      staticRoutingRouter1->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.0.0.2"), 1);

      // Routing for Router 2
      staticRoutingRouter2->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.0.0.1"), 1);

      // Routing for Receiver
      staticRoutingReceiver->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.0.2.1"), 1);

      std::ostringstream cmd_oss;

      // Default route for sender
      cmd_oss.str ("");
      cmd_oss << "route add default via " << (routers.Get (0)->GetObject<Ipv4> ())->GetAddress (2, 0).GetLocal () << " dev sim0";
      LinuxStackHelper::RunIp (leftNodes.Get (0), Seconds (0.00001), cmd_oss.str ());
      LinuxStackHelper::RunIp (leftNodes.Get (0), Seconds (0.00001), "link set sim0 up");
    }
  else if (stack == "ns3")
    {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  // Sets default sender and receiver buffer size as 1MB
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 20));
  // Sets default initial congestion window as 10 segments
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  // Sets default delayed ack count to a specified value
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (delAckCount));
  // Sets default segment size of TCP packet to a specified value
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (dataSize));
  // Enable/Disable SACK in TCP
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (isSack));
  // Enable/Disable Window Scaling in TCP
  Config::SetDefault ("ns3::TcpSocketBase::WindowScaling", BooleanValue (isWindowScale));

  // Creates directories to store plotme files
  dir += (currentTime + "/");
  std::string dirToSave = "mkdir -p " + dir;
  system (dirToSave.c_str ());
  system ((dirToSave + "/pcap/").c_str ());
  system ((dirToSave + "/queueTraces/").c_str ());
  if (stack == "ns3")
    {
      system ((dirToSave + "/cwndTraces/").c_str ());
    }

  // Set default parameters for queue discipline
  Config::SetDefault (queue_disc_type + "::MaxSize", QueueSizeValue (QueueSize ("100p")));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> streamWrapper;

  // Install queue discipline on router
  TrafficControlHelper tch;
  tch.SetRootQueueDisc (queue_disc_type);
  QueueDiscContainer qd;
  tch.Uninstall (routers.Get (0)->GetDevice (0));
  qd.Add (tch.Install (routers.Get (0)->GetDevice (0)).Get (0));

  // Enables BQL
  if (isBql)
    {
      tch.SetQueueLimits ("ns3::DynamicQueueLimits");
    }

  // Calls function to check queue size
  if (stack == "linux")
    {
      Simulator::ScheduleNow (&LinuxCheckQueueSize, qd.Get (0));
    }
  else if (stack == "ns3")
    {
      Simulator::ScheduleNow (&ns3CheckQueueSize, qd.Get (0));
    }

  // Create plotme to store packets dropped and marked at the router
  streamWrapper = asciiTraceHelper.CreateFileStream (dir + "/queueTraces/drop-0.plotme");
  qd.Get (0)->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropAtQueue, streamWrapper));

  // Install packet sink at receiver side
  uint16_t port = 50000;
  InstallPacketSink (rightNodes.Get (0), port, "ns3::TcpSocketFactory");

  // Install BulkSend application
  if (stack == "linux")
    {
      InstallBulkSend (leftNodes.Get (0), routerToRightIPAddress [0].GetAddress (1), port, sock_factory);
    }
  else if (stack == "ns3")
    {
      InstallBulkSend (leftNodes.Get (0), routerToRightIPAddress [0].GetAddress (1), port, sock_factory, 2, 0, MakeCallback (&CwndChangeA));
    }

  // Calls function to run ss command on Linux stack after every 0.05 seconds
  if (stack == "linux")
    {
      for (int j = 0; j < leftNodes.GetN (); j++)
        {
          for (float i = 10.0; i <= stopTime; i = i + 0.05)
            {
              GetSSStats (leftNodes.Get (j), Seconds (i), stack);
            }
        }
    }

  // Enables PCAP on all the point to point interfaces
  if (stack == "linux")
    {
      pointToPointLeaf.EnablePcapAll (dir + "pcap/Linux", true);
    }
  else
    {
      pointToPointLeaf.EnablePcapAll (dir + "pcap/ns-3", true);
    }

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();

  // Stores queue stats in a file
  std::ofstream myfile;
  myfile.open (dir + "queueStats.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << std::endl;
  myfile << "Stat for Queue 1";
  myfile << qd.Get (0)->GetStats ();
  myfile.close ();

  // Stores configuration of the simulation in a file
  myfile.open (dir + "config.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  myfile << "queue_disc_type " << queue_disc_type << "\n";
  myfile << "stream  " << stream << "\n";
  myfile << "stack  " << stack << "\n";
  (stack == "ns3") ? myfile << "transport_prot " << transport_prot << "\n" : myfile << "linux_prot " << linux_prot << "\n";
  myfile << "dataSize " << dataSize << "\n";
  myfile << "delAckCount " << delAckCount << "\n";
  myfile << "stopTime " << stopTime << "\n";
  myfile.close ();

  Simulator::Destroy ();

  return 0;
}
