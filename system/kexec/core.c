// input: pinned GPL kernel kexec implementation; output: module-local core; pos: temporary kernel loader.
// SPDX-License-Identifier: GPL-2.0-only
// Included kernel/kexec.c: Copyright (C) 2002-2004 Eric Biederman.
// Source: katadelos/linux-3.0.35-lab126 at 7ac2585f384573da8cbdcc987a3503e2e6ff1fb4.
#include <linux/module.h>
#undef module_init
#define module_init(fn) __attribute__((unused)) static int fn(void);
#include "orig/kexec.c"
int panic_on_oops;
