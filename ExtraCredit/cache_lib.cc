/*
 * Implementaion for promised interface in cache.hh
 * Uses the pImpl idiom to hide details from the user.
 */
#include <utility>
#include <memory>
#include <cassert>
#include <string.h>
#include <iostream>
#include "cache.hh"
#include "fifo_evictor.hh"
#include <mutex>
#include <thread>

class Cache::Impl 
{
  private:
    const Cache::size_type maxmem_;
    int64_t remmem_;
    const float max_load_factor_;
    Evictor* evictor_;
    const Cache::hash_func hasher_;
    std::unordered_map<key_type, std::pair<Cache::val_type, Cache::size_type>, Cache::hash_func> tbl_;
    std::mutex data_lock_;
    std::mutex touch_lock_;
  public:

    Impl(Cache::size_type maxmem,
        float max_load_factor = 0.75,
        Evictor* evictor = nullptr,
        Cache::hash_func hasher = std::hash<key_type>());
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    void set(key_type key, Cache::val_type val, Cache::size_type size);
    Cache::val_type get(key_type key, Cache::size_type& val_size) const;
    bool del(key_type key);
    Cache::size_type space_used() const;
    void reset();
};

Cache::Impl::Impl(Cache::size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        Cache::hash_func hasher)
        : maxmem_(maxmem), remmem_(maxmem), max_load_factor_(max_load_factor), 
          evictor_(evictor), hasher_(hasher), tbl_(5, hasher_),
          data_lock_(std::mutex()), touch_lock_(std::mutex())
{
  tbl_.max_load_factor(max_load_factor_);
}

  // Create a new cache object with the following parameters:
  // maxmem: The maximum allowance for storage used by values.
  // max_load_factor: Maximum allowed ratio between buckets and table rows.
  // evictor: Eviction policy implementation (if nullptr, no evictions occur
  // and new insertions fail after maxmem has been exceeded).
  // hasher: Hash function to use on the keys. Defaults to C++'s std::hash.
Cache::Cache(Cache::size_type maxmem,
        float max_load_factor,
        Evictor* evictor,
        Cache::hash_func hasher)
        : pImpl_(new Cache::Impl(maxmem, max_load_factor, evictor, hasher))
{}

  // Constructor for networked cache client, only defined in cache_client.cc
/*Cache::Cache(std::string host, std::string port){
  host = port;
  assert(false);
}*/

Cache::Impl::~Impl()
{
  for (auto it = tbl_.begin(); it != tbl_.end(); it++)
  {
    delete[] it->second.first;
  }
  if (evictor_ != nullptr){
    delete evictor_;
  }
}

Cache::~Cache(){}


  // Add a <key, value> pair to the cache.
  // If key already exists, it will overwrite the old value.
  // Both the key and the value are deep-copied.
  // If maxmem capacity is exceeded, enough values will be removed
  // from the cache to accomodate the new value. If unable, the new value
  // isn't inserted to the cache and no values are removed.
void 
Cache::Impl::set(key_type key, Cache::val_type val, Cache::size_type size)
{
  if (size > maxmem_) return; 
  assert(key != ""); /* key can't be empty string */
  del(key); // prevents unnecessary eviction in the case of an overwrite.
  {
    std::scoped_lock guard(data_lock_);
    if (remmem_ - size < 0)
    {
      if (evictor_ == nullptr) return;
      else
      {
        key_type evictKey;
        while (remmem_ - size < 0)
        {
          evictKey = evictor_->evict();
          if (evictKey != "") {
            val = tbl_.find(evictKey);
            if (val != tbl_.end()){
              auto evictSize = val->second.second;
              delete[] val->second.first;
              tbl_.erase(key);
              remmem_ += evictSize;
            }
          }
        }
      }
    }
    Cache::byte_type* theVal = new Cache::byte_type[size]; /*assumes user includes space for 0 termination if passing a string */
    std::copy(val,val+size, theVal);
    tbl_[key] = std::make_pair(theVal,size);
    remmem_ -= size;
  }
  if (!evictor_) return;
  {
    std::scoped_lock guard(touch_lock_);
    evictor_->touch_key(key);
  }
  return;
}


  // Retrieve a pointer to the value associated with key in the cache,
  // or nullptr if not found.
  // Sets the actual size of the returned value (in bytes) in val_size.
Cache::val_type
Cache::Impl::get(key_type key, Cache::size_type& val_size) const
{
  auto val = tbl_.find(key);
  if (val == tvl_.end()) return nullptr;
  // if (tbl_.find(key) == tbl_.end()) return nullptr;
  // std::pair res = tbl_.at(key);
  // if (evictor_) evictor_->touch_key(key);
  if (evictor_) {
    std::scoped_lock guard(touch_lock_);
    evictor_touch_key(key);
  }
  val_size = val->second->second;
  return val->second->first;
}


  // Delete an object from the cache, if it's still there
bool 
Cache::Impl::del(key_type key)
{
  {
    std::scoped_lock guard(data_lock_);
    auto val = tbl_.find(key);
    if (val == tbl_.end()) return false;
    auto size = val->second.second;
    delete[] val->second.first;
    tbl_.erase(key);
    remmem_ += size;
  }
  return true;
  // if (tbl_.erase(key))
  // {
  //   remmem_ += size;
  //   assert(remmem_ <= maxmem_);
  //   return true;
  // }
  // return false; 
}

  // Compute the total amount of memory used up by all cache values (not keys)
Cache::size_type 
Cache::Impl::space_used() const
{
  return maxmem_ - remmem_;
}

  // Delete all data from the cache
void
Cache::Impl::reset()
{
  {
    std::scoped_lock guard(data_lock_);
    for (auto it = tbl_.begin(); it != tbl_.end(); it++)
    {
      delete[] it->second.first;
    }
    tbl_.clear();
    remmem_ = maxmem_;
  }
  return;
}

/* here are the cache methods, all they do is call the corresponding Impl methods */
void Cache::set(key_type key, Cache::val_type val, Cache::size_type size)
{
  return pImpl_->set(key, val, size);
}

Cache::val_type Cache::get(key_type key, Cache::size_type& val_size) const
{
  return pImpl_->get(key, val_size);
}

bool Cache::del(key_type key)
{
  return pImpl_->del(key);
}

Cache::size_type Cache::space_used() const
{
  return pImpl_->space_used();
}

void Cache::reset()
{
  return pImpl_->reset();
}

