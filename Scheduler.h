#pragma once

#include <iomanip>
#include <map>

#include "ctpl_stl.h"

#include "InterruptableSleep.h"
#include "Cron.h"

namespace Bosma {
  using Clock = std::chrono::system_clock;

  class Task {
  public:
    explicit Task(std::function<void()> &&f, bool recur = false, bool interval = false) :
        f(std::move(f)), recur(recur), interval(interval) {}

    virtual Clock::time_point get_new_time() const = 0;

    std::function<void()> f;

    bool recur;
    bool interval;
  };

  class InTask : public Task {
  public:
    explicit InTask(std::function<void()> &&f) : Task(std::move(f)) {}

    // dummy time_point because it's not used
    Clock::time_point get_new_time() const override { return Clock::time_point(Clock::duration(0)); }
  };

  class EveryTask : public Task {
  public:
    EveryTask(Clock::duration time, std::function<void()> &&f, bool interval = false) :
        Task(std::move(f), true, interval), time(time) {}

    Clock::time_point get_new_time() const override {
      return Clock::now() + time;
    };
    Clock::duration time;
  };

  class CronTask : public Task {
  public:
    CronTask(const std::string &expression, std::function<void()> &&f) : Task(std::move(f), true), cron(expression) {}

    Clock::time_point get_new_time() const override {
      return cron.cron_to_next();
    };
    Cron cron;
  };

  inline bool try_parse(std::tm &tm, const std::string& expression, const std::string& format) {
    std::stringstream ss(expression);
    return !(ss >> std::get_time(&tm, format.c_str())).fail();
  }

  class Scheduler {
  public:
    explicit Scheduler(unsigned int max_n_tasks = 4) : done(false), threads(max_n_tasks + 1) {
      threads.push([this](int) {
        while (!done) {
          if (tasks.empty()) {
            sleeper.sleep();
          } else {
            auto time_of_first_task = (*tasks.begin()).first;
            sleeper.sleep_until(time_of_first_task);
          }
          std::lock_guard<std::mutex> l(lock);
          manage_tasks();
        }
      });
    }

    Scheduler(const Scheduler &) = delete;

    Scheduler(Scheduler &&) noexcept = delete;

    Scheduler &operator=(const Scheduler &) = delete;

    Scheduler &operator=(Scheduler &&) noexcept = delete;

    ~Scheduler() {
      done = true;
      sleeper.interrupt();
	  threads.stop(true);
    }

    template<typename _Callable, typename... _Args>
    void in(const Clock::time_point time, _Callable &&f, _Args &&... args) {
      std::shared_ptr<Task> t = std::make_shared<InTask>(std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...));
      add_task(time, std::move(t));
    }

    template<typename _Callable, typename... _Args>
    void in(const Clock::duration time, _Callable &&f, _Args &&... args) {
      in(Clock::now() + time, std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template<typename _Callable, typename... _Args>
    void at(const std::string &time, _Callable &&f, _Args &&... args) {
      // get current time as a tm object
      auto time_now = Clock::to_time_t(Clock::now());
      std::tm tm = *std::localtime(&time_now);

      // our final time as a time_point
      Clock::time_point tp;

      if (try_parse(tm, time, "%H:%M:%S")) {
        // convert tm back to time_t, then to a time_point and assign to final
        tp = Clock::from_time_t(std::mktime(&tm));

        // if we've already passed this time, the user will mean next day, so add a day.
        if (Clock::now() >= tp)
          tp += std::chrono::hours(24);
      } else if (try_parse(tm, time, "%Y-%m-%d %H:%M:%S")) {
        tp = Clock::from_time_t(std::mktime(&tm));
      } else if (try_parse(tm, time, "%Y/%m/%d %H:%M:%S")) {
        tp = Clock::from_time_t(std::mktime(&tm));
      } else {
        // could not parse time
        throw std::runtime_error("Cannot parse time string: " + time);
      }

      in(tp, std::forward<_Callable>(f), std::forward<_Args>(args)...);
    }

    template<typename _Callable, typename... _Args>
    void every(const Clock::duration time, _Callable &&f, _Args &&... args) {
      std::shared_ptr<Task> t = std::make_shared<EveryTask>(time, std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...));
      auto next_time = t->get_new_time();
      add_task(next_time, std::move(t));
    }

// expression format:
// from https://en.wikipedia.org/wiki/Cron#Overview
//    ┌───────────── minute (0 - 59)
//    │ ┌───────────── hour (0 - 23)
//    │ │ ┌───────────── day of month (1 - 31)
//    │ │ │ ┌───────────── month (1 - 12)
//    │ │ │ │ ┌───────────── day of week (0 - 6) (Sunday to Saturday)
//    │ │ │ │ │
//    │ │ │ │ │
//    * * * * *
    template<typename _Callable, typename... _Args>
    void cron(const std::string &expression, _Callable &&f, _Args &&... args) {
      std::shared_ptr<Task> t = std::make_shared<CronTask>(expression, std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...));
      auto next_time = t->get_new_time();
      add_task(next_time, std::move(t));
    }

    template<typename _Callable, typename... _Args>
    void interval(const Clock::duration time, _Callable &&f, _Args &&... args) {
      std::shared_ptr<Task> t = std::make_shared<EveryTask>(time, std::bind(std::forward<_Callable>(f), std::forward<_Args>(args)...), true);
      add_task(Clock::now(), std::move(t));
    }

  private:
    std::atomic<bool> done;

    Bosma::InterruptableSleep sleeper;

    std::multimap<Clock::time_point, std::shared_ptr<Task>> tasks;
    std::mutex lock;
    ctpl::thread_pool threads;

    void add_task(const Clock::time_point time, std::shared_ptr<Task> t) {
      std::lock_guard<std::mutex> l(lock);
      tasks.emplace(time, std::move(t));
      sleeper.interrupt();
    }

    void manage_tasks() {
      auto end_of_tasks_to_run = tasks.upper_bound(Clock::now());

      // if there are any tasks to be run and removed
      if (end_of_tasks_to_run != tasks.begin()) {
        decltype(tasks) recurred_tasks;

        for (auto i = tasks.begin(); i != end_of_tasks_to_run; ++i) {

          auto &task = (*i).second;

          if (task->interval) {
            // if it's an interval task, add the task back after f() is completed
            threads.push([this, task](int) {
              task->f();
              add_task(task->get_new_time(), task);
            });
          }
          else {
            threads.push([task](int) {
              task->f();
            });
            // calculate time of next run and add the new task to the tasks to be recurred
            if (task->recur)
              recurred_tasks.emplace(task->get_new_time(), std::move(task));
          }
        }

        // remove the completed tasks
        tasks.erase(tasks.begin(), end_of_tasks_to_run);

        // re-add the tasks that are recurring
        for (auto &task : recurred_tasks)
          tasks.emplace(task.first, std::move(task.second));
      }
    }
  };
}