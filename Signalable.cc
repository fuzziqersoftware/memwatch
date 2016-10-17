#include "Signalable.hh"

#include <unordered_set>

#include <phosg/Concurrency.hh>

using namespace std;


Signalable::Signalable() : signaled(false) {
  rw_guard g(Signalable::lock, true);
  Signalable::all_signalables.emplace(this);
}

Signalable::~Signalable() {
  rw_guard g(Signalable::lock, true);
  Signalable::all_signalables.erase(this);
}

bool Signalable::is_signaled() const {
  return this->signaled;
}

bool Signalable::signal_all() {
  rw_guard g(Signalable::lock, false);
  for (auto& s : all_signalables) {
    s->signaled = true;
  }
  return !all_signalables.empty();
}

rw_lock Signalable::lock;
unordered_set<Signalable*> Signalable::all_signalables;
