// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-

#pragma once

#include "common/ceph_mutex.h"

#include <pthread.h>
#include <thread>
#include <string>
#include <mutex>

namespace ceph {
/// a FIFO mutex
class fair_mutex {
public:
  fair_mutex(const std::string& name) {}
  ~fair_mutex() = default;
  fair_mutex(const fair_mutex&) = delete;
  fair_mutex& operator=(const fair_mutex&) = delete;

  pthread_mutex_t* native_handle() {
    return mutex.native_handle();
  }

  pthread_cond_t* native_cond_handle() {
    return cond.native_handle();
  }

  void _pre_unlock() {
    mutex.lock();
    ceph_assert(locked_by == std::this_thread::get_id());
    ++unblock_id;
    locked_by = std::thread::id();
    // wake up all the waiters before giving up the lock
    // in ceph_condition_variable's wait()
    int r = pthread_cond_broadcast(cond.native_handle());
    ceph_assert(r == 0);
  }

  void _post_lock() {
    const unsigned my_id = next_id++;
    while (my_id != unblock_id) {
      pthread_cond_wait(cond.native_handle(), mutex.native_handle());
    }
    locked_by = std::this_thread::get_id();
    mutex.unlock();
  }

  void lock()
  {
    std::unique_lock lock(mutex);
    const unsigned my_id = next_id++;
    cond.wait(lock, [&] {
      return my_id == unblock_id;
    });
    locked_by = std::this_thread::get_id();
  }

  bool try_lock()
  {
    std::lock_guard lock(mutex);
    if (is_locked()) {
      return false;
    }
    ++next_id;
    locked_by = std::this_thread::get_id();
    return true;
  }

  void unlock()
  {
    std::lock_guard lock(mutex);
    ++unblock_id;
    locked_by = std::thread::id();
    cond.notify_all();
  }

  bool is_locked() const
  {
    return next_id != unblock_id;
  }

  bool is_locked_by_me() const {
    return is_locked() && locked_by == std::this_thread::get_id();
  }

private:
  unsigned next_id = 0;
  unsigned unblock_id = 0;
  std::condition_variable cond;
  std::mutex mutex;
  std::thread::id locked_by = {};
};
} // namespace ceph

extern template class ceph::ceph_condition_variable<ceph::fair_mutex>;
