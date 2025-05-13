#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seq.unwrap(isn_, windowLeft)-windowLeft;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissions;
}

void TCPSender::push( const TransmitFunction& transmit )
{
    if((is_connected.has_value()
        &&!is_connected.value())
        ||is_closed.has_value()
        ){
            return;
    }
    debug("seq = {}",seq.getRaw());
    while((windowSize==0?1:windowSize)>sequence_numbers_in_flight()&&!is_closed)
    {
        TCPSenderMessage msg=make_empty_message();
        if(!is_connected.has_value()){
            msg.SYN=true;
            is_connected=false;
        }
        uint16_t remain=(windowSize==0?1:windowSize)-sequence_numbers_in_flight();
        uint16_t len=min(remain,static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE));
        read(reader(),len-msg.sequence_length(),msg.payload);
        if(reader().is_finished()&&(msg.sequence_length()<remain)){
            msg.FIN=true;
            is_closed=false;
        }
        if(msg.sequence_length()==0){
            return;
        }
        debug("message sent with seq=Wrap32<{}> {}SYN {} {}FIN {}RST", msg.seqno.getRaw(),msg.SYN?'+':'-',msg.payload.size()?msg.payload:"(no payload)",msg.FIN?'+':'-',msg.RST?'+':'-');
        seq=seq+msg.sequence_length();
        debug("after pushed seq = {}",seq.getRaw());
        transmit(msg);
        msgq.emplace(std::move(msg));
        if(!timer.is_start()){
            timer.turn_on();
        }
    }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return {seq,false,"",false,reader().has_error()};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
    if(msg.RST){
        reader().set_error();
        return;
    }
    windowSize=msg.window_size;
    uint64_t ack=msg.ackno->unwrap(isn_, windowLeft);
    if(ack>windowLeft&&ack<=seq.unwrap(isn_, windowLeft)){
        timer.turn_off();
        retransmissions=0;
        is_connected=true;
        while(!msgq.empty()&&((msgq.front().seqno.unwrap(isn_, windowLeft))+msgq.front().sequence_length())<=ack){
            windowLeft+=msgq.front().sequence_length();
            msgq.pop();
        }
        if(!msgq.empty()){
            timer.turn_on();
            return;
        }
        if(is_closed.has_value()){
            debug("is finished!");
            is_closed=reader().is_finished();
        }
    }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
    if(msgq.empty()){
        return;
    }
    if(timer.is_start()){
        timer.update(ms_since_last_tick);
        debug("time = {} ,RTO = {} ,initial = {}", timer.getTime(),timer.getRTO(),timer.getinitial());
        if(timer.is_expired()){
            debug("timer is expired!");
            transmit(msgq.front());
            debug("seqno = Wrap32<{}> {}SYN ,window_size = {}", msgq.front().seqno.getRaw(),(msgq.front().SYN)?'+':'-',windowSize);
            if(windowSize||msgq.front().SYN){
                retransmissions++;
                timer.back_off();
                debug("back off");
            }
            timer.reset();
        }
    }
}
