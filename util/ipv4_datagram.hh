#pragma once

#include "ipv4_header.hh"
#include "parser.hh"
#include "ref.hh"

#include <string>
#include <vector>

//! \brief [IPv4](\ref rfc::rfc791) Internet datagram
struct IPv4Datagram
{
  IPv4Header header {};
  std::vector<Ref<std::string>> payload {};

  void parse( Parser& parser )
  {
    header.parse( parser );
    parser.truncate( header.payload_length() );
    parser.all_remaining( payload );
  }

  void serialize( Serializer& serializer ) const
  {
    header.serialize( serializer );
    serializer.buffer( payload );
  }
  bool operator==(const IPv4Datagram& rhs)
    {
        // 首先比较 header（假设 IPv4Header 实现了 operator==）
        if (header.to_string() != rhs.header.to_string())
            return false;

        // 比较 payload 长度
        if (payload.size() != rhs.payload.size())
            return false;

        // 比较 payload 中每个 Ref<std::string> 解引用后的值
        for (size_t i = 0; i < payload.size(); ++i) {
            if ((payload[i].get()) != (rhs.payload[i].get()))
                return false;
        }

        return true;
    }

};

using InternetDatagram = IPv4Datagram;
