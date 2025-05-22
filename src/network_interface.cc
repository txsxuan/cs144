#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <arpa/inet.h>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

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
  debug( "unimplemented send_datagram called" );
  auto it=arpTabel.find(next_hop.ipv4_numeric());
  if(it!=arpTabel.end()){
    EthernetFrame frame{{it->second.MAC,ethernet_address_,EthernetHeader::TYPE_IPv4},{}};
    frame.payload=serialize(dgram);
    transmit(frame);
    debug("send datagram with head {}",frame.header.to_string());
  }
  else{
    // {
    //     Datagram_in_stand[next_hop.ipv4_numeric()].queue.emplace(dgram);
    //     if(){

    //     }
    // }
    auto & Item=dgram_in_stand[next_hop.ipv4_numeric()];
    // EthernetFrame frame{{ETHERNET_BROADCAST,ethernet_address_,EthernetHeader::TYPE_IPv4},{}};
    // frame.payload=serialize(dgram);
    // Item.queue.emplace(frame);
    // auto item=Item.queue.begin();
    // for(;item!=Item.queue.end();item++){
    //     if(*item==dgram){
    //         break;
    //     }
    // }
    // if(item==Item.queue.end()){
    //     Item.queue.emplace_back(dgram);
    // }
    if(!Item.queue.empty()){
        Item.queue.emplace_back(dgram);
        return;
    }
    Item.queue.emplace_back(dgram);
    // if(Item.TTL>ms_since_construct){//保证同一个IP不会发太多次，避免泛洪
    //     return;
    // }
    Item.TTL=ms_since_construct+5000;
    ARPMessage arpmsg{};
    arpmsg.opcode=ARPMessage::OPCODE_REQUEST;
    arpmsg.sender_ethernet_address=ethernet_address_;
    arpmsg.sender_ip_address=ip_address_.ipv4_numeric();
    // arpmsg.target_ethernet_address=ETHERNET_BROADCAST;
    arpmsg.target_ip_address=next_hop.ipv4_numeric();
    debug("send arp request : {}",arpmsg.to_string());
    EthernetFrame ARPframe{{ETHERNET_BROADCAST,ethernet_address_,EthernetHeader::TYPE_ARP},serialize(arpmsg)};
    transmit(ARPframe);
    
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
    if(frame.header.dst==ETHERNET_BROADCAST||frame.header.dst==ethernet_address_){    
        switch (frame.header.type) {
            case EthernetHeader::TYPE_IPv4:{
                InternetDatagram ipv4frame{};
                if(parse(ipv4frame,frame.payload)){
                    datagrams_received_.emplace(ipv4frame);
                }
                
                break;
            }
            case EthernetHeader::TYPE_ARP:{
                ARPMessage arpmsg{};
                if(parse(arpmsg,frame.payload)){
                    debug("recv arp : {}",arpmsg.to_string());
                    if(arpmsg.opcode==ARPMessage::OPCODE_REQUEST||arpmsg.opcode==ARPMessage::OPCODE_REPLY){                    
                        arpTabel[arpmsg.sender_ip_address]={arpmsg.sender_ethernet_address,ms_since_construct+30*1000};
                        auto it=dgram_in_stand.find(arpmsg.sender_ip_address);
                        if(it!=dgram_in_stand.end()){
                            EthernetFrame sendframe{{arpmsg.sender_ethernet_address,ethernet_address_,EthernetHeader::TYPE_IPv4},{}};
                            for(auto &dgram:it->second.queue){
                                sendframe.payload=serialize(dgram);
                                transmit(sendframe);
                            }
                            // while(!it->second.set.empty()){

                            //     it->second.queue.front().header.dst=arpmsg.sender_ethernet_address;
                            //     transmit(it->second.queue.front());
                            //     debug("send stand datagram with head {},dst ip = {}",it->second.queue.front().header.to_string(),inet_ntoa( { htobe32( it->first ) } ));
                            //     it->second.queue.pop();
                            // }
                            // frame_in_stand.erase(it);
                            dgram_in_stand.erase(it);
                        }
                        if(arpmsg.opcode==ARPMessage::OPCODE_REQUEST
                            &&arpmsg.target_ip_address==ip_address_.ipv4_numeric()){//只有当arp request的目的报文是该交换机（路由器）的ip时，才会reply
                            ARPMessage arpreply{};
                            arpreply.opcode=ARPMessage::OPCODE_REPLY;
                            arpreply.target_ip_address=arpmsg.sender_ip_address;
                            arpreply.target_ethernet_address=arpmsg.sender_ethernet_address;
                            arpreply.sender_ip_address=ip_address_.ipv4_numeric();
                            arpreply.sender_ethernet_address=ethernet_address_;
                            EthernetFrame replyframe{};
                            replyframe.payload=serialize(arpreply);
                            replyframe.header.type=EthernetHeader::TYPE_ARP;
                            replyframe.header.src=ethernet_address_;
                            replyframe.header.dst=arpmsg.sender_ethernet_address;
                            transmit(replyframe);
                            debug("send arp reply : {}",arpreply.to_string());
                        }
                    }
                }
                break;
            }
            default: break;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  ms_since_construct+=ms_since_last_tick;
  if(!arpTabel.empty()){
    for(auto it=arpTabel.begin();it!=arpTabel.end();){
        if(it->second.TTL<=ms_since_construct){
            it=arpTabel.erase(it);
        }
        else{
            it++;
        }
    }
  }
  if(!dgram_in_stand.empty()){
    for(auto it=dgram_in_stand.begin();it!=dgram_in_stand.end();){
        if(it->second.TTL<=ms_since_construct){
            it=dgram_in_stand.erase(it);
        }
        else{
            it++;
        }
    }
  }
}
