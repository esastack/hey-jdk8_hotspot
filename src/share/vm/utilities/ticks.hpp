/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_UTILITIES_TICKS_HPP
#define SHARE_VM_UTILITIES_TICKS_HPP

#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"

class TimeInstant {
 protected:
  jlong _instant;
 public:
  TimeInstant(jlong stamp = 0) : _instant(stamp) {}
  TimeInstant& operator+=(const TimeInstant& rhs) {
    _instant += rhs.value();
    return *this;
  }
  TimeInstant& operator-=(const TimeInstant& rhs) {
    _instant -= rhs.value();
    return *this;
  }
  bool operator==(const TimeInstant& rhs) const {
    return _instant == rhs.value();
  }
  bool operator!=(const TimeInstant& rhs) const {
    return _instant != rhs.value();
  }
  bool operator>(const TimeInstant& rhs) const {
    return _instant > rhs.value();
  }
  bool operator<(const TimeInstant& rhs) const {
    return _instant < rhs.value();
  }
  bool operator>=(const TimeInstant& rhs) const {
    return _instant >= rhs.value();
  }
  bool operator<=(const TimeInstant& rhs) const {
    return _instant <= rhs.value();
  }
  operator jlong() const { return _instant; }

  jlong value() const { return _instant; }
};

template <typename InstantType>
class TimeInterval {
 private:
  InstantType _interval;
 public:
  TimeInterval(jlong interval = 0) : _interval(interval) {}
  TimeInterval(const InstantType& end, const InstantType& start) : _interval(end - start) {}
  const InstantType& get() const { return _interval; }
  
  InstantType now() { return InstantType::now(); }
  double seconds() { return InstantType::seconds(_interval); }
  uint64_t milliseconds() { return InstantType::milliseconds(_interval); }
  uint64_t microseconds() { return InstantType::microseconds(_interval); }
  uint64_t nanoseconds() { return InstantType::nanoseconds(_interval); }
  jlong value() const { return _interval; }
  
  TimeInterval<InstantType>& operator+=(const TimeInterval<InstantType>& rhs) {
    _interval += rhs.get();
    return *this;
  }
  TimeInterval<InstantType>& operator-=(const TimeInterval<InstantType>& rhs) {
    _interval -= rhs.get();
    return *this;
  }
};

class ElapsedCounter : public TimeInstant {
 public:
  ElapsedCounter(jlong stamp = 0) : TimeInstant(stamp) {}
  ElapsedCounter& operator+=(const ElapsedCounter& rhs) {
    _instant += rhs.value();
    return *this;
  }
  ElapsedCounter& operator-=(const ElapsedCounter& rhs) {
    _instant -= rhs.value();
    return *this;
  }
  void stamp();
  
  static ElapsedCounter now();
  static uint64_t frequency();
  static double seconds(const ElapsedCounter& value);
  static uint64_t milliseconds(const ElapsedCounter& value);
  static uint64_t microseconds(const ElapsedCounter& value);
  static uint64_t nanoseconds(const ElapsedCounter& value);
};

class ElapsedCounterStamped : public ElapsedCounter {
 public:
  ElapsedCounterStamped();
};

class FastElapsedCounter : public TimeInstant {
 public:
  FastElapsedCounter(jlong stamp = 0) : TimeInstant(stamp) {}
  FastElapsedCounter& operator+=(const FastElapsedCounter& rhs) {
    _instant += rhs.value();
    return *this;
  }
  FastElapsedCounter& operator-=(const FastElapsedCounter& rhs) {
    _instant -= rhs.value();
    return *this;
  }
  void stamp();
  
  static FastElapsedCounter now();
  static uint64_t frequency();
  static double seconds(const FastElapsedCounter& value);
  static uint64_t milliseconds(const FastElapsedCounter& value);
  static uint64_t microseconds(const FastElapsedCounter& value);
  static uint64_t nanoseconds(const FastElapsedCounter& value);
};

class FastElapsedCounterStamped : public FastElapsedCounter {
 public:
  FastElapsedCounterStamped();
};

typedef TimeInterval<ElapsedCounter> ElapsedCounterInterval;
typedef TimeInterval<FastElapsedCounter> FastElapsedCounterInterval;

class Ticks;

class Tickspan {
  friend Tickspan operator-(const Ticks& end, const Ticks& start);
 private:
  ElapsedCounterInterval _elapsed_interval;
  FastElapsedCounterInterval _ft_elapsed_interval;
  Tickspan(const Ticks& end, const Ticks& start);
 public:
  Tickspan(jlong interval = 0);
  Tickspan& operator+=(const Tickspan& rhs);
  Tickspan& operator-=(const Tickspan& rhs);

  jlong value() const { return _elapsed_interval.value(); }
  jlong ft_value() const;
};

class Ticks {
 protected:
  ElapsedCounter _elapsed;
  X86_ONLY(FastElapsedCounter _ft_elapsed;)
 public:
  Ticks(jlong stamp = 0);
  Ticks& operator+=(const Tickspan& rhs);
  Ticks& operator-=(const Tickspan& rhs);

  static Ticks now();
  void stamp();
  jlong value() const { return _elapsed.value(); }
  jlong ft_value() const;
};

class TicksStamped : public Ticks {
 public:
  TicksStamped();
};

#endif // SHARE_VM_UTILITIES_TICKS_HPP
