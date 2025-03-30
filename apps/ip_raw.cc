#include "socket.hh"

using namespace std;

class RawSocket : public DatagramSocket
{
public:
  RawSocket() : DatagramSocket( AF_INET, SOCK_RAW, IPPROTO_RAW ) {}
  RawSocket(int protocol):DatagramSocket( AF_INET, SOCK_RAW, protocol ) {}
};

int main()
{
  RawSocket check1sock(17);
  std::string payload("hello,this is wsl!");
  std::string datagram;
  uint16_t dest=80;
  uint16_t source=0;
  datagram+=char(source>>8);
  datagram+=char(source&0xff);
  datagram+=char(dest>>8);
  datagram+=char(dest&0xff);
  uint16_t length=8+payload.size();
  datagram+=char(length>>8);
  datagram+=char(length&0xff);
  datagram+=char(0);
  datagram+=char(0);
  datagram+=payload;
  check1sock.sendto(Address{"172.27.160.1"},datagram);

  // construct an Internet or user datagram here, and send using the RawSocket as in the Jan. 10 lecture

  return 0;
}
