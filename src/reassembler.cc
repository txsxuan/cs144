#include "reassembler.hh"
#include "debug.hh"
#include <cassert>
#include <iterator>

bool Reassembler::newinserthelp( uint64_t first_index, std::string& data )
{
  if ( !unassembled.empty() ) {
    auto ub = unassembled.upper_bound( first_index + data.size() ); // ub包括之后的都和data没关系了
    auto lb = unassembled.lower_bound( first_index ); // lb到ub之间的字符串全部需要拼起来。
    if ( ub != unassembled.begin() ) {
      // debug("{} {}", lb->second,std::prev(ub)->second);
      if ( lb != ub ) { // ub之前的和lb前面的不是同一块,那样需要考虑是否要将这部分拼进去
        if ( std::prev( ub )->first + std::prev( ub )->second.size() > ( first_index + data.size() ) ) {
          data.append( std::move(
            std::prev( ub )->second.substr( ( first_index + data.size() - std::prev( ub )->first ) ) ) ); //
        }
      }
      if ( lb != unassembled.begin() ) { // lb之后的字符串全部都在ta里面
        uint64_t temp
          = std::prev( lb )->first + std::prev( lb )->second.size(); // temp是第一个不在prev里面的字符的序列号
        if ( temp >= first_index ) { // 如果temp和first_index相等，那么很显然需要将整个data拼进去
          if ( temp - first_index <= data.size() ) { // 判断data是不是整个都在prev里面
            std::prev( lb )->second.append( std::move( data.substr( temp - first_index ) ) );
          }
          unassembled.erase( lb, ub );
          return true;
        }
      }
      unassembled.erase( lb, ub );
    }
  }
  if ( output_.writer().bytes_pushed() < first_index ) {
    unassembled[first_index] = std::move( data );
    return true;
  }
  return false;
}
void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring )
{
  debug( "index = {} , data = {} , FIN = {} \n", first_index, data, is_last_substring );
  uint64_t Firstunaccept = ( output_.writer().bytes_pushed() + writer().available_capacity() );
  const auto checkclose = [&]() {
    if (
      haslastSubstr
      && unassembled
           .empty() ) { // 如果已经没有被assemble的数据报，并且我知道已经有了最后一个子串，那么就要关闭我们的写端了（或者说，in_bound)
      output_.writer().close();
      debug( "closed: {}", data );
    }
  };
  if ( writer().is_closed() || first_index >= Firstunaccept
       || writer().available_capacity()
            == 0 ) { // 如果写端已经关闭了就不再接收了，或者干脆大于Firstunaccept，也没什么好说的了
    return;
  }
  if ( data.empty() ) {
    if ( output_.writer().bytes_pushed() == first_index && is_last_substring ) {
      haslastSubstr = true;
      return checkclose();
    }
    return;
  }
  if ( output_.writer().bytes_pushed() > first_index ) {
    if ( output_.writer().bytes_pushed() >= first_index + data.size() ) {
      return;
    }
    data = data.substr( output_.writer().bytes_pushed() - first_index );
    first_index = output_.writer().bytes_pushed();
  }
  if ( first_index + data.size() - 1 >= Firstunaccept ) { // 说明有一部分是无法被接受进来的
    data.resize( Firstunaccept - first_index );
  } else if ( is_last_substring ) {
    haslastSubstr = true;
  }
  if ( !newinserthelp( first_index, data ) ) {
    output_.writer().push( move( data ) );
  }
  return checkclose();
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( const auto& [key, value] : unassembled ) {
    count += value.size();
  }
  return count;
}
