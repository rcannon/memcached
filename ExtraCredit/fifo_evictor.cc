/*
 * Implementation of a FIFO (First In, First Out) evictor for use in a cache following the patterns in cache.hh
 * Stores values in a standard library queue, pushing values to the back when touched and popping values from when evicting.
 */

#include "fifo_evictor.hh"
#include <cassert>
 
// pushes key onto back of queue
void
Fifo_Evictor::touch_key(const key_type& key)
{
  keyq_.push(key);
}

// pops key at front of queue and returns it to user
const key_type
Fifo_Evictor::evict()
{
  key_type x = "";
  if (keyq_.empty()) return x;
  x = keyq_.front();
  keyq_.pop();
  return x;
}
