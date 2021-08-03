// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-

#include <array>
#include <mutex>
#include <numeric>
#include <thread>
#include <gtest/gtest.h>
#include <condition_variable>

#include "common/ceph_mutex.h"
#include "common/condition_variable_debug.h"

TEST(FairMutex, simple)
{
  ceph::fair_mutex mutex{"fair::simple"};
  {
    std::unique_lock lock{mutex};
    ASSERT_TRUE(mutex.is_locked());
    // fair_mutex does not recursive ownership semantics
    ASSERT_FALSE(mutex.try_lock());
  }
  // re-acquire the lock
  {
    std::unique_lock lock{mutex};
    ASSERT_TRUE(mutex.is_locked());
  }
  ASSERT_FALSE(mutex.is_locked());
}

TEST(FairMutex, fair)
{
  // waiters are queued in FIFO order, and they are woken up in the same order
  // we have a marathon participated by multiple teams:
  // - each team is represented by a thread.
  // - each team should have equal chance of being selected and scoring, assuming
  //   the runners in each team are distributed evenly in the waiting queue.
  ceph::fair_mutex mutex{"fair::fair"};
  const int NR_TEAMS = 2;
  std::array<unsigned, NR_TEAMS> scoreboard{0, 0};
  const int NR_ROUNDS = 256;
  auto play = [&](int team) {
    for (int i = 0; i < NR_ROUNDS; i++) {
      std::unique_lock lock{mutex};
      // pretent that i am running.. and it takes time
      std::this_thread::sleep_for(std::chrono::microseconds(20));
      // score!
      scoreboard[team]++;
      // fair?
      unsigned total = std::accumulate(scoreboard.begin(),
                                       scoreboard.end(),
                                       0);
      for (unsigned score : scoreboard) {
        if (total < NR_ROUNDS) {
          // not quite statistically significant. to reduce the false positive,
          // just consider it fair
          continue;
        }
        // check if any team is donimating the game.
        unsigned avg = total / scoreboard.size();
        // leave at least half of the average to other teams
        ASSERT_LE(score, total - avg / 2);
        // don't treat myself too bad
        ASSERT_GT(score, avg / 2);
      };
    }
  };
  std::array<std::thread, NR_TEAMS> teams;
  for (int team = 0; team < NR_TEAMS; team++) {
    teams[team] = std::thread(play, team);
  }
  for (auto& team : teams) {
    team.join();
  }
}

TEST(FairMutex, faircond)
{
  int NR = 1000;
  int counter = 0;
  ceph::fair_mutex mutex{"fair::fair"};
#ifdef CEPH_DEBUG_MUTEX
  ceph::condition_variable_debug<ceph::fair_mutex> cond;
#else
  std::condition_variable_any cond;
#endif
  auto threadA = [&]() {
    while (1) {
      std::unique_lock lock{mutex};
      if (counter >= NR)
        break;
      cond.wait(lock, [&] {
        return counter && (counter % 3 == 0 || counter >= NR);
      });
    }
  };
  auto threadB = [&]() {
    while (1) {
      std::unique_lock lock{mutex};
      if (counter >= NR)
        break;
      cond.wait(lock, [&] {
        return counter && (counter % 7 == 0 || counter >= NR);
      });
    }
  };
  auto threadC = [&]() {
    while (1) {
      std::lock_guard lock{mutex};
      if (++counter >= NR)
        break;
      cond.notify_one();
    }
  };

  std::thread tA = std::thread(threadA);
  std::thread tB = std::thread(threadB);
  std::thread tC = std::thread(threadC);
  tA.join();
  tB.join();
  tC.join();
}
