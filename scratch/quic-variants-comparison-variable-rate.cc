/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 ResiliNets, ITTC, University of Kansas
 * Copyright (c) 2019 SIGNET Lab, Department of Information Engineering, University of Padova
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
 * Authors of the original TCP example: 
 * Justin P. Rohrer, Truc Anh N. Nguyen <annguyen@ittc.ku.edu>, Siddharth Gangadhar <siddharth@ittc.ku.edu>
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 *
 * “TCP Westwood(+) Protocol Implementation in ns-3”
 * Siddharth Gangadhar, Trúc Anh Ngọc Nguyễn , Greeshma Umapathi, and James P.G. Sterbenz,
 * ICST SIMUTools Workshop on ns-3 (WNS3), Cannes, France, March 2013
 *
 * Adapted to QUIC by:
 *          Alvise De Biasio <alvise.debiasio@gmail.com>
 *          Federico Chiariotti <chiariotti.federico@gmail.com>
 *          Michele Polese <michele.polese@gmail.com>
 *          Davide Marcato <davidemarcato@outlook.com>
 *
 */

#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/quic-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/quic-bbr.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("QuicVariantsComparisonBulkSend");

// connect to a number of traces
static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
RttChange (Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldRtt.GetSeconds () << "\t" << newRtt.GetSeconds () << std::endl;
}

static void
BbrStateChange (Ptr<OutputStreamWrapper> stream, QuicBbr::BbrMode_t oldState, QuicBbr::BbrMode_t newState)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldState
                        << "\t" << newState << std::endl;
}

static void
PacingRateChange (Ptr<OutputStreamWrapper> stream, DataRate oldRate, DataRate newRate)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldRate.GetBitRate ()
                        << "\t" << newRate.GetBitRate () << std::endl;
}

static void
Rx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p, const QuicHeader& q, Ptr<const QuicSocketBase> qsb)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << p->GetSize() << std::endl;
}

static void
AppRx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
  NS_UNUSED (address);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << packet->GetSize () << std::endl;
}

static void
Traces(uint32_t serverId, std::string pathVersion, std::string finalPart)
{
  AsciiTraceHelper asciiTraceHelper;

  std::ostringstream pathCW;
  pathCW << "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/CongestionWindow";
  NS_LOG_INFO("Matches cw " << Config::LookupMatches(pathCW.str().c_str()).GetN());

  std::ostringstream fileCW;
  fileCW << pathVersion << "QUIC-cwnd-change"  << serverId << "" << finalPart;

  std::ostringstream pathRTT;
  pathRTT << "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/RTT";

  std::ostringstream fileRTT;
  fileRTT << pathVersion << "QUIC-rtt"  << serverId << "" << finalPart;

  std::ostringstream pathRCWnd;
  pathRCWnd<< "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/RWND";

  std::ostringstream fileRCWnd;
  fileRCWnd<<pathVersion << "QUIC-rwnd-change"  << serverId << "" << finalPart;

  std::ostringstream pathPacing;
  pathPacing << "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/CurrentPacingRate";

  std::ostringstream filePacing;
  filePacing << pathVersion << "QUIC-pacing-rate"  << serverId << "" << finalPart;

  std::ostringstream pathBbrState;
  pathBbrState << "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/CongControl/BbrState";

  std::ostringstream fileBbrState;
  fileBbrState << pathVersion << "QUIC-BBR-state"  << serverId << "" << finalPart;

  std::ostringstream fileName;
  fileName << pathVersion << "QUIC-rx-data" << serverId << "" << finalPart;
  std::ostringstream pathRx;
  pathRx << "/NodeList/" << serverId << "/$ns3::QuicL4Protocol/SocketList/0/QuicSocketBase/Rx";
  NS_LOG_INFO("Matches rx " << Config::LookupMatches(pathRx.str().c_str()).GetN());

  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (fileName.str ().c_str ());
  Config::ConnectWithoutContext (pathRx.str ().c_str (), MakeBoundCallback (&Rx, stream));

  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (fileCW.str ().c_str ());
  Config::ConnectWithoutContext (pathCW.str ().c_str (), MakeBoundCallback(&CwndChange, stream1));

  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (fileRTT.str ().c_str ());
  Config::ConnectWithoutContext (pathRTT.str ().c_str (), MakeBoundCallback(&RttChange, stream2));

  Ptr<OutputStreamWrapper> stream4 = asciiTraceHelper.CreateFileStream (fileRCWnd.str ().c_str ());
  Config::ConnectWithoutContext (pathRCWnd.str ().c_str (), MakeBoundCallback(&CwndChange, stream4));

  Ptr<OutputStreamWrapper> streamPacing = asciiTraceHelper.CreateFileStream (filePacing.str ().c_str ());
  Config::ConnectWithoutContext (pathPacing.str ().c_str (), MakeBoundCallback(&PacingRateChange, streamPacing));

  Ptr<OutputStreamWrapper> streamBbrState = asciiTraceHelper.CreateFileStream (fileBbrState.str ().c_str ());
  Config::ConnectWithoutContext (pathBbrState.str ().c_str (), MakeBoundCallback(&BbrStateChange, streamBbrState));

}

