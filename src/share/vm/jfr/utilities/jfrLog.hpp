/*
 * Copyright (c) 1997, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JFR_UTILITIES_JFRLOG_HPP
#define SHARE_VM_JFR_UTILITIES_JFRLOG_HPP

#include "runtime/os.hpp"
#include "utilities/debug.hpp"

// This file holds all globally used constants & types, class (forward)
// declarations and a few frequently used utility functions.

//----------------------------------------------------------------------------------------------------
// Printf-style formatters for fixed- and variable-width types as pointers and
// integers.  These are derived from the definitions in inttypes.h.  If the platform
// doesn't provide appropriate definitions, they should be provided in
// the compiler-specific definitions file (e.g., globalDefinitions_gcc.hpp)

#define BOOL_TO_STR(_b_) ((_b_) ? "true" : "false")

// Format 32-bit quantities.
#define INT32_FORMAT           "%" PRId32
#define UINT32_FORMAT          "%" PRIu32
#define INT32_FORMAT_W(width)  "%" #width PRId32
#define UINT32_FORMAT_W(width) "%" #width PRIu32

#define PTR32_FORMAT           "0x%08" PRIx32
#define PTR32_FORMAT_W(width)  "0x%" #width PRIx32

// Format 64-bit quantities.
#define INT64_FORMAT           "%" PRId64
#define UINT64_FORMAT          "%" PRIu64
#define UINT64_FORMAT_X        "%" PRIx64
#define INT64_FORMAT_W(width)  "%" #width PRId64
#define UINT64_FORMAT_W(width) "%" #width PRIu64

#define PTR64_FORMAT           "0x%016" PRIx64

// Format jlong, if necessary
#ifndef JLONG_FORMAT
#define JLONG_FORMAT           INT64_FORMAT
#endif
#ifndef JULONG_FORMAT
#define JULONG_FORMAT          UINT64_FORMAT
#endif
#ifndef JULONG_FORMAT_X
#define JULONG_FORMAT_X        UINT64_FORMAT_X
#endif

// Format pointers which change size between 32- and 64-bit.
#ifdef  _LP64
#define INTPTR_FORMAT "0x%016" PRIxPTR
#define PTR_FORMAT    "0x%016" PRIxPTR
#else   // !_LP64
#define INTPTR_FORMAT "0x%08"  PRIxPTR
#define PTR_FORMAT    "0x%08"  PRIxPTR
#endif  // _LP64

// Format pointers without leading zeros
#define INTPTRNZ_FORMAT "0x%"  PRIxPTR

#define INTPTR_FORMAT_W(width)   "%" #width PRIxPTR

#define SSIZE_FORMAT             "%"   PRIdPTR
#define SIZE_FORMAT              "%"   PRIuPTR
#define SIZE_FORMAT_HEX          "0x%" PRIxPTR
#define SSIZE_FORMAT_W(width)    "%"   #width PRIdPTR
#define SIZE_FORMAT_W(width)     "%"   #width PRIuPTR
#define SIZE_FORMAT_HEX_W(width) "0x%" #width PRIxPTR

#define INTX_FORMAT           "%" PRIdPTR
#define UINTX_FORMAT          "%" PRIuPTR
#define INTX_FORMAT_W(width)  "%" #width PRIdPTR
#define UINTX_FORMAT_W(width) "%" #width PRIuPTR


#define log_error(...)     (!log_is_enabled(Error, __VA_ARGS__))    ? (void)0 : tty->print_cr
#define log_warning(...)   (!log_is_enabled(Warning, __VA_ARGS__))  ? (void)0 : tty->print_cr
#define log_info(...)      (!log_is_enabled(Info, __VA_ARGS__))     ? (void)0 : tty->print_cr
#define log_debug(...)     (!log_is_enabled(Debug, __VA_ARGS__))    ? (void)0 : tty->print_cr
#define log_trace(...)     (!log_is_enabled(Trace, __VA_ARGS__))    ? (void)0 : tty->print_cr

#define log_is_enabled(level, ...) (LogImpl::is_level(LogLevel::level))

class LogLevel : public AllStatic {
 public:
   enum type {
     Off,
     Trace,
     Debug,
     Info,
     Warning,
     Error
   };
};
typedef LogLevel::type LogLevelType;

class LogImpl {
 public:

  LogImpl() {}

  static bool is_level(LogLevelType level) {
    // TODO For debug case.
//    return true;
    if (level >= (jint)LogLevel::Error) {
      return true;
    }

    return false;
  }
};

#endif //SHARE_VM_JFR_UTILITIES_JFRLOG_HPP