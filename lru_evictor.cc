/*
 * Implementation of an LRU_Evictor according to the declarations in lru_evictor.hh
 * Stores keys as nodes in a doubly linked list, moving keys to the back when touched
 * and taking keys from the front when needed for eviction.
 * Stores pointers to each node in an unordered map so that the list can be effectively searched and accessed in constant time.
 */


#include "lru_evictor.hh"
#include <cassert>
#include <iostream>
#include <memory>

LRU_Evictor::LRU_Evictor()
  : LL_(new LList())
{}

void
LRU_Evictor::touch_key(const key_type& key)
// Inserts a new key to the back of the linked list, or moves an old key to the back
{
    auto it = map_.find(key);
    if (it == map_.end()) {
        auto N = std::make_shared<Node>(key, nullptr, nullptr);
        N->prev_ = LL_->back_;
        if (LL_->back_ != nullptr) LL_->back_->next_ = N;
        LL_->back_ = N;
        if (LL_->root_ == nullptr) LL_->root_ = N;
        map_[key] = N;
        assert(N->next_ == nullptr);
    } else {
        assert(LL_->back_ != nullptr);
        assert(LL_->root_ != nullptr);
        std::shared_ptr<Node> N = it->second;
        if (LL_->back_ != N){
            if (LL_->root_ == N) {
                LL_->root_ = N->next_;
            } else {
                N->prev_->next_ = N->next_;
            }
            N->next_->prev_ = N->prev_;
            LL_->back_->next_ = N;
            N->prev_ = LL_->back_;
            LL_->back_ = N;
            N->next_ = nullptr;
        }
    }
}
const key_type
LRU_Evictor::evict()
// returns the front element of the linked list and removes it from the unordered map
{
  if (LL_->root_ != nullptr){
    std::shared_ptr<Node> N = LL_->root_;
    if (N->next_ !=nullptr) N->next_->prev_ = nullptr;
    LL_->root_ = N->next_;
    if (LL_->back_ == N) LL_->back_ = nullptr;
    key_type k = N->key_;
    map_.erase(k);
    return k;
  } else {
    return "";
  }
}

LRU_Evictor::~LRU_Evictor()
// destructor needs to delete all the pointers going in one direction in the linked list
// to prevent mutual ownership between nodes, which prevents automatic deallocation by shared pointers since neither can deallocate the other.
{
  std::shared_ptr<Node> n = LL_->root_;
  if (n == nullptr) 
  {
    delete LL_;
    return;
  }
  assert(LL_->back_ != nullptr);
  while (n->next_ != nullptr)
  {
    n = n->next_;
    n->prev_ = nullptr;
  }
  delete LL_;
}
