#include <chrono>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>
#include <algorithm>

namespace Bosma {
    using Clock = std::chrono::system_clock;

    inline void add(std::tm &tm, Clock::duration time) {
      auto tp = Clock::from_time_t(std::mktime(&tm));
      auto tp_adjusted = tp + time;
      auto tm_adjusted = Clock::to_time_t(tp_adjusted);
      tm = *std::localtime(&tm_adjusted);
    }

    class BadCronExpression : public std::exception {
    public:
        explicit BadCronExpression(std::string msg) : msg_(std::move(msg)) {}

        const char *what() const noexcept override { return (msg_.c_str()); }

    private:
        std::string msg_;
    };

    inline void
    verify_and_set(const std::string &token, const std::string &expression, int field[], int size, const int lower_bound,
                   const int upper_bound, const bool adjust = false) {
    if (token != "*"){
      try {
        int i = 0;
        std::istringstream iss(token);
        std::string word;
        while (getline(iss, word, ','))
        {
          if (i >= size)
            throw std::out_of_range("Invalid setsize param");
          field[i] = std::stoi(word);
          if (adjust) field[i]--;
          if (field[i] < lower_bound || field[i] > upper_bound)
          {
            std::ostringstream oss;
            oss << "malformed cron string ('" << token << "' must be <= " << upper_bound << " and >= " << lower_bound
                << "): " << expression;
            throw BadCronExpression(oss.str());
          }
          i++;
        }
      }
      catch (const std::invalid_argument &)
      {
        throw BadCronExpression("malformed cron string (`" + token + "` not an integer or *): " + expression);
      }
      catch (const std::out_of_range &)
      {
        throw BadCronExpression("malformed cron string (`" + token + "` not convertable to int): " + expression);
      }
    }
  }

    class Cron {
    public:
        explicit Cron(const std::string &expression) {
          std::fill_n(minute, 60, -1);
          std::fill_n(hour,   24, -1);
          std::fill_n(day,    31, -1);
          std::fill_n(month,  12, -1);
          std::fill_n(day_of_week, 7, -1);
          std::istringstream iss(expression);
          std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                          std::istream_iterator<std::string>{}};

          if (tokens.size() != 5) throw BadCronExpression("malformed cron string (must be 5 fields): " + expression);

          verify_and_set(tokens[0], expression, minute, 60, 0, 59);
          verify_and_set(tokens[1], expression, hour, 24, 0, 23);
          verify_and_set(tokens[2], expression, day, 31, 1, 31);
          verify_and_set(tokens[3], expression, month, 12, 1, 12, true);
          verify_and_set(tokens[4], expression, day_of_week, 7, 0, 6);
        }

        // http://stackoverflow.com/a/322058/1284550
        Clock::time_point cron_to_next(const Clock::time_point from = Clock::now()) const {
          // get current time as a tm object
          auto now = Clock::to_time_t(from);
          std::tm next(*std::localtime(&now));
          // it will always at least run the next minute
          next.tm_sec = 0;
          add(next, std::chrono::minutes(1));
          while (true) {
            if (std::count(month, month+12, -1) < 12 && std::find(month, month+12, next.tm_mon) - month >= 12){
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
            if (std::count(day, day+31, -1) < 31 && std::find(day, day+31, next.tm_mday) - day >= 31){
              add(next, std::chrono::hours(24));
              next.tm_hour = 0;
              next.tm_min = 0;
              continue;
            }
            if (std::count(day_of_week, day_of_week+7, -1) < 7 && std::find(day_of_week, day_of_week+7, next.tm_wday) - day_of_week >= 7) {
              add(next, std::chrono::hours(24));
              next.tm_hour = 0;
              next.tm_min = 0;
              continue;
            }
            if (std::count(hour, hour + 24, -1) < 24 && std::find(hour, hour+24, next.tm_hour) - hour >= 24) {
              add(next, std::chrono::hours(1));
              next.tm_min = 0;
              continue;
            }
            if (std::count(minute, minute+60, -1) < 60 && std::find(minute, minute+60, next.tm_min) - minute >= 60){
              add(next, std::chrono::minutes(1));
              continue;
            }
            break;
          }

          // telling mktime to figure out dst
          next.tm_isdst = -1;
          return Clock::from_time_t(std::mktime(&next));
        }

        int minute[60], hour[24], day[31], month[12], day_of_week[7];
    };
}
