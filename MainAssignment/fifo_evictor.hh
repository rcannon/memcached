/*
 * Declarations for a FIFO (First In, First Out) evictor, according to the pattern in evictor.hh
 * for use in a cache according to the pattern in cache.hh
 */


#pragma once
#include "evictor.hh"
#include <queue>

class Fifo_Evictor : public Evictor {
  private:
    // a fifo queue in which to store keys
    std::queue<key_type> keyq_;
  public:

    Fifo_Evictor() = default;
    ~Fifo_Evictor() = default;
    Fifo_Evictor(const Fifo_Evictor&) = delete;
    Fifo_Evictor& operator=(const Fifo_Evictor&) = delete;
    
    // pushes a key onto back of queue
    void touch_key(const key_type&) override;

    // pops first key off front and returns it
    const key_type evict() override;
};
