#include "WorkloadGenerator.hh"
#include <iostream>

int main()
{
  unsigned nsets = 290000;
  unsigned ndels = 10000;
  unsigned ngets = 700000;
  unsigned warmups = 50000;
  std::string server = "127.0.0.1";
  std::string port = "65413";

  WorkloadGenerator wg(nsets, ngets, ndels, warmups, server, port);
  auto hr = wg.get_hit_rate();
  std::cout << "hit rate: " << hr << std::endl;
  std::pair<double, double> res = wg.baseline_performance(1000000);
  std::cout << "95 percentile: " << res.first << std::endl;
  std::cout << "mean throughput: " << res.second << std::endl;

  return 0;
}
