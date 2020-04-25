#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <thread>
#include "WorkloadGenerator.hh"



WorkloadGenerator::WorkloadGenerator(unsigned nsets, unsigned ngets, unsigned ndels,
                                     unsigned num_warmups, std::string host, std::string port)

  : cache_(Cache(host, port)),
    nsets_(nsets), ngets_(ngets), ndels_(ndels), num_warmups_(num_warmups), 
    keys_(std::vector<key_type>()), 
    vals_(std::vector<Cache::val_type>()), 
    sizes_(std::vector<Cache::size_type>()), 
    requests_(std::vector<std::string>()),
    random_device_(std::random_device()), 
    total_(nsets + num_warmups),
    gen_(std::mt19937(random_device_()))

{
  // easy condition check to simplify things
  assert(num_warmups_ < ngets_ + ndels_ + nsets_);
  fill_requests();
  fill_vals_and_sizes();
  fill_keys();
  assert(total_ <= keys_.size());
  assert(total_ == vals_.size());
  assert(total_ == sizes_.size());
}

// need to delete all the cstrings in the vals_ vector
WorkloadGenerator::~WorkloadGenerator()
{
  for (unsigned i = 0; i < vals_.size(); ++i) delete[] vals_[i];
}

// need a predetermined vector of requests, 
// with numbers of each request based on 
// the number of each request given as an 
// arguement in the constructor
void WorkloadGenerator::fill_requests()
{
  for (unsigned i = 0; i < ngets_; i++)
  {
    requests_.push_back("get");
  }
  for (unsigned i = 0; i < ndels_; i++)
  {
    requests_.push_back("del");
  }
  for (unsigned i = 0; i < nsets_; i++)
  {
    requests_.push_back("set");
  }
  std::shuffle(requests_.begin(), requests_.end(), gen_);

}

// the the vals vector with random length
// cstrings of random chararacters, then 
// put the length of that cstring into the 
// sizes_ vector, since some cache methods
// require the size. It is easiest to compute 
// it now
void WorkloadGenerator::fill_vals_and_sizes()
{
  unsigned total = num_warmups_ + nsets_;
  std::geometric_distribution<int> distribution(0.1);
  for (unsigned i = 0; i < total; i++)
  {
    sizes_.push_back(distribution(gen_)+2);
  }
  static const std::string lookup_table = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::uniform_int_distribution<int> dis1(0, lookup_table.size());
  std::string temp;
  for (unsigned i = 0; i < sizes_.size(); i++)
  {
    for (unsigned j = 1; j < sizes_.at(i); j++)
    {
      temp.push_back(lookup_table[dis1(gen_)]);
    }
    assert(temp != "");
    const auto svl = temp.c_str();
    Cache::size_type lgth = temp.length()+1;
    auto vl = new char[lgth];
    std::copy(svl, svl+lgth, vl);
    vals_.push_back(vl);
    temp.erase();
  }
}

// fill a string key of random length with 
// random charaters and add it to the keys vector.
// The key at index i will correcpond to the 
// value at index i and the value size at index i
// in the other vectors
//
// Repeat the key some random number of times to
// mimic nonlinear behavior of actual workload
void WorkloadGenerator::fill_keys()
{
  std::geometric_distribution<int> length_dist(0.1); 
  static const std::string lookup_table = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::uniform_int_distribution<int> lookup_dist(1, lookup_table.size()-1);
  std::geometric_distribution<int> repeat_dist(0.02);

  unsigned total = nsets_ + num_warmups_;
  std::string temp;
  unsigned size;
  unsigned repeats;
  for (unsigned i = 0; i < total;)
  {
    size = length_dist(gen_)+2; 
    for (unsigned j = 0; j < size; j++)
    {
      temp.push_back(lookup_table[lookup_dist(gen_)]);
    }
    repeats = repeat_dist(gen_)+1;
    for(unsigned k = 0; k < repeats; k++)
    {
      keys_.push_back(temp);
    }
    temp.erase();
    i = i + repeats;
  }
  std::shuffle(keys_.begin(), keys_.end(), gen_);
}

key_type WorkloadGenerator::get_key(unsigned i) const
{
  unsigned j = i % keys_.size();
  return keys_.at(j);
}

Cache::val_type WorkloadGenerator::get_val(unsigned i) const
{
  unsigned j = i % vals_.size();
  return vals_.at(j);
}

std::string WorkloadGenerator::get_req(unsigned i) const
{
  unsigned j = i % requests_.size();
  return requests_.at(j);
}

Cache::size_type WorkloadGenerator::get_size(unsigned i) const
{
  unsigned j = i % requests_.size();
  return sizes_.at(j);
}


// performance metrics are more representitive
// if the cache is already warm
void WorkloadGenerator::WarmCache()
{
  cache_.reset();
  for (unsigned i = 0; i < num_warmups_; i++)
  {
    cache_.set(get_key(i), get_val(i), sizes_.at(i));
  }
}

// get a random index for access during a "get"
// request. More recently added keys are more 
// likely to be chosen, to mimic temporal locality
// of actual workload

unsigned WorkloadGenerator::get_total() const
{
  return total_;
}

unsigned WorkloadGenerator::get_req_size() const
{
  return requests_.size();
}

unsigned WorkloadGenerator::get_num_warmups() const
{
  return num_warmups_;
}

unsigned WorkloadGenerator::get_ngets() const
{
  return ngets_;
}
