#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>


bool Reassembler::newinserthelp( uint64_t first_index, std::string &data ){
    if(!unassembled.empty()){
        auto ub = unassembled.upper_bound( first_index + data.size() ); // ub包括之后的都和data没关系了
        auto lb = unassembled.upper_bound( first_index ); // lb到ub之间的字符串全部需要拼起来。
        if(ub!=unassembled.begin()){
            if ( lb != ub ) { // ub之前的和lb前面的不是同一块,那样需要考虑是否要将这部分拼进去
                if ( std::prev( ub )->first + std::prev( ub )->second.size() > ( first_index + data.size() ) ) {
                  data.append(
                    std::move( std::prev( ub )->second.substr( ( first_index + data.size() - std::prev( ub )->first ) ) ) ); //
                }
            }
            if ( lb != unassembled.begin() ) { // lb之后的字符串全部都在ta里面
                uint64_t temp
                  = std::prev( lb )->first + std::prev( lb )->second.size(); // temp是第一个不在prev里面的字符的序列号
                if ( temp >= first_index ) { // 如果temp和first_index相等，那么很显然需要将整个data拼进去
                  if ( temp - first_index <= data.size() ) { // 判断data是不是整个都在prev里面
                    // debug("temp : {} ;{} {} {}");
                    std::prev( lb )->second.append( std::move( data.substr( temp - first_index ) ) );
                  }
                return true;
                } 
            } 
            unassembled.erase( lb, ub );                        
        }
    }
    else if(acknum<first_index){
        unassembled[first_index]=std::move(data);
        return true;        
    }
    return false;
}

void Reassembler::inserthelp( uint64_t first_index, std::string &data )
{
  if ( unassembled.empty() ) {
    unassembled[first_index] = std::move( data );
    return;
  }
  auto ub = unassembled.upper_bound( first_index + data.size() ); // ub包括之后的都和data没关系了
  auto lb = unassembled.upper_bound( first_index ); // lb到ub之间的字符串全部需要拼起来。
  if ( ub == unassembled.begin() ) {
    unassembled[first_index] = std::move( data );
    return;
  }
  if ( lb != ub ) { // ub之前的和lb前面的不是同一块,那样需要考虑是否要将这部分拼进去
    if ( std::prev( ub )->first + std::prev( ub )->second.size() > ( first_index + data.size() ) ) {
      data.append(
        std::move( std::prev( ub )->second.substr( ( first_index + data.size() - std::prev( ub )->first ) ) ) ); //
    }
  }
  // 考虑将data拼进lb前面的
  if ( lb != unassembled.begin() ) { // lb之后的字符串全部都在ta里面
    uint64_t temp
      = std::prev( lb )->first + std::prev( lb )->second.size(); // temp是第一个不在prev里面的字符的序列号
    if ( temp >= first_index ) { // 如果temp和first_index相等，那么很显然需要将整个data拼进去
      if ( temp - first_index <= data.size() ) { // 判断data是不是整个都在prev里面
        // debug("temp : {} ;{} {} {}");
        std::prev( lb )->second.append( std::move( data.substr( temp - first_index ) ) );
      } else { // 否则什么都不用干
        return;
      }
    } else {
      unassembled[first_index] = std::move( data );
    }
  } else { // 否则
    unassembled[first_index] = std::move( data );
  }
  unassembled.erase( lb, ub );
}
void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring )
{

  uint64_t Firstunaccept = ( acknum + writer().available_capacity() );
  if ( haslastSubstr ) {
    debug( "has last str: {}", data );
  }
  debug( "acknum: {}; firstunaccept: {}", acknum, Firstunaccept );
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
    if ( acknum == first_index && is_last_substring ) {
      haslastSubstr = true;
      return checkclose();
    }
    return;
  }
  if ( acknum > first_index ) {
    if ( acknum >= first_index + data.size() ) {
      return;
    }
    data = data.substr( acknum - first_index );
    first_index = acknum;
  }
  if ( first_index + data.size() - 1 >= Firstunaccept ) { // 说明有一部分是无法被接受进来的
    debug( "{}   {}   {}", first_index, data.size(), Firstunaccept );
    data.resize( Firstunaccept - first_index );
  } else if ( is_last_substring ) {
    haslastSubstr = true;
  }
  if ( acknum == first_index ) { // 来的正是我需要的
    acknum += data.size();
    output_.writer().push( move( data ) );
    if ( unassembled.empty() ) { // 如果为空
      return checkclose();
    }
    if ( acknum == ( unassembled.begin()->first ) ) {
      acknum += unassembled.begin()->second.size();
      output_.writer().push( move( unassembled.begin()->second ) );
      unassembled.erase( unassembled.begin() );
      return checkclose();
    }
    auto it = unassembled.lower_bound( acknum );
    if ( it
         == unassembled
              .end() ) { // 说明所有在unassembled中的元素的索引都比acknum小，清空unassembled这个容器再返回即可。
      auto previt = std::prev( it );
      int splitsize = previt->first + previt->second.size() - acknum;
      if ( splitsize > 0 ) {
        output_.writer().push( ( previt->second.substr( acknum - previt->first ) ) );
        acknum += splitsize;
      }
      unassembled.clear();
      return checkclose();
    } else {
      if ( it == unassembled.begin() ) {
        return;
      }
      auto previt = std::prev( it );
      int splitsize = previt->first + previt->second.size() - acknum;
      debug( "previt->first: {} ; +previt->second.size(): {} ; acknum: {} ; split {} ",
             previt->first,
             previt->second.size(),
             acknum,
             splitsize );
      if ( splitsize > 0 ) {
        output_.writer().push( ( previt->second.substr( acknum - previt->first ) ) );
        acknum += splitsize;
      }
      unassembled.erase( unassembled.begin(), it );
      if ( it->first == acknum ) {
        acknum += splitsize;
        output_.writer().push( std::move( it->second ) );
        unassembled.erase( it );
      }
    }
  } else {
    inserthelp( first_index, data);
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
  // debug( "unimplemented count_bytes_pending() called" );
  return count;
}
