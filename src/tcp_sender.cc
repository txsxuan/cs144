#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cassert>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <iostream>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
//   debug( "unimplemented sequence_numbers_in_flight() called" );
  return seq.unwrap(isn_, windowLeft)-windowLeft;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
//   debug( "unimplemented consecutive_retransmissions() called" );
  return retransmissions;
}

void TCPSender::push( const TransmitFunction& transmit )
{
    try {
        if(reader().has_error()){
            throw std::runtime_error("bytes has error!");
        }
        if((is_closed.has_value()&&is_closed.value())||(!msgq.empty()&&windowLeft==0)){
            return;
        }
        debug("seq = {} , isn_ = {} , windowsize = {}", seq.unwrap(isn_, windowLeft),isn_.unwrap(isn_, windowLeft),window_size);
        // if(reader().is_finished()){
        //     return;
        // }
        if(window_size==0&&reader().bytes_buffered()&&!writer().is_closed()){
            debug("windowsize == 0");
            transmit(make_empty_message());
            return;
        }
        while((reader().bytes_buffered()
                    ||seq==isn_//如果的是seq==isn_，那么无论如何都要发
                    ||reader().is_finished()
                    )
                    &&window_size>0&&!is_closed.has_value()
                        ){
            uint16_t len=((window_size)<TCPConfig::MAX_PAYLOAD_SIZE)?window_size:TCPConfig::MAX_PAYLOAD_SIZE;
            std::string payload;
            read(reader(),len, payload);
            const bool FIN=reader().is_finished()&&payload.size()!=window_size;//不能在窗口满的时候加上FIN，因为它也占一位
            debug("seq = {} , isn_ = {} , payload = {} ,FIN = {} ,reader_is_finished = {}", 
                seq.unwrap(isn_, windowLeft),
                isn_.unwrap(isn_, windowLeft),
                payload,
                FIN,
                reader().is_finished());
            TCPSenderMessage msg{seq,seq==isn_,std::move(payload),FIN,reader().has_error()};
            seq=seq+msg.sequence_length();
            transmit(msg);
            // if(window_size>msg.sequence_length()){
            //     window_size-=msg.sequence_length();
            // }
            window_size-=msg.sequence_length();
            msgq.emplace(std::move(msg));
            debug("msgq.size = {}", msgq.size());
            if(reader().is_finished()&&FIN){
                is_closed=false;
                break;
            }
        }
        // if(window_size==0&&reader().bytes_buffered()){
        //     transmit(make_empty_message());
        // }
        if(!msgq.empty()&&!timer.is_start()){ 
            timer.turn_on();
        }
    
    } catch (const exception& e) {
        std::cout<<e.what()<<std::endl;
        if(timer.is_start()){
            timer.turn_off();
        }
        return;
    }

}

TCPSenderMessage TCPSender::make_empty_message() const
{
//   debug( "unimplemented make_empty_message() called" );
  return {seq,false,"",false,reader().has_error()};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
    if(msg.RST){
        reader().set_error();
        return;
    }

    uint64_t ack=msg.ackno->unwrap(isn_, windowLeft);
    if(ack>windowLeft&&ack<=seq.unwrap(isn_, windowLeft)){
        timer.turn_off();
        retransmissions=0;
        // debug("what ? {}", +msgq.front().sequence_length());
        while(!msgq.empty()&&((msgq.front().seqno.unwrap(isn_, windowLeft))+msgq.front().sequence_length())<=ack){
            debug("seq = {} , size = {}", msgq.front().seqno.getRaw(),msgq.front().sequence_length());
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
    window_size=(msg.window_size>=sequence_numbers_in_flight())?(msg.window_size-sequence_numbers_in_flight()):0;
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
            debug("seqno = Wrap32<{}> {}SYN ,window_size = {}", msgq.front().seqno.getRaw(),(msgq.front().SYN)?'+':'-',window_size);
            if(window_size!=0||msgq.front().SYN||msgq.front().FIN){
                retransmissions++;
                timer.back_off();
                debug("back off");
            }
            timer.reset();
        }
    }
}
