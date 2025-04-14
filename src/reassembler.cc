#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  resize_buffer_();
  last_index_ = max(last_index_, first_index + data.size());
  if (is_last_substring)
    received_last_ = true;
    
  if (received_last_ && last_index_ == first_index_){
      output_.writer().close();
      return;
  }
  // store as much of the data as possible in buffer_
  for (uint64_t i = 0; i < data.size(); ++i){
    if (is_valid_index_(first_index + i))
      buffer_[first_index + i - first_index_] = data[i];
  }

  // empty as much of the buffer as possible
  uint64_t idx = 0;
  while (!buffer_.empty() && idx < buffer_.size() && buffer_[idx].has_value()){
    string s{buffer_[idx].value()};
    output_.writer().push(s);
    ++first_index_;
    ++idx;
    if (received_last_ && first_index_ == last_index_){
      output_.writer().close();
      return;
    }
  }
  if (!buffer_.empty())
    buffer_.erase(buffer_.begin(), buffer_.begin() + idx);
  
  resize_buffer_();
  check_rep_();
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t bytes = 0;
  for (uint64_t i = 0; i< buffer_.size(); ++i){
    if (buffer_[i].has_value() && first_index_ + i < last_index_)
      ++bytes;
  }
  return bytes;
}

void Reassembler::resize_buffer_()
{
  while (writer().available_capacity() > buffer_.size()){
    buffer_.push_back(nullopt);
  }
}

bool Reassembler::is_valid_index_(uint64_t idx) const
{
  return idx >= first_index_ && idx < first_index_ + writer().available_capacity();
}

void Reassembler::check_rep_() const
{
  assert(first_index_ <= last_index_);
  assert(buffer_.size() == writer().available_capacity());
}