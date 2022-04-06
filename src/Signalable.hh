#pragma once

#include <unordered_set>
#include <shared_mutex>



class Signalable {
public:
  Signalable();
  ~Signalable();

  bool is_signaled() const;
  static bool signal_all();

private:
  bool signaled;

  static std::shared_mutex lock;
  static std::unordered_set<Signalable*> all_signalables;
};
