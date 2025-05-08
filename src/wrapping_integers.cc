#include "wrapping_integers.hh"
#include "debug.hh"
#include <climits>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n + zero_point.raw_value_ ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t ret { static_cast<uint64_t>(
    raw_value_
    - zero_point
        .raw_value_ ) }; // 这里首先得到32位的绝对地址，然后让它和checkpoint的高32位组合，来得到一个待定的返回值
  ret = ret | ( checkpoint >> 32 << 32 );
  // debug("checkpoint = {} ret = {}\n", checkpoint,ret);
  if ( ret > checkpoint
       && ( ret >= 1ULL << 32 ) ) { // 如果这时候retbicheckpoint要大，那么它是有可能离checkpoint更近的
    ret = ( ( ret - checkpoint ) < ( checkpoint - ( ret - ( 1ULL << 32 ) ) ) ) ? ret : ( ret - ( 1ULL << 32 ) );
  } else if ( ret < checkpoint && ( ret < 0xffffffff00000000 ) ) {
    ret = ( ( checkpoint - ret ) < ( ret + ( 1ULL << 32 ) - checkpoint ) ) ? ret : ( ret + ( 1ULL << 32 ) );
  }
  // debug("now ret = {}\n", ret);
  return ret;
}
