#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  for (const auto c: data){
    if (available_capacity()){
      stream_.push_back(c);
      ++bytes_pushed_;
    }
  }
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - stream_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return string_view(stream_);
}

void Reader::pop( uint64_t len )
{
  if (len == 0 || is_finished()){
    return;
  }
  bytes_popped_ += len;
  stream_.erase(stream_.begin(), stream_.begin() + len);
}

bool Reader::is_finished() const
{
  return closed_ && stream_.size() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return stream_.size();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

