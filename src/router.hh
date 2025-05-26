#pragma once

#include "address.hh"
#include "debug.hh"
#include "exception.hh"
#include "network_interface.hh"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
    struct Item{
        std::optional<Address> next_hop;
        size_t interface_num;
    };
    struct bitTrie{
        struct bitNode{
            std::optional<Item> table;
            std::shared_ptr<bitNode> next[2]; 
        };
        std::shared_ptr<bitNode> head=std::make_shared<bitNode>();
        std::shared_ptr<bitNode> insert(uint32_t route_prefix,uint8_t prefix_length){
            if(prefix_length==0){
                return head;
            }
            std::shared_ptr<bitNode> node=head;
            for(int i=31;i>=32-prefix_length;i--){
                bool bit=(route_prefix>>i)&1UL;
                if(!node->next[bit]){
                    node->next[bit]=std::make_shared<bitNode>();
                }
                debug("route_prefix = {},prefix= {}",Address::from_ipv4_numeric((route_prefix>>(32-prefix_length))<<(32-prefix_length)).ip(),prefix_length);
                node=node->next[bit];
            }
            return node;
        }
        const std::optional<Item> search(const uint32_t ipv4) const{//最长匹配
            std::shared_ptr<bitNode> node=head;
            for(int i=31;i>=0;i--){
                bool bit=(ipv4>>i)&1UL;
                if(!node->next[bit]){
                    if(!node->table){
                        return head->table;
                    }
                    break;
                }
                debug("i = {} ,route_prefix = {},prefix = {}",i,Address::from_ipv4_numeric((ipv4>>i)<<i).ip(),(32-i));
                node=node->next[bit];
            }
            return node->table;
        }
    };
    bitTrie BTrie{};
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
    // std::unordered_map<addressPair, Item> RoutingTable {};
};
