// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#pragma once

#include <condition_variable>
#include <ctime>
#include <pthread.h>
#include "common/ceph_time.h"

namespace ceph {

template<class T>
class ceph_condition_variable {

  pthread_cond_t cond;
  T* waiter_mutex;

  ceph_condition_variable&
  operator=(const ceph_condition_variable&) = delete;
  ceph_condition_variable(const ceph_condition_variable&) = delete;

public:
  ceph_condition_variable();
  ~ceph_condition_variable();
  void wait(std::unique_lock<T>& lock);
  template<class Predicate>
  void wait(std::unique_lock<T>& lock, Predicate pred) {
    while (!pred()) {
      wait(lock);
    }
  }
  template<class Clock, class Duration>
  std::cv_status wait_until(
    std::unique_lock<T>& lock,
    const std::chrono::time_point<Clock, Duration>& when) {
    if constexpr (Clock::is_steady) {
      // convert from mono_clock to real_clock
      auto real_when = ceph::real_clock::now();
      const auto delta = when - Clock::now();
      real_when += std::chrono::ceil<typename Clock::duration>(delta);
      timespec ts = ceph::real_clock::to_timespec(real_when);
      return _wait_until(lock.mutex(), &ts);
    } else {
      timespec ts = Clock::to_timespec(when);
      return _wait_until(lock.mutex(), &ts);
    }
  }
  template<class Rep, class Period>
  std::cv_status wait_for(
    std::unique_lock<T>& lock,
    const std::chrono::duration<Rep, Period>& awhile) {
    ceph::real_time when{ceph::real_clock::now()};
    when += awhile;
    timespec ts = ceph::real_clock::to_timespec(when);
    return _wait_until(lock.mutex(), &ts);
  }
  template<class Rep, class Period, class Pred>
  bool wait_for(
    std::unique_lock<T>& lock,
    const std::chrono::duration<Rep, Period>& awhile,
    Pred pred) {
    ceph::real_time when{ceph::real_clock::now()};
    when += awhile;
    timespec ts = ceph::real_clock::to_timespec(when);
    while (!pred()) {
      if ( _wait_until(lock.mutex(), &ts) == std::cv_status::timeout) {
        return pred();
      }
    }
    return true;
  }
  void notify_one();
  void notify_all(bool sloppy = false);
private:
  std::cv_status _wait_until(T* mutex, timespec* ts);
};

} // namespace ceph
