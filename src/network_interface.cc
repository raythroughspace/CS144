#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"
#include <assert.h>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if (mappings_.count(next_hop)){
    EthernetFrame frame = {.header = {.dst = mappings_[next_hop], .src = ethernet_address_, .type = EthernetHeader::TYPE_IPv4}, 
                            .payload = serialize(dgram)};
    transmit(frame);
  }
  else{
    ARPMessage arpmsg = {.opcode = arpmsg.OPCODE_REQUEST, 
                        .sender_ethernet_address = ethernet_address_, .sender_ip_address = ip_address_.ipv4_numeric(),
                         .target_ip_address = next_hop.ipv4_numeric()};
    EthernetFrame arpframe = {.header = {.dst = ETHERNET_BROADCAST, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP},
                              .payload = serialize(arpmsg)};
    if (!sent_arp_msgs_.count(next_hop)){
      sent_arp_msgs_[next_hop] = 5000;
      transmit(arpframe);
    }
    if (!dgram_queue_.count(next_hop))
      dgram_queue_[next_hop] = {};
    dgram_queue_[next_hop].push_back(dgram);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  auto& [header, payload] = frame;
  if (header.type == header.TYPE_IPv4 && header.dst == ethernet_address_){
    InternetDatagram dgram;
    if (parse(dgram, payload)){
      datagrams_received_.push(dgram);
    }
  }
  else if (header.type == header.TYPE_ARP){
    ARPMessage arp;
    if (parse(arp, payload)){
      Address new_addr = Address::from_ipv4_numeric(arp.sender_ip_address);
      mappings_[new_addr] = arp.sender_ethernet_address;
      mappings_TMO_[new_addr] = 30000;
      if (arp.target_ip_address == ip_address_.ipv4_numeric()){
        if (arp.opcode == arp.OPCODE_REQUEST){
          ARPMessage reply = {.opcode = reply.OPCODE_REPLY, 
                              .sender_ethernet_address = ethernet_address_, .sender_ip_address = ip_address_.ipv4_numeric(),
                              .target_ethernet_address = arp.sender_ethernet_address, .target_ip_address = arp.sender_ip_address};
          EthernetFrame replyframe = {.header = {.dst = arp.sender_ethernet_address, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP},
                                      .payload = serialize(reply)};
          transmit(replyframe);
        }
        else if (arp.opcode == arp.OPCODE_REPLY){
          if (sent_arp_msgs_.count(new_addr))
            sent_arp_msgs_.erase(new_addr);
        }
      }
      for (auto& dgram: dgram_queue_[new_addr]){
        assert(mappings_.count(new_addr));
        send_datagram(dgram, new_addr);
      }
      dgram_queue_.erase(new_addr);
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for (auto [addr, time]: sent_arp_msgs_){
    if (ms_since_last_tick >= time){
      sent_arp_msgs_.erase(addr);
      dgram_queue_.erase(addr);
    }
    else{
      sent_arp_msgs_[addr] -= ms_since_last_tick;
    }
  }

  for (auto [addr, time] : mappings_TMO_){
    if (ms_since_last_tick >= time){
      mappings_.erase(addr);
      mappings_TMO_.erase(addr);
    }
    else{
      mappings_TMO_[addr] -= ms_since_last_tick;
    }
  }

}
