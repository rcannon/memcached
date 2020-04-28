#include <algorithm>
#include <functional>
#include <utility>
#include <cmath>
#include <cassert>
#include <chrono>
#include <random>
#include <mutex>
#include "cache.hh"

//using ctime_type = std::chrono::duration<double,std::chrono::milliseconds>;

class WorkloadGenerator {
  private:
    Cache cache_;
    const unsigned nsets_; 
    const unsigned ngets_;
    const unsigned ndels_;
    const unsigned num_warmups_;
    std::vector<key_type> keys_;
    std::vector<Cache::val_type> vals_;
    std::vector<Cache::size_type> sizes_;
    std::vector<std::string> requests_;
    std::random_device random_device_;
    const unsigned total_;
    std::mt19937 gen_;


    void fill_requests();

    void fill_vals_and_sizes();

    void fill_keys();

  public:
 
    WorkloadGenerator(unsigned nsets, unsigned ngets, unsigned ndels,
                      unsigned num_warmups, std::string host, std::string port);

    ~WorkloadGenerator();

    void WarmCache();

    // these functions allow limited extrnal access (read only) to private data
    key_type get_key(unsigned i) const;
    Cache::val_type get_val(unsigned i) const;
    std::string get_req(unsigned i) const;
    unsigned get_total() const;
    Cache::size_type get_size(unsigned i) const;
    unsigned get_req_size() const;
    unsigned get_num_warmups() const;
    unsigned get_ngets() const;

};
