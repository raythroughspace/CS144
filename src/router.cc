#include "router.hh"
#include "debug.hh"

#include <iostream>
#include <assert.h>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  routing_table_[{route_prefix, prefix_length}] = {next_hop, interface_num};
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for (size_t i=0; i<interfaces_.size(); ++i){
    queue<InternetDatagram> dgrams = interfaces_[i]->datagrams_received();
    while(dgrams.size() != 0){
      InternetDatagram dgram = dgrams.front();
      dgrams.pop();
      if (dgram.header.ttl == 0)
        continue;
      --dgram.header.ttl;
      optional<pair<optional<Address>, size_t>> match = find_route_(dgram.header.dst);
      if (match.has_value()){
        auto [next_hop, interface_num] = match.value();
        if (next_hop.has_value()){
          interfaces_[interface_num]->send_datagram(dgram, next_hop.value());
        }
        else{
          interfaces_[interface_num]->send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
        }
      }
    }
  }
}

optional<pair<optional<Address>, size_t>> Router::find_route_(uint32_t dst) {
  optional<pair<optional<Address>, size_t>> match;
  ssize_t longest_match = -1;
  for (auto [key, val]: routing_table_){
    auto [route_prefix, prefix_length] = key;
    if (prefix_length <= longest_match){
      continue;
    }
    // prefix_length > longest_match
    if (prefix_length == 0){
      longest_match = prefix_length;
      match = val;
      continue;
    }
    uint32_t mask = ((1<<prefix_length) - 1) << (32 - prefix_length);
    if ((dst & mask) == (route_prefix & mask)){
      longest_match = prefix_length;
      match = val;
    }
  }
  return match;
}
