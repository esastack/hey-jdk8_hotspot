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

#include "precompiled.hpp"
#include "runtime/os.hpp"
#include "utilities/ticks.inline.hpp"

#ifdef X86
#include "rdtsc_x86.hpp"
#endif

#ifdef TARGET_OS_ARCH_linux_x86
# include "os_linux_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_sparc
# include "os_linux_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_zero
# include "os_linux_zero.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_x86
# include "os_solaris_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_sparc
# include "os_solaris_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_x86
# include "os_windows_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_arm
# include "os_linux_arm.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_ppc
# include "os_linux_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_aix_ppc
# include "os_aix_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_x86
# include "os_bsd_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_zero
# include "os_bsd_zero.hpp"
#endif

template <typename TimeSource, const int unit>
inline double conversion(const TimeSource& value) {
  return (double)value * ((double)unit / (double)TimeSource::frequency());
}

uint64_t ElapsedCounter::frequency() {
  static const uint64_t freq = (uint64_t)os::elapsed_frequency();
  return freq;
}

ElapsedCounterStamped::ElapsedCounterStamped() : ElapsedCounter(os::elapsed_counter()) {}

void ElapsedCounter::stamp() {
  _instant = now().value();
}

ElapsedCounter ElapsedCounter::now() {
  return ElapsedCounterStamped();
}

double ElapsedCounter::seconds(const ElapsedCounter& value) {
  return conversion<ElapsedCounter, 1>(value);
}
uint64_t ElapsedCounter::milliseconds(const ElapsedCounter& value) {
  return (uint64_t)conversion<ElapsedCounter, MILLIUNITS>(value);
}

uint64_t ElapsedCounter::microseconds(const ElapsedCounter& value) {
  return (uint64_t)conversion<ElapsedCounter, MICROUNITS>(value);
}

uint64_t ElapsedCounter::nanoseconds(const ElapsedCounter& value) {
  return (uint64_t)conversion<ElapsedCounter, NANOUNITS>(value);
}

uint64_t FastElapsedCounter::frequency() {
#if defined(X86) && !defined(ZERO)
  static bool valid_rdtsc = Rdtsc::initialize();
  if (valid_rdtsc) {
    static const uint64_t freq = (uint64_t)Rdtsc::frequency();
    return freq;
  }
#endif
  static const uint64_t freq = (uint64_t)os::elapsed_frequency();
  return freq;
}

#ifdef X86
FastElapsedCounterStamped::FastElapsedCounterStamped() : FastElapsedCounter(Rdtsc::elapsed_counter()) {}
#else
FastElapsedCounterStamped::FastElapsedCounterStamped() : FastElapsedCounter(os::elapsed_counter()) {}
#endif

void FastElapsedCounter::stamp() {
  _instant = now().value();
}

FastElapsedCounter FastElapsedCounter::now() {
  return FastElapsedCounterStamped();
}

double FastElapsedCounter::seconds(const FastElapsedCounter& value) {
  return conversion<FastElapsedCounter, 1>(value);
}
uint64_t FastElapsedCounter::milliseconds(const FastElapsedCounter& value) {
  return (uint64_t)conversion<FastElapsedCounter, MILLIUNITS>(value);
}

uint64_t FastElapsedCounter::microseconds(const FastElapsedCounter& value) {
  return (uint64_t)conversion<FastElapsedCounter, MICROUNITS>(value);
}

uint64_t FastElapsedCounter::nanoseconds(const FastElapsedCounter& value) {
  return (uint64_t)conversion<FastElapsedCounter, NANOUNITS>(value);
}

Tickspan::Tickspan(const Ticks& end, const Ticks& start) :
  _elapsed_interval(end.value() - start.value())
#ifdef X86
  , _ft_elapsed_interval(end.ft_value() - start.ft_value())
#endif
  {}

Tickspan::Tickspan(jlong interval) :
  _elapsed_interval(interval)
#ifdef X86
  , _ft_elapsed_interval(interval)
#endif
  {}

Tickspan& Tickspan::operator+=(const Tickspan& rhs) {
  _elapsed_interval += rhs._elapsed_interval;
  X86_ONLY(_ft_elapsed_interval += rhs._ft_elapsed_interval;)
  return *this;
}

Tickspan& Tickspan::operator-=(const Tickspan& rhs) {
  _elapsed_interval -= rhs._elapsed_interval;
  X86_ONLY(_ft_elapsed_interval -= rhs._ft_elapsed_interval;)
  return *this;
}

jlong Tickspan::ft_value() const {
#ifdef X86
  return _ft_elapsed_interval.value();
#else
  return _elapsed_interval.value();
#endif
}

Ticks::Ticks(jlong stamp) :
  _elapsed(stamp)
#ifdef X86
  , _ft_elapsed(stamp)
#endif
  {}

Ticks Ticks::now() {
  TicksStamped dec;
  return dec;
}

Ticks& Ticks::operator+=(const Tickspan& rhs) {
  _elapsed += rhs.value();
  X86_ONLY(_ft_elapsed += rhs.ft_value();)
  return *this;
}

Ticks& Ticks::operator-=(const Tickspan& rhs) {
  _elapsed -= rhs.value();
  X86_ONLY(_ft_elapsed -= rhs.ft_value();)
  return *this;
}

void Ticks::stamp() {
  _elapsed.stamp();
  X86_ONLY(_ft_elapsed.stamp();)
}

jlong Ticks::ft_value() const {
#ifdef X86
  return _ft_elapsed.value();
#else
  return _elapsed.value();
#endif
}

TicksStamped::TicksStamped() : Ticks() {
  _elapsed.stamp();
  X86_ONLY(_ft_elapsed.stamp());
}