#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point + static_cast<uint32_t>(n % (1UL << 32)) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // want to find closest raw_value_ - zero_point + x* (1UL << 32) to checkpoint
  // aka closest value  = raw_value - zero_point (mod 2^32) to checkpoint
  uint64_t remainder = (raw_value_ + (1UL << 32) - zero_point.raw_value_) % (1UL << 32);
  uint64_t factor = checkpoint / (1UL << 32);
  uint64_t min_diff = UINT64_MAX;
  uint64_t closest = 0;
  for (int i=-1; i<=1; ++i){
    uint64_t test_val = (factor + i) * (1UL << 32) + remainder;
    if (min_diff > max(test_val, checkpoint) - min(test_val, checkpoint)){
      min_diff = max(test_val, checkpoint) - min(test_val, checkpoint);
      closest = test_val;
    }
  }
  return closest;
}