static void
TraceAppRx (ApplicationContainer serverApps, std::string pathVersion, std::string finalPart)
{
  AsciiTraceHelper asciiTraceHelper;
  for (auto it = serverApps.Begin (); it != serverApps.End (); ++it)
    {
      Ptr<PacketSink> app = DynamicCast<PacketSink> (*it);
      std::ostringstream file;
      file << pathVersion << "-App-rx-data-" << app->GetNode ()->GetId () << finalPart;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (file.str ().c_str ());
      app->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&AppRx, stream));
    }
}

static void
BytesInQueueTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  NS_UNUSED(oldVal);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newVal << std::endl;
}

static void
TraceBottleneckQueue (NodeContainer gateways, std::string pathVersion, std::string finalPart)
{
  AsciiTraceHelper asciiTraceHelper;
  for (auto it = gateways.Begin (); it != gateways.End (); ++it)
    {
      Ptr<Node> node = DynamicCast<Node> (*it);
      std::ostringstream file;
      file << pathVersion << "-Queue-size-" << node->GetId () << finalPart;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (file.str ().c_str ());

      Ptr<Queue<Packet> > queue = StaticCast<PointToPointNetDevice> (node->GetDevice (2))->GetQueue ();

      queue->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&BytesInQueueTrace, stream));
    }
}

/**
 * Changes the rate of the bottleneck link
 * \param gateways the nodes at the end of the bottleneck link
 * \param newRate the new rate to set
 */
static void
ChangeRate (NodeContainer gateways, DataRate newRate)
{
  for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
      uint32_t id = (*it)->GetId ();
      std::ostringstream path;
      path << "/NodeList/" << id << "/DeviceList/2/$ns3::PointToPointNetDevice/DataRate";
      Config::Set (path.str ().c_str (), DataRateValue (newRate));
    }
  NS_LOG_INFO ("BtlBw changed to " << newRate);
}

int main (int argc, char *argv[])
{
  std::string transport_prot = "QuicBbr";
  bool pacing = true;
  double error_p = 0.0;
  std::string bandwidth1 = "2Mbps";
  std::string bandwidth2 = "4Mbps";
  std::string delay = "0.01ms";
  std::string access_bandwidth = "10Mbps";
  std::string access_delay = "45ms";
  bool tracing = false;
  std::string prefix_file_name = "QuicVariantsComparisonVarRate";
  double data_mbytes = 0;
  uint32_t mtu_bytes = 1500;
  uint16_t num_flows = 1;
  float duration = 60.0;
  uint32_t run = 0;
  bool flow_monitor = false;
  bool pcap = false;
  std::string queue_disc_type = "ns3::PfifoFastQueueDisc";

  // LogComponentEnable ("Config", LOG_LEVEL_ALL);
  CommandLine cmd;
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bandwidth1", "Bottleneck bandwidth 1", bandwidth1);
  cmd.AddValue ("bandwidth2", "Bottleneck bandwidth 2", bandwidth2);
  cmd.AddValue ("delay", "Bottleneck delay", delay);
  cmd.AddValue ("access_bandwidth", "Access link bandwidth", access_bandwidth);
  cmd.AddValue ("access_delay", "Access link delay", access_delay);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("num_flows", "Number of flows", num_flows);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
  cmd.AddValue ("RngRun", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("flow_monitor", "Enable flow monitor", flow_monitor);
  cmd.AddValue ("pcap_tracing", "Enable or disable PCAP tracing", pcap);
  cmd.AddValue ("queue_disc_type", "Queue disc type for gateway (e.g. ns3::CoDelQueueDisc)", queue_disc_type);
  cmd.Parse (argc, argv);

  transport_prot = std::string ("ns3::") + transport_prot;

  SeedManager::SetSeed (1);
  SeedManager::SetRun (run);

  // User may find it convenient to enable logging
  Time::SetResolution (Time::NS);
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnableAll (LOG_PREFIX_FUNC);
  LogComponentEnableAll (LOG_PREFIX_NODE);
  // LogComponentEnable ("QuicSocketTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable ("QuicSocketBase", LOG_LEVEL_ALL);
  // LogComponentEnable ("QuicBbr", LOG_LEVEL_ALL);
  // LogComponentEnable ("QuicCongestionControl", LOG_LEVEL_ALL);
  // LogComponentEnable ("QuicStreamBase", LOG_LEVEL_ALL);
  // LogComponentEnable ("QuicStreamTxBuffer", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicVariantsComparison", LOG_LEVEL_ALL);
  // LogComponentEnable("QuicL5Protocol", LOG_LEVEL_ALL);
  // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  // LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);
  // LogComponentEnable("TcpVegas", LOG_LEVEL_ALL);

  // Set the simulation start and stop time
  float start_time = 0.1;
  float stop_time = start_time + duration;

  // 4 MB of TCP buffer
  Config::SetDefault ("ns3::QuicSocketBase::SocketRcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::QuicSocketBase::SocketSndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::QuicStreamBase::StreamSndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::QuicStreamBase::StreamRcvBufSize", UintegerValue (1 << 21));

  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (pacing));

 
  // Select congestion control variant
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)
    { 
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::QuicL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
      Config::SetDefault ("ns3::QuicL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transport_prot)));
    }

  // Create gateways, sources, and sinks
  NodeContainer sources;
  sources.Create (num_flows);
  NodeContainer sinks;
  sinks.Create (num_flows);
  NodeContainer gateways;
  gateways.Create (2);

  // Configure the error model
  // Here we use RateErrorModel with packet error rate
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (error_p);

  PointToPointHelper BottleneckLink;
  BottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bandwidth1));
  BottleneckLink.SetChannelAttribute ("Delay", StringValue (delay));
  BottleneckLink.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (&error_model));
  
  PointToPointHelper AccessLink;
  AccessLink.SetDeviceAttribute ("DataRate", StringValue (access_bandwidth));
  AccessLink.SetChannelAttribute ("Delay", StringValue (access_delay));

  QuicHelper stack;
  stack.InstallQuic (sources);
  stack.InstallQuic (sinks);
  stack.InstallQuic (gateways);

  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");

  TrafficControlHelper tchCoDel;
  tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  // Configure the sources and sinks net devices
  // and the channels between the sources/sinks and the gateways
  PointToPointHelper LocalLink;
  LocalLink.SetDeviceAttribute ("DataRate", StringValue (access_bandwidth));
  LocalLink.SetChannelAttribute ("Delay", StringValue (access_delay));

  Ipv4InterfaceContainer sink_interfaces;

  DataRate access_b (access_bandwidth);
  DataRate bottle_b1 (bandwidth1);
  DataRate bottle_b2 (bandwidth2);
  Time access_d (access_delay);
  Time bottle_d (delay);

  uint32_t size = (std::min (access_b, 
    std::max (bottle_b1, bottle_b2)).GetBitRate () / 8) *
      ((access_d + bottle_d) * 2).GetSeconds ();

  Config::SetDefault ("ns3::PfifoFastQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, size / mtu_bytes)));
  Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, size)));

  Config::SetDefault ("ns3::QuicSocketBase::MaxPacketSize", UintegerValue (mtu_bytes - 60));
  // Config::SetDefault ("ns3::TcpSocketBase::MaxPacketSize", UintegerValue (mtu_bytes - 60));

  for (int i = 0; i < num_flows; i++)
    {
      NetDeviceContainer devices;
      devices = AccessLink.Install (sources.Get (i), gateways.Get (0));
      tchPfifo.Install (devices);
      address.NewNetwork ();
      Ipv4InterfaceContainer interfaces = address.Assign (devices);

      devices = LocalLink.Install (gateways.Get (1), sinks.Get (i));
      if (queue_disc_type.compare ("ns3::PfifoFastQueueDisc") == 0)
        {
          tchPfifo.Install (devices);
        }
      else if (queue_disc_type.compare ("ns3::CoDelQueueDisc") == 0)
        {
          tchCoDel.Install (devices);
        }
      else
        {
          NS_FATAL_ERROR ("Queue not recognized. Allowed values are ns3::CoDelQueueDisc or ns3::PfifoFastQueueDisc");
        }
      address.NewNetwork ();
      interfaces = address.Assign (devices);
      sink_interfaces.Add (interfaces.Get (1));
      
      devices = BottleneckLink.Install (gateways.Get (0), gateways.Get (1));
      if (queue_disc_type.compare ("ns3::PfifoFastQueueDisc") == 0)
        {
          tchPfifo.Install (devices);
        }
      else if (queue_disc_type.compare ("ns3::CoDelQueueDisc") == 0)
        {
          tchCoDel.Install (devices);
        }
      else
        {
          NS_FATAL_ERROR ("Queue not recognized. Allowed values are ns3::CoDelQueueDisc or ns3::PfifoFastQueueDisc");
        }
      address.NewNetwork ();
      interfaces = address.Assign (devices);
    }

  NS_LOG_INFO ("Initialize Global Routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  // applications client and server
  for (uint16_t i = 0; i < sources.GetN (); i++)
    {
      AddressValue remoteAddress (InetSocketAddress (sink_interfaces.GetAddress (i, 0), port));
      BulkSendHelper ftp ("ns3::QuicSocketFactory", Address ());
      ftp.SetAttribute ("Remote", remoteAddress);
      ftp.SetAttribute ("SendSize", UintegerValue (2*mtu_bytes));
      clientApps.Add(ftp.Install (sources.Get (i)));
      PacketSinkHelper sinkHelper ("ns3::QuicSocketFactory", sinkLocalAddress);
      sinkHelper.SetAttribute ("Protocol", TypeIdValue (QuicSocketFactory::GetTypeId ()));
      serverApps.Add(sinkHelper.Install (sinks.Get (i)));
    }

  serverApps.Start (Seconds (start_time));
  clientApps.Stop (Seconds (stop_time));
  clientApps.Start (Seconds (2));

  Simulator::Schedule (Seconds (start_time + 0.00001), &TraceAppRx, serverApps, "./server", ".data");
  Simulator::Schedule (Seconds (start_time + 0.00001), &TraceBottleneckQueue, gateways, "./queue", ".data");

  for (uint16_t i = 0; i < num_flows; i++)
    {
      auto n2 = sinks.Get (i);
      auto n1 = sources.Get (i);
      Time t = Seconds(2.100001);
      std::ostringstream serverFilesPfx;
      serverFilesPfx << "./" /* << prefix_file_name */ << "server";
      std::ostringstream clientFilesPfx;
      clientFilesPfx << "./" /* << prefix_file_name */ << "client";

      Simulator::Schedule (t, &Traces, n2->GetId(), 
            serverFilesPfx.str ().c_str (), ".data");
      Simulator::Schedule (t, &Traces, n1->GetId(), 
            clientFilesPfx.str ().c_str(), ".data");
    }

  Simulator::Schedule (Seconds(20), &ChangeRate, gateways, bottle_b2);
  Simulator::Schedule (Seconds(35), &ChangeRate, gateways, bottle_b1);

  if (pcap)
    {
      BottleneckLink.EnablePcapAll (prefix_file_name, true);
      LocalLink.EnablePcapAll (prefix_file_name, true);
      AccessLink.EnablePcapAll (prefix_file_name, true);
    }

  // Flow monitor
  FlowMonitorHelper flowHelper;
  if (flow_monitor)
    {
      flowHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (stop_time));
  Simulator::Run ();

  if (flow_monitor)
    {
      flowHelper.SerializeToXmlFile (prefix_file_name + ".flowmonitor", true, true);
    }

  Simulator::Destroy ();

  for (uint16_t i = 0; i < num_flows; i++)
    {
      Ptr<PacketSink> s = DynamicCast<PacketSink> (serverApps.Get (i));
      std::cout << i << "\tRX_Bytes\t" << s->GetTotalRx () << std::endl;
    }

  return 0;
}