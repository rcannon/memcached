#include "WorkloadGenerator.hh"
#include <iostream>

void doit(unsigned t)
{
  unsigned nsets = 29000;
  unsigned ndels = 1000;
  unsigned ngets = 70000;
  unsigned warmups = 50000;
  std::string server = "127.0.0.1";
  std::string port = "65413";
  unsigned nreq = 1000000;
  unsigned nthreads = t;
  std::cout << "THREADS: " << nthreads << std::endl;


  WorkloadGenerator wg(nsets, ngets, ndels, warmups, server, port);
  //wg.WarmCache();
  //auto hr = wg.get_hit_rate();
  //std::cout << "hit rate: " << hr << std::endl;
  wg.WarmCache();
  std::pair<double, double> res = wg.baseline_performance(nthreads, nreq);
  std::cout << "95 percentile: " << res.first << std::endl;
  std::cout << "mean throughput: " << res.second << std::endl;

}

int main()
{
  for (unsigned i = 1; i < 9; ++i)
  {
    doit(i);
  }
  return 0;
}
