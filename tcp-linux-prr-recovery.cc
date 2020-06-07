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
 * Author: Apoorva Bhargava <apoorvabhargava13@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 *
 */

#include "tcp-linux-prr-recovery.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpLinuxPrrRecovery");
NS_OBJECT_ENSURE_REGISTERED (TcpLinuxPrrRecovery);

TypeId
TcpLinuxPrrRecovery::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpLinuxPrrRecovery")
    .SetParent<TcpClassicRecovery> ()
    .AddConstructor<TcpLinuxPrrRecovery> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpLinuxPrrRecovery::TcpLinuxPrrRecovery (void)
  : TcpClassicRecovery ()
{
  NS_LOG_FUNCTION (this);
}

TcpLinuxPrrRecovery::TcpLinuxPrrRecovery (const TcpLinuxPrrRecovery& recovery)
  : TcpClassicRecovery (recovery),
    m_prrDelivered (recovery.m_prrDelivered),
    m_prrOut (recovery.m_prrOut),
    m_cWndPrior (recovery.m_cWndPrior)
{
  NS_LOG_FUNCTION (this);
}

TcpLinuxPrrRecovery::~TcpLinuxPrrRecovery (void)
{
  NS_LOG_FUNCTION (this);
}

void
TcpLinuxPrrRecovery::EnterRecovery (Ptr<TcpSocketState> tcb, uint32_t dupAckCount,
                               uint32_t unAckDataCount, uint32_t deliveredBytes)
{
  NS_LOG_FUNCTION (this << tcb << dupAckCount << unAckDataCount);
  NS_UNUSED (dupAckCount);
  NS_UNUSED (unAckDataCount);

  m_prrOut = 0;
  m_prrDelivered = 0;
  m_cWndPrior = tcb->m_cWnd;

  DoRecovery (tcb, deliveredBytes);
}

void
TcpLinuxPrrRecovery::DoRecovery (Ptr<TcpSocketState> tcb, uint32_t deliveredBytes)
{
  NS_LOG_FUNCTION (this << tcb << deliveredBytes);
  m_prrDelivered += deliveredBytes;

  int sendCount;
  if (tcb->m_bytesInFlight > tcb->m_ssThresh)
    {
      uint64_t dividend = ((uint64_t)tcb->m_ssThresh / tcb->m_segmentSize) * (m_prrDelivered / tcb->m_segmentSize) + (m_cWndPrior / tcb->m_segmentSize) - 1;
      sendCount = (dividend / (m_cWndPrior / tcb->m_segmentSize)) - (m_prrOut / tcb->m_segmentSize);
    }
  else if (tcb->m_isRetransDataAcked)
    {
      sendCount = std::min (static_cast<int> (tcb->m_ssThresh - tcb->m_bytesInFlight) / tcb->m_segmentSize,
                            std::max ((m_prrDelivered / tcb->m_segmentSize) - (m_prrOut / tcb->m_segmentSize),
                                      static_cast<int> (deliveredBytes) / tcb->m_segmentSize) + 1);
    }
  else
    {
      sendCount = std::min (static_cast<int> (tcb->m_ssThresh - tcb->m_bytesInFlight) / tcb->m_segmentSize, static_cast<int> (deliveredBytes) / tcb->m_segmentSize);
    }


  /* Force a fast retransmit upon entering fast recovery */
  sendCount = std::max (sendCount * ((int)tcb->m_segmentSize), static_cast<int> (m_prrOut > 0 ? 0 : tcb->m_segmentSize));
  tcb->m_cWnd = tcb->m_bytesInFlight + sendCount;
  tcb->m_cWndInfl = tcb->m_cWnd;
}

void
TcpLinuxPrrRecovery::ExitRecovery (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);


//  tcb->m_cWnd = tcb->m_ssThresh.Get ();
//  tcb->m_cWndInfl = tcb->m_cWnd;
}

void
TcpLinuxPrrRecovery::UpdateBytesSent (uint32_t bytesSent)
{
  NS_LOG_FUNCTION (this << bytesSent);
  m_prrOut += bytesSent;
}

Ptr<TcpRecoveryOps>
TcpLinuxPrrRecovery::Fork (void)
{
  return CopyObject<TcpLinuxPrrRecovery> (this);
}

std::string
TcpLinuxPrrRecovery::GetName () const
{
  return "LinuxPrrRecovery";
}

} // namespace ns3
