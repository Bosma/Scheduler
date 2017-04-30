#include <chrono>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

namespace Bosma {
  using Clock = std::chrono::system_clock;
  using namespace std::chrono_literals;
  
  inline void add(std::tm &tm, std::chrono::nanoseconds time) {
    auto tp = Clock::from_time_t(std::mktime(&tm));
    auto tm_adjusted = Clock::to_time_t(tp + time);
    tm = *std::localtime(&tm_adjusted);
  }

  class Cron {
  public:
    Cron(const std::string &expression) {
      std::istringstream iss(expression);
      std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                      std::istream_iterator<std::string>{}};
      minute = (tokens[0] == "*") ? -1 : std::stoi(tokens[0]);
      hour = (tokens[1] == "*") ? -1 : std::stoi(tokens[1]);
      day = (tokens[2] == "*") ? -1 : std::stoi(tokens[2]);
      month = (tokens[3] == "*") ? -1 : std::stoi(tokens[3]);
      day_of_week = (tokens[4] == "*") ? -1 : std::stoi(tokens[4]);
    }

    // http://stackoverflow.com/a/322058/1284550
    Clock::time_point cron_to_next() const {
      // get current time as a tm object
      auto now = Clock::to_time_t(Clock::now());
      std::tm next(*std::localtime(&now));
      // it will always at least run the next minute
      next.tm_sec = 0;
      add(next, std::chrono::minutes(1));
      while (true) {
        if (month != -1 && next.tm_mon != month) {
          // add a month
          // if this will bring us over a year, increment the year instead and reset the month
          if (next.tm_mon + 1 > 11) {
            next.tm_mon = 0;
            next.tm_year++;
          } else
            next.tm_mon++;

          next.tm_mday = 1;
          next.tm_hour = 0;
          next.tm_min = 0;
          continue;
        }
        if (day != -1 && next.tm_mday != day) {
          add(next, std::chrono::hours(24));
          next.tm_hour = 0;
          next.tm_min = 0;
          continue;
        }
        if (day_of_week != -1 && next.tm_wday != day_of_week) {
          add(next, std::chrono::hours(24));
          next.tm_wday++;
          next.tm_hour = 0;
          next.tm_min = 0;
          continue;
        }
        if (hour != -1 && next.tm_hour != hour) {
          add(next, std::chrono::hours(1));
          next.tm_min = 0;
          continue;
        }
        if (minute != -1 && next.tm_min != minute) {
          add(next, std::chrono::minutes(1));
          continue;
        }
        break;
      }

      return Clock::from_time_t(std::mktime(&next));
    }

    int minute, hour, day, month, day_of_week;
  };
}