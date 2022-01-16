#include "WorkloadGenerator.hh"
#include "cache.hh"
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <thread>

// declare global mutex so we do not need to pass
// it as an argument
std::mutex mutx;

static std::random_device rd;
static thread_local std::mt19937 gen(rd());

unsigned get_index(int max)
{
  std::geometric_distribution<int> dist(0.001);
  int idx = dist(gen)+1;
  idx = std::min(idx, max);
  assert((max - idx) >= 0);
  return (max - idx);
}


// hit rate is the number of successful get
// requests divided by the number of total 
// get requests
double get_hit_rate(WorkloadGenerator& wg, Cache& cache)
{
  unsigned total = wg.get_req_size();
  unsigned max = wg.get_total();
  unsigned get_val_index = 0;
  unsigned del_val_counter = 0;
  key_type key;
  Cache::size_type sz;
  Cache::val_type va;
  unsigned set_val_counter = wg.get_num_warmups();

  double hit_rate = 0;
  for (unsigned i = 0; i < total; ++i)
  {
    if ((i % 100000) == 0) std::cout << i << std::endl;
    if (wg.get_req(i) == "get")
    {
      get_val_index = get_index(set_val_counter);

      key = wg.get_key(get_val_index);
      sz = wg.get_size(get_val_index);
      auto x = cache.get(key, sz);
      if (x != nullptr)
      {
        std::string xp(x);
        if (xp == std::string(wg.get_val(get_val_index))) hit_rate = hit_rate + 1.;
        delete[] x;
      } 
    }
    else if (wg.get_req(i) == "set")
    {
      key = wg.get_key(set_val_counter);
      sz = wg.get_size(set_val_counter);
      va = wg.get_val(set_val_counter);
      cache.set(key, va , sz);
      set_val_counter = ((set_val_counter + 1) % max);
    }
    else
    {
      key = wg.get_key(del_val_counter);
      cache.del(key);
      del_val_counter = (del_val_counter+1) % max;
    }
  }
  hit_rate = hit_rate / wg.get_ngets();
  return hit_rate;
}


// helper function to get the time taken by a single 
// random request
double
get_bl(std::string request, unsigned get_val_counter, unsigned del_val_counter, unsigned set_val_counter, WorkloadGenerator& wg, Cache& cache)
{
  Cache::size_type sz;
  key_type ky;
  Cache::val_type vl;
  std::chrono::steady_clock::time_point start;
  std::chrono::steady_clock::time_point end;
  if (request == "set")
  { 
    ky = wg.get_key(set_val_counter-1);
    sz = wg.get_size(set_val_counter-1);
    vl = wg.get_val(set_val_counter-1);
    std::scoped_lock guard(mutx);
    start = std::chrono::steady_clock::now();
    cache.set(ky, vl, sz);
    end = std::chrono::steady_clock::now();
  }
  else if (request == "get")
  {
    unsigned idx = get_val_counter-1;
    ky = wg.get_key(idx);
    sz = wg.get_size(idx);
    //std::scoped_lock guard(mutx);
    //std::cout << "key: " << ky << ", size: " << sz << std::endl;
    start = std::chrono::steady_clock::now();
    auto x = cache.get(ky, sz);
    end = std::chrono::steady_clock::now();
    if (x != nullptr) delete[] x; // this is new memory that is allocated by cache_client when retruning a value, so it is safe to delete it after we have done comparisons.
  }
  else
  {
    ky = wg.get_key(del_val_counter-1);
    std::scoped_lock guard(mutx);
    start = std::chrono::steady_clock::now();
    cache.del(ky);
    end = std::chrono::steady_clock::now();
  }
  //const auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1000>>>(end - start).count();
  return time;
}

// construct a vector of latency times based on
// repeated calls to get_bl above
//
// note: instead of returning a vector, we modify a vector passed as reference,
// for the sake of speed
std::vector<double>
baseline_latencies(unsigned nreq, WorkloadGenerator& wg, Cache& cache)
{
  std::vector<double> res(nreq);
  const unsigned total = wg.get_total();
  unsigned get_val_index = 0;
  unsigned del_val_counter = 0;
  unsigned set_val_counter = wg.get_num_warmups();
  std::string rq;
  unsigned j;
  double time;
  for (unsigned i = 0; i < nreq; i++)
  {
    if ((i % 100000) == 0) std::cout << i << std::endl;
    j = i % total;
    rq = wg.get_req(j);
    if (rq == "get") get_val_index = get_index(set_val_counter);
    else if (rq == "set") set_val_counter++;
    else del_val_counter++;
    time = get_bl(rq, get_val_index, del_val_counter, set_val_counter, wg, cache); 
    res.at(i) = time;
  }
  return res;
}

// Here is our multithreaded benchmark :)
std::pair<double, double>
threaded_performance(unsigned nthreads, unsigned nreq, WorkloadGenerator& wg, std::string server, std::string port)
{
  unsigned runs = nreq / nthreads;
  std::vector<double> big_res;

  auto run_one_thread = [&]() 
  {
    Cache cache(server, port);
    std::vector<double> res = baseline_latencies(runs, wg, cache);
    std::scoped_lock guard(mutx);
    big_res.insert(big_res.end(), res.begin(), res.end());
  };

  std::vector<std::thread> threads;
  for (unsigned i = 0; i < nthreads; ++i) 
  {
    threads.push_back(std::thread(run_one_thread));
  }

  // get the time so we can calculate mean throughput
  const auto start = std::chrono::steady_clock::now();
  for (auto& t : threads) 
  {
    t.join();
  }
  const auto end = std::chrono::steady_clock::now();
  double time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1>>>(end - start).count();
  double mean_throughput = big_res.size() / time;
  
  // get the 95% percetile latency
  std::sort(big_res.begin(), big_res.end(), std::greater<double>());
  unsigned idx = std::round(big_res.size() * 0.05);
  double ninefive_percent = big_res.at(idx-1);
  
  return std::pair<double, double>(ninefive_percent, mean_throughput);
}

void doit(unsigned t)
{
  unsigned nsets = 290000;
  unsigned ndels = 10000;
  unsigned ngets = 700000;
  unsigned warmups = 50000;
  std::string server = "127.0.0.1";
  std::string port = "65413";
  unsigned nreq = 1000000;
  unsigned nthreads = t;
  std::cout << "THREADS: " << nthreads << std::endl;


  WorkloadGenerator wg(nsets, ngets, ndels, warmups, server, port);
  //wg.WarmCache();
  //auto hr = get_hit_rate(wg, Cache(server, port));
  //std::cout << "hit rate: " << hr << std::endl;
  wg.WarmCache();
  std::pair<double, double> res = threaded_performance(nthreads, nreq, wg, server, port);
  std::cout << "95 percentile: " << res.first << std::endl;
  std::cout << "mean throughput: " << res.second << std::endl;
}

int main()
{
  for (unsigned i = 2; i < 9; ++i)
  {
    doit(i);
  }
  return 0;
}
