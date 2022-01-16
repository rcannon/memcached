/*
 * Implementation of a FIFO (First In, First Out) evictor for use in a cache following the patterns in cache.hh
 * Stores values in a standard library queue, pushing values to the back when touched and popping values from when evicting.
 */

#include "fifo_evictor.hh"
#include <thread>
#include <mutex>
#include <cassert>

Fifo_Evictor::Fifo_Evictor()
  : mutx_(std::mutex()) {}
 
// pushes key onto back of queue
void
Fifo_Evictor::touch_key(const key_type& key)
{
  std::scoped_lock guard(mutx_);
  keyq_.push(key);
}

// pops key at front of queue and returns it to user
const key_type
Fifo_Evictor::evict()
{
  key_type x = "";
  if (keyq_.empty()) return x;
  x = keyq_.front();
  std::scoped_lock guard(mutx_);
  keyq_.pop();
  return x;
}
