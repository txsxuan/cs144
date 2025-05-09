#include "tcp_receiver.hh"
#include "debug.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( message.SYN ) {
    zeropoint = message.seqno;
  }
  if ( zeropoint ) {
    reassembler_.insert( message.seqno.unwrap( zeropoint.value(), writer().bytes_pushed() + 1 )
                           - static_cast<uint64_t>( !message.SYN ),
                         std::move( message.payload ),
                         message.FIN );
    debug( "bytes pushed {}\n", writer().bytes_pushed() );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  std::optional<Wrap32> ackno {};
  if ( zeropoint ) {
    ackno = Wrap32::wrap( writer().bytes_pushed() + 1 + static_cast<uint64_t>( writer().is_closed() ),
                          zeropoint.value() );
  }
  uint16_t window_size
    = ( UINT16_MAX < writer().available_capacity() ) ? ( UINT16_MAX ) : writer().available_capacity();
  return { ackno, window_size, writer().has_error() };
}
