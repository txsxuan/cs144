#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ),seq(isn),timer(initial_RTO_ms){}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
    class RetransmissionTimer{
        friend class TCPSender;
        RetransmissionTimer(uint64_t initial_RTO_ms):RTO(initial_RTO_ms),initial_RTO_ms_(initial_RTO_ms){};
        void reset(){//仅仅计时器归零

            expired=false;
            timehold=0;

        };
        void turn_on(){//初始化计时器并且归零
            RTO=initial_RTO_ms_;
            isstart=true;
            reset();
        }
        void turn_off(){
            isstart=false;
        }
        bool is_expired(){
            return expired;
        }
        void back_off(){
            RTO=(RTO>>63==0)?(RTO*2):UINT16_MAX;
        }
        bool is_start(){
            return isstart;
        }
        void update(uint64_t ms_since_last_tick){
            timehold=(timehold<(UINT64_MAX-ms_since_last_tick))?(timehold+ms_since_last_tick):UINT64_MAX;
            expired=(timehold>=RTO);

        }
        uint64_t getTime() const {return timehold;}
        uint64_t getRTO() const {return RTO;}
        uint64_t getinitial() const {return  initial_RTO_ms_;}
        bool isstart=false;
        bool expired=false;
        uint64_t RTO;
        uint64_t initial_RTO_ms_;
        uint64_t timehold{};

    };
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint16_t window_size{1};
  Wrap32 seq;
  uint64_t windowLeft=0;
  std::queue<TCPSenderMessage> msgq{};
  uint64_t retransmissions=0;
  RetransmissionTimer timer;
  std::optional<bool> is_closed=std::nullopt;
};
