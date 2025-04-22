#include "tcp_receiver.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if (message.SYN){
    zero_point_ = message.seqno;
    SYNCED_ = true;
  }
  
  if (message.RST)
    RST_ = message.RST;

  if (SYNCED_ && !RST_){
    uint64_t stream_index = message.seqno.unwrap(zero_point_.value(), checkpoint_) - !message.SYN;
    checkpoint_ = stream_index + message.payload.size();

    reassembler_.insert(stream_index, message.payload, message.FIN);
  }

}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage ack;
  if (SYNCED_)
    ack.ackno = Wrap32::wrap(writer().bytes_pushed() + SYNCED_ + writer().is_closed(), zero_point_.value());
  ack.window_size = min<uint64_t>(writer().available_capacity(), UINT16_MAX);
  ack.RST = RST_;
  return ack;
}
