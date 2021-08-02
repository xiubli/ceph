#include "condition_variable_debug.h"
#include "common/mutex_debug.h"
#include "common/fair_mutex.h"
#include "common/ceph_mutex.h"

namespace ceph {

template <class T>
condition_variable_debug<T>::condition_variable_debug()
  : waiter_mutex{nullptr}
{
  int r = pthread_cond_init(&cond, nullptr);
  if (r) {
    throw std::system_error(r, std::generic_category());
  }
}

template <class T>
condition_variable_debug<T>::~condition_variable_debug()
{
  pthread_cond_destroy(&cond);
}

template <class T>
void condition_variable_debug<T>::wait(std::unique_lock<T>& lock)
{
  // make sure this cond is used with one mutex only
  ceph_assert(waiter_mutex == nullptr ||
         waiter_mutex == lock.mutex());
  waiter_mutex = lock.mutex();
  ceph_assert(waiter_mutex->is_locked());
  waiter_mutex->_pre_unlock();
  if (int r = pthread_cond_wait(&cond, waiter_mutex->native_handle());
      r != 0) {
    throw std::system_error(r, std::generic_category());
  }
  waiter_mutex->_post_lock();
}

template <class T>
void condition_variable_debug<T>::notify_one()
{
  // make sure signaler is holding the waiter's lock.
  ceph_assert(waiter_mutex == nullptr ||
         waiter_mutex->is_locked());
  if (int r = pthread_cond_signal(&cond); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
}

template <class T>
void condition_variable_debug<T>::notify_all(bool sloppy)
{
  if (!sloppy) {
    // make sure signaler is holding the waiter's lock.
    ceph_assert(waiter_mutex == NULL ||
                waiter_mutex->is_locked());
  }
  if (int r = pthread_cond_broadcast(&cond); r != 0 && !sloppy) {
    throw std::system_error(r, std::generic_category());
  }
}

template <class T>
std::cv_status condition_variable_debug<T>::_wait_until(T* mutex,
                                                        timespec* ts)
{
  // make sure this cond is used with one mutex only
  ceph_assert(waiter_mutex == nullptr ||
         waiter_mutex == mutex);
  waiter_mutex = mutex;
  ceph_assert(waiter_mutex->is_locked());

  waiter_mutex->_pre_unlock();
  int r = pthread_cond_timedwait(&cond, waiter_mutex->native_handle(), ts);
  waiter_mutex->_post_lock();
  switch (r) {
  case 0:
    return std::cv_status::no_timeout;
  case ETIMEDOUT:
    return std::cv_status::timeout;
  default:
    throw std::system_error(r, std::generic_category());
  }
}

} // namespace ceph

template class ceph_condition_variable<ceph::fair_mutex>;
template class ceph_condition_variable<ceph::mutex>;
