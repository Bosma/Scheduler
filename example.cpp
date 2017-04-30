#include <iostream>

#include "Scheduler.h"

using namespace std::chrono_literals;

void message(const std::string &s) {
  std::cout << s << std::endl;
}

int main() {
  // number of tasks that can run simultaneously
  // Note: not the number of tasks that can be added,
  //       but number of tasks that can be run in parallel
  unsigned int max_n_threads = 12;

  // cannot be moved or copied
  Bosma::Scheduler s(max_n_threads);

  s.every(1s, message, "every second");

  s.in(1min, []() { std::cout << "in one minute" << std::endl; });

  s.every(1min, []() { std::cout << "every minute" << std::endl; });

  // https://en.wikipedia.org/wiki/Cron
  s.cron("* * * * *", []() { std::cout << "top of every minute" << std::endl; });

  s.at("2017-04-19 12:31:15", []() { std::cout << "at a specific time." << std::endl; });

  s.cron("5 0 * * *", []() { std::cout << "every day 5 minutes after midnight" << std::endl; });

  // destructor of Bosma::Scheduler will cancel all schedules but finish any tasks currently running
  std::this_thread::sleep_for(30min);
}