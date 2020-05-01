CSCI 389 HW6

Authors: Reilly Cannon (reicannon@reed.edu) and James McCaull (jamccaull@reed.edu)

Note: For each of the following tests, we ran the server with a maximum memory of 500,000 bytes, on local host, with no evictor (to minimize latency), with a load factor of 0.75. We before each test, we warmed the cache with 50,000 set requests, each with expected size equal to 10 bytes (so the expect memory remaining in the cache when we start testing is 0 bytes). The system on which we are running both the server and the client has 4 physical CPU cores and 8 hyperthreaded cores. We reset the cache after each test and warm it again with the same keys and values for each number of client threads (the keys and values between test on different number of server threads may differ.

# Part 1 - Multithreaded Benchmark

All of the listed functions can be found in the bechmark.cc file. The results of running the server with one thread and varying the number of client threads is shown on the two plots below. The exact results can be seen in part1_results.csv.

![ThruVsThreads](/Plots/MeanThrPlot.png "MeanThroughput")

![95thVsThreads](/Plots/95thPlot.png "95thLatency")

We can identify our saturation point as being at 1-3 client threads, with our maximum unsaturated mean throughput of 964 requests per second (when client threads = 2) and minimum unsaturated 95th percentile latency of 1.94 milliseconds (when client threads = 2). It makes sense that the mean throughput stays constant for a low number of client threads and decreases when the number of client threads grows too much, because there is only a single server thread servicing all of the client threads. That single server thread can't move faster with three client threads than with one, and with too many client threads it cannot keep up with all of the incoming requests.

# Part 2 - Multithreaded Server

Note: For these test, we tried our best to multithread the server, but judging from the cpu usage of our server, we were not very successful at achieving concurrent access to the cache for get requests (Eitan said that the server alone should achieve close to 200% cpu utilization, we were not able to even get it close to 100%). Some performance improvement is shown for 2 server threads. It is possible that this is casued by the server being able to utilize the multithreading during other parts of the computation, just not the cache accessing part. Using gdb we confirmed that the server was utilizing multiple threads; we just couldn't get performance or the cpu utilization up to the levels that we had hoped.

Also, we realized too late that we neglected to test an instance where the number of client threads is 1. We only did 2 through 8. 

In this section we run the same test as above, varying client threads from 2 to 8. We report the results for three different values of server threads: 2,3, and 4. We only test lower numbers of server threads because we are running both the server and the client on the same system, so they are competing for CPU resources. The plot for 2 server threads can be seen below. The exact results can be seen in part2_results.csv.

First the results from running the server with two threads.

![2ThruVsThreads](/Plots/2MeanThrPlot.png "2MeanThroughput")

![95thVsThreads2](/Plots/95thPlot2.png "95thLatency2")

We can see from these plots that the saturation point for two server threads is when there are four client threads sending requests. The maximum unsaturated mean throughput is 1347 requests per second (when client threads = 4), and the minimum unsaturated 95th percentile latency is 3.33 milliseconds (client threads = 2). This is a clear improvement over the single threaded server, and improvement in throughput of 40% (from basseline 963 requests per second).

We next ran the tests with 3 server threads. The results can be seen in the plots below.

![3ThruVsThreads](/Plots/4MeanThrPlot.png "2MeanThroughput")

![95thVsThreads3](/Plots/95thPlot4.png "95thLatency2")

These plots tell us that the server become saturated immediately, with a single client thread giving the only unsatuated throughput. The maximum unsaturated mean throughput is 972 requests per second (when client threads = 2), and the minimum unsaturated 95th percentile latency is 3.33 milliseconds (client threads = 2). This is on par the baseline results in terms of throughput but worse in terms of 95th-latency. It is alsoworse than our results from 2 server threads. The fact that the server first become saturaed at 2 client threads suggests that the 2 client threads and 3 server threads are competing for cpu resources (there are only 4 physical cores).

Finally, we ran the tests with 4 server threads. The results can be seen in the plots below.

![4ThruVsThreads](/Plots/4MeanThrPlot.png "2MeanThroughput")

![95thVsThreads4](/Plots/95thPlot4.png "95thLatency2")

We can see from these plots that the saturation point for two server threads is when there are four client threads sending requests. The maximum unsaturated mean throughput is 852 requests per second (when client threads = 4), and the minimum unsaturated 95th percentile latency is 3.66 milliseconds (client threads = 2). This is clearly worse than the baseline and our results for 2 and 3 server threads. The low performance suggests that the client and server threads are competing too much for resourses (like the cache) for the computation to be done quickly, despite the throughput having an upward trend cient threads = 2,3,4.

# Extra Credit

Here also, we realized too late that we neglected to test an instance where the number of client threads is 1. We only did 2 through 8. 

Despite our inability to full multithread the server, we attempt to enhance it by modifying the cache that our server is running. The results are shown below. The exact results can be seen in EC_results.csv. We begin with our results for two server threads.

![ec2ThruVsThreads](/ExtraCreditPlots/EC_MeanPlot2.png "ec2MeanThroughput")

![ec95thVsThreads2](/ExtraCreditPlots/EC_95thPlot2.png "ec95thLatency2")

We can see from these plots that the saturation point for two server threads is when there are three client threads sending requests. The maximum unsaturated mean throughput is 1226 requests per second (when client threads = 3), and the minimum unsaturated 95th percentile latency is 2.53 milliseconds (client threads = 2). This is a clear improvement over the single threaded server, but interestingly the performance is worse than our results in part 2. The overall slower throughput is possibly due to the inconsistency with which my laptop is plugged in to power, which could be throttling cpu performance.

We next test 3 server threads. 

![ec2ThruVsThreads](/ExtraCreditPlots/EC_MeanPlot3.png "ec3MeanThroughput")

![ec95thVsThreads2](/ExtraCreditPlots/EC_95thPlot3.png "ec95thLatency3")

We can see from these plots that the saturation point for three server threads is when there is 1 client threads sending requests. The maximum unsaturated mean throughput is 906 requests per second (when client threads = 2), and the minimum unsaturated 95th percentile latency is 2.61 milliseconds (client threads = 2). This is worse than our baseline results in terms of thoughput as well as latency. This is also worse than our results before moving to finer grained locking. Perhaps our "enhancement" didn't actually make things better. 

Finally, we test 4 server threads.

![ec2ThruVsThreads](/ExtraCreditPlots/EC_MeanPlot4.png "ec4MeanThroughput")

![ec95thVsThreads2](/ExtraCreditPlots/EC_95thPlot4.png "ec95thLatency4")

We can see from these plots that the saturation point for four server threads is when there are 3 client threads sending requests. The maximum unsaturated mean throughput is 863 requests per second (when client threads = 3), and the minimum unsaturated 95th percentile latency is 2.7 milliseconds (client threads = 2). This is significantly worse than our baseline result, but ever so slightly better than the result in the last test of part 2, in terms of throughput. It is significantly better than the last test in part two with regard to latency. 

It is interesting that the server reaches saturation with these modifications to the cache than with just the course grained multithreading of the server, as well as having lower performance in all but the last test. It seems plausible that the lower overall performance could be due to the fact that, with finer graining, there are more scoped_lock guards that need to be initialized and destructed (as opposed to just one in the cache_server, resulting in more overall overhead and lowering performance.  

Overall our best result was with an unmodified cache_lib, 2 server threads, and 2 client threads.
