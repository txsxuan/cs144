#include "router.hh"
#include "debug.hh"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

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
    if(interface_num>=interfaces_.size()||interface_num<0){
        throw std::runtime_error(std::string("interface_num out of range !"));
    }
    else if(prefix_length>32){
        throw std::runtime_error(std::string("prefix_length out of range !"));
    }
    std::shared_ptr<bitTrie::bitNode> node=BTrie.insert(route_prefix,prefix_length);
    node->table={next_hop,interface_num};
    debug("route_ {}",node->table.has_value()?'+':'-');
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto interface_:interfaces_){
    std::queue<InternetDatagram>& dgrams=interface_->datagrams_received();
    while(!dgrams.empty()){
        auto dgram=std::move(dgrams.front());
        debug("dgram with head : {}",dgram.header.to_string());
        if(dgram.header.ttl<=1){
            dgrams.pop();
            continue;
        }
        dgram.header.ttl--;
        const std::optional<Item> route_=BTrie.search(dgram.header.dst);
        if(route_){
            debug("find route for {}: interface {} , next_hop {}",Address::from_ipv4_numeric(dgram.header.dst).ip(),route_->interface_num,( route_->next_hop.has_value() ? route_->next_hop->ip() : "(direct)" ));
            interface(route_->interface_num)->send_datagram(dgram, (route_->next_hop)?route_->next_hop.value():Address::from_ipv4_numeric(dgram.header.dst));
        }
        else{
            debug("no route with {}",Address::from_ipv4_numeric(dgram.header.dst).ip());
        }
        dgrams.pop();
    }
  }
}


