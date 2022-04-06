#include "Signalable.hh"

#include <unordered_set>

using namespace std;


Signalable::Signalable() : signaled(false) {
  unique_lock g(Signalable::lock);
  Signalable::all_signalables.emplace(this);
}

Signalable::~Signalable() {
  unique_lock g(Signalable::lock);
  Signalable::all_signalables.erase(this);
}

bool Signalable::is_signaled() const {
  return this->signaled;
}

bool Signalable::signal_all() {
  shared_lock g(Signalable::lock);
  for (auto& s : all_signalables) {
    s->signaled = true;
  }
  return !all_signalables.empty();
}

shared_mutex Signalable::lock;
unordered_set<Signalable*> Signalable::all_signalables;
