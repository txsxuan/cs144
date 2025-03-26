#include "byte_stream.hh"

using namespace std;
// #define MAX_LEN 1024 
ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Your code here.
  //这里是单线程，check0，不要想多
  if(isclosed||data.size()==0||available_capacity()==0){
    set_error();
    return;
  }
  int writesize=(data.size()<available_capacity())?data.size():available_capacity();
  data.resize(writesize);
  for(char bit:data){
    buffer.emplace(bit);
  }
  bytesPushed+=writesize;
}

void Writer::close()
{
  isclosed=true;
  // Your code here.
}

bool Writer::is_closed() const
{
  return isclosed; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return (capacity_-buffer.size()); // Your code here.
}

uint64_t Writer::bytes_pushed() const
{
  return bytesPushed; // Your code here.
}

string_view Reader::peek() const
{
  return string_view(&buffer.front(),1); // Your code here.
}

void Reader::pop( uint64_t len )
{
  if(len>buffer.size()){
    set_error();
    return ;
  }
  for(uint64_t i=0;i<len;i++){
    buffer.pop();
  }
  bytesPopped+=len;
}

bool Reader::is_finished() const
{
  return isclosed&&(buffer.size()==0); // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return buffer.size(); // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return bytesPopped; // Your code here.
}
