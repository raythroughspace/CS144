#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t seqno_n = 0;
  for (const auto& msg: outstanding_)
    seqno_n += msg.sequence_length();
  
  return seqno_n;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retx_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // can send seqnos from checkpoint_ to checkpoint_ + window_size_
  // seqnos in outsantding_ guaranteed to lie in above interval
  while (sequence_numbers_in_flight() < max(window_size_, static_cast<unsigned short>(1))){
    TCPSenderMessage send{};
    read(reader(), min<ssize_t>(max(window_size_, static_cast<unsigned short>(1)) - sequence_numbers_in_flight(), 
          TCPConfig::MAX_PAYLOAD_SIZE), send.payload);
    send.seqno = Wrap32::wrap(checkpoint_ + sequence_numbers_in_flight(), isn_);
    send.SYN = send.seqno == isn_;
    send.RST = input_.has_error();
    if (reader().is_finished() && sequence_numbers_in_flight() + send.sequence_length() < max(window_size_, static_cast<unsigned short>(1)))
      send.FIN = true;
    if ((send.payload.empty() && !send.SYN && !send.FIN) 
    || last_segment_.FIN) // nothing to send OR last_segment was a FIN -> nothing to send anymore
      return; 
    outstanding_.push_back(send);
    transmit(send);
    if (!is_timer_on_){
      // start timer whenever a segment of nonzero legnth is sent
      is_timer_on_ = true;
      ms_since_RTO_ = 0;
    }
    
    if (checkpoint_ + sequence_numbers_in_flight() > max_seqno_sent_){
      max_seqno_sent_ = checkpoint_ + sequence_numbers_in_flight();
      last_segment_ = send;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage empty{.seqno = Wrap32::wrap(checkpoint_ + sequence_numbers_in_flight(), isn_), .RST = input_.has_error()};
  return empty;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST){
    input_.set_error();
    return; // stream error
  }
  window_size_ = msg.window_size;
  if (!msg.ackno.has_value())
    return;
  uint64_t ackno = msg.ackno.value().unwrap(isn_, checkpoint_);
  if (ackno > max_seqno_sent_)
    return; // ackno beyond maximum seqno sent is ignored
  for (auto it = outstanding_.begin(); it < outstanding_.end();){
    if (ackno >= it->seqno.unwrap(isn_, checkpoint_) + it->sequence_length()){
      it = outstanding_.erase(it);
    }
    else{
      ++it;
    }
  }
  if (ackno > checkpoint_){
    RTO_ms_ = initial_RTO_ms_;
    consecutive_retx_ = 0;
    is_timer_on_ = outstanding_.size() != 0;
    if(is_timer_on_)
      ms_since_RTO_ = 0;
    checkpoint_ = ackno;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (!is_timer_on_)
    return;

  ms_since_RTO_ += ms_since_last_tick;
  if (ms_since_RTO_ < RTO_ms_)
    return;
  
  uint64_t lowest_seqno = UINT64_MAX;
  TCPSenderMessage lowest_seq;
  for (const auto& seq: outstanding_){
    if (seq.seqno.unwrap(isn_, checkpoint_) < lowest_seqno){
      lowest_seq = seq;
      lowest_seqno = seq.seqno.unwrap(isn_, checkpoint_);
    }
  }
  transmit(lowest_seq);
  if (window_size_){
    ++consecutive_retx_;
    RTO_ms_ *= 2;
  }
  ms_since_RTO_ = 0;
}