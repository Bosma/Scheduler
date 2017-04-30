# Scheduler
Modern C++ Scheduling Library. Tasks run in thread pool. Requires [ctpl_stl.h](https://github.com/vit-vit/CTPL) in the path. 

Inspired by the [Rufus-Scheduler](https://github.com/jmettraux/rufus-scheduler) gem. Offers mostly the same functionality.

```C++
  // number of tasks that can run simultaneously
  // Note: not the number of tasks that can be added,
  //       but number of tasks that can be run in parallel
  unsigned int max_n_threads = 12;

  // Make a new scheduling object.
  // Note: s cannot be moved or copied
  Bosma::Scheduler s(max_n_threads);

  // every second call message("every second")
  s.every(1s, message, "every second");

  // in one minute
  s.in(1min, []() { std::cout << "in one minute" << std::endl; });

  s.every(1min, []() { std::cout << "every minute" << std::endl; });

  // https://en.wikipedia.org/wiki/Cron
  s.cron("* * * * *", []() { std::cout << "top of every minute" << std::endl; });

  // Time formats supported:
  // %Y/%m/%d %H:%M:%S, %Y-%m-%d %H:%M:%S, %H:%M:%S
  // With only a time given, it will run tomorrow if that time has already passed.
  // But with a date given, it will run immediately if that time has already passed.
  s.at("2017-04-19 12:31:15", []() { std::cout << "at a specific time." << std::endl; });

  s.cron("5 0 * * *", []() { std::cout << "every day 5 minutes after midnight" << std::endl; });
```
See [example.cpp](example.cpp) for a full example.
