#include "byte_stream.hh"
#include <iostream>

using namespace std;
ByteStream::ByteStream( uint64_t capacity )
  : bytesPushed( 0 ), bytesPopped( 0 ), hasbuffered( 0 ), hasremoved( 0 ), capacity_( capacity ), isclosed( false )
{}

void Writer::push( string data )
{
  // Your code here.
  // 这里是单线程，check0，不要想多
  if ( isclosed || data.size() == 0 || available_capacity() == 0 ) {
    set_error();
    return;
  }
  int writesize = ( data.size() < available_capacity() ) ? data.size() : available_capacity();
  data.resize( writesize );
  buffer.emplace( move( data ) );
  bytesPushed += writesize;
  hasbuffered += writesize;
}

void Writer::close()
{
  isclosed = true;
  // Your code here.
}

bool Writer::is_closed() const
{
  return isclosed; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return ( capacity_ - hasbuffered ); // Your code here.
}

uint64_t Writer::bytes_pushed() const
{
  return bytesPushed; // Your code here.
}

string_view Reader::peek() const
{
  return ( hasbuffered > 0 ) ? string_view( buffer.front().data() + hasremoved, buffer.front().size() - hasremoved )
                             : string_view {}; // Your code here.
}

void Reader::pop( uint64_t len )
{
  if ( len > hasbuffered ) {
    set_error();
    return;
  }
  bytesPopped += len;
  hasbuffered -= len;
  while ( len > 0 && len >= ( buffer.front().size() - hasremoved ) ) {
    len -= ( buffer.front().size() - hasremoved );
    buffer.pop();
    hasremoved = 0;
  }
  if ( len > 0 ) {
    hasremoved += len;
  }
}

bool Reader::is_finished() const
{
  return isclosed && ( hasbuffered == 0 ); // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return hasbuffered; // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return bytesPopped; // Your code here.
}
