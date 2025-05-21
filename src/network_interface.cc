#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>

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
    EthernetFrame frame{{move(it->second.MAC),ethernet_address_,EthernetHeader::TYPE_IPv4},serialize(dgram)};
    transmit(std::move(frame));
  }
  else{
    EthernetFrame frame{{ETHERNET_BROADCAST,ethernet_address_,EthernetHeader::TYPE_IPv4},serialize(dgram)};
    frame_in_stand[next_hop.ipv4_numeric()].emplace(move(frame));
    ARPMessage arpmsg{};
    arpmsg.opcode=ARPMessage::OPCODE_REQUEST;
    arpmsg.sender_ethernet_address=ethernet_address_;
    arpmsg.sender_ip_address=ip_address_.ipv4_numeric();
    arpmsg.target_ethernet_address=ETHERNET_BROADCAST;
    arpmsg.target_ip_address=next_hop.ipv4_numeric();
    EthernetFrame ARPframe{{ETHERNET_BROADCAST,ethernet_address_,EthernetHeader::TYPE_ARP},serialize(arpmsg)};
    transmit(std::move(ARPframe));
    
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
                    datagrams_received_.emplace(move(ipv4frame));
                }
                
                break;
            }
            case EthernetHeader::TYPE_ARP:{
                ARPMessage arpmsg{};
                if(parse(arpmsg,frame.payload)){
                    if(arpmsg.opcode==ARPMessage::OPCODE_REQUEST
                        &&arpmsg.target_ip_address==ip_address_.ipv4_numeric()){
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
                        replyframe.header.dst=ETHERNET_BROADCAST;
                        transmit(std::move(replyframe));
                    }
                    else if(arpmsg.opcode==ARPMessage::OPCODE_REPLY){
                        auto it=arpTabel.find(arpmsg.target_ip_address);
                        if(it==arpTabel.end()){
                            arpTabel[arpmsg.target_ip_address]={arpmsg.target_ethernet_address,ms_since_construct+30*1000};
                            auto it2=frame_in_stand.find(arpmsg.target_ip_address);
                            if(it2!=frame_in_stand.end()){
                                while(!it2->second.empty()){
                                    it2->second.front().header.dst=arpmsg.target_ethernet_address;
                                    transmit(move(it2->second.front()));
                                    it2->second.pop();
                                }
                            }
                        }
                    }
                }
                break;
            }
            default: break;
        }
    }
  debug( "unimplemented recv_frame called" );
  (void)frame;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick({}) called", ms_since_last_tick );
  ms_since_construct+=ms_since_last_tick;
  for(auto it=arpTabel.begin();it!=arpTabel.end();){
    if(it->second.TTL<=ms_since_construct){
        it=arpTabel.erase(it);
    }
    else{
        it++;
    }
  }
}
