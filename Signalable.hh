#pragma once

#include <unordered_set>

#include <phosg/Concurrency.hh>

class Signalable {
public:
  Signalable();
  ~Signalable();

  bool is_signaled() const;
  static bool signal_all();

private:
  bool signaled;

  static rw_lock lock;
  static std::unordered_set<Signalable*> all_signalables;
};
