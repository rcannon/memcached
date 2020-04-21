#include <algorithm>
#include <functional>
#include <utility>
#include <cmath>
#include <cassert>
#include <chrono>
#include <random>
#include "cache.hh"

using ctime_type = std::chrono::duration<double,std::chrono::milliseconds>;

class WorkloadGenerator {
  private:
    Cache cache_;
    unsigned nsets_; 
    unsigned ngets_;
    unsigned ndels_;
    unsigned num_warmups_;
    std::vector<key_type> keys_;
    std::vector<Cache::val_type> vals_;
    std::vector<Cache::size_type> sizes_;
    std::vector<std::string> requests_;
    //std::vector<unsigned> get_repeats_;
    std::random_device random_device_;
    std::mt19937 gen_;

    void fill_requests();

    void fill_vals_and_sizes();

    void fill_keys();

    double get_bl(std::string request, unsigned get_val_counter, 
                  unsigned del_val_counter, unsigned set_val_counter);

    void WarmCache();

    unsigned get_index(int max);

  public:

    // run the setup of the vectors based on the inputs and specifications in notes.txt
    WorkloadGenerator(unsigned nsets, unsigned ngets, unsigned ndels,
                      unsigned num_warmups, std::string host, std::string port);

    ~WorkloadGenerator(); // need to delete the vals in vals_


    double get_hit_rate(); // returns the hit rate of the cache

    std::vector<double> baseline_latencies(unsigned nreq);

    std::pair<double, double> baseline_performance(unsigned nreq);
};
