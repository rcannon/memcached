#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
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
    gen_(std::mt19937(random_device_()))
{
  // easy condition check to simplify things
  assert(num_warmups_ < ngets_ + ndels_ + nsets_);
  fill_requests();
  fill_vals_and_sizes();
  fill_keys();
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
  std::geometric_distribution<int> length_dist(0.1); // probably change distributions later
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

// performance metrics are more representitive
// if the cache is already warm
void WorkloadGenerator::WarmCache()
{
  cache_.reset();
  for (unsigned i = 0; i < num_warmups_; i++)
  {
    cache_.set(keys_.at(i), vals_.at(i), sizes_.at(i));
  }
}

// get a random index for access during a "get"
// request. More recently added keys are more 
// likely to be chosen, to mimic temporal locality
// of actual workload
unsigned WorkloadGenerator::get_index(int max)
{
  std::geometric_distribution<int> dist(0.001);
  int idx = dist(gen_)+1;
  idx = std::min(idx, max);
  assert((max - idx) >= 0);
  return (max - idx);
}


// hit rate is the number of successful get
// requests divided by the number of total 
// get requests
double WorkloadGenerator::get_hit_rate()
{
  WarmCache();
  unsigned total = nsets_ + ngets_ + ndels_;
  unsigned max = keys_.size();
  unsigned get_val_index = 0;
  unsigned del_val_counter = 0;
  key_type key;
  Cache::size_type sz;
  Cache::val_type va;
  unsigned set_val_counter = num_warmups_;

  double hit_rate = 0;
  assert(requests_.size() == total);
  for (unsigned i = 0; i < total; ++i)
  {
    if ((i % 100000) == 0) std::cout << i << std::endl;
    if (requests_.at(i) == "get")
    {
      get_val_index = get_index(set_val_counter);

      key = keys_.at(get_val_index);
      sz = sizes_.at(get_val_index);
      auto x = cache_.get(key, sz);
      if (x != nullptr)
      {
        std::string xp(x);
        if (xp == std::string(vals_.at(get_val_index))) hit_rate = hit_rate + 1.;
        delete[] x;
      } 
    }
    else if (requests_.at(i) == "set")
    {
      key = keys_.at(set_val_counter);
      sz = sizes_.at(set_val_counter);
      va = vals_.at(set_val_counter);
      cache_.set(key, va , sz);
      set_val_counter = ((set_val_counter + 1) % max);
    }
    else
    {
      key = keys_.at(del_val_counter);
      cache_.del(key);
      del_val_counter = (del_val_counter+1) % max;
    }
  }
  hit_rate = hit_rate / ngets_;
  return hit_rate;
}

// helper function to get the time taken by a single 
// random request
double
WorkloadGenerator::get_bl(std::string request, unsigned get_val_counter, unsigned del_val_counter, unsigned set_val_counter)
{
  unsigned max = nsets_ + num_warmups_;
  const auto start = std::chrono::steady_clock::now();
  if (request == "set")
  { 
    cache_.set(keys_.at((set_val_counter-1) % max), vals_.at((set_val_counter-1) % max), sizes_.at((set_val_counter-1) % max));
  }
  else if (request == "get")
  {
    auto x = cache_.get(keys_.at((get_val_counter-1) % max), sizes_.at((get_val_counter-1) % max));
    if (x != nullptr) delete[] x;
  }
  else
  {
    cache_.del(keys_.at((del_val_counter-1) % max));
  }
  const auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1000>>>(end - start).count();
  return time;
}

// construct a vector of latency times based on
// repeated calls to get_bl above
std::vector<double> 
WorkloadGenerator::baseline_latencies(unsigned nreq)
{
  WarmCache();
  std::vector<double> res(nreq);

  unsigned total = nsets_ + num_warmups_;
  unsigned get_val_index = 0;
  unsigned del_val_counter = 0;
  unsigned set_val_counter = num_warmups_;
  std::string rq;
  unsigned j;
  double time;
  for (unsigned i = 0; i < nreq; i++)
  {
    j = i % total;
    rq = requests_.at(j);
    if (rq == "get") get_val_index = get_index(set_val_counter);
    else if (rq == "set") set_val_counter++;
    else del_val_counter++;
    time = get_bl(rq, get_val_index, del_val_counter, set_val_counter);
    res[i] = time;
  }
  return res;
}

// get the performance statistics based on latency vector returned above
std::pair<double, double> WorkloadGenerator::baseline_performance(unsigned nreq)
{
  auto res = baseline_latencies(nreq);
  std::sort(res.begin(), res.end(), std::greater<double>());

  unsigned idx = std::round(res.size() * 0.05);
  double ninefive_percent = res.at(idx-1);

  double mean_throughput = res.size();
  auto denom = std::accumulate(res.begin(), res.end(), 0.);
  
  mean_throughput = (mean_throughput / denom) * 1000 ;

  return std::pair<double, double>(ninefive_percent, mean_throughput);
}
