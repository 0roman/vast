// SPDX-FileCopyrightText: (c) 2021 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

/// A printing signal handler meant for SIGSEGV and SIGABRT. Prints a backtrace
/// if support for that is enabled at compile time.
extern "C" void fatal_handler(int sig);
