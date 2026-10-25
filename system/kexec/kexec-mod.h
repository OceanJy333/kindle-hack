// input: stock kernel headers; output: kexec declarations; pos: loadable 3.0 kexec compatibility.
// SPDX-License-Identifier: GPL-2.0-only
#define CONFIG_KEXEC 1
#include <linux/kexec.h>
#undef VMCOREINFO_SYMBOL
#define VMCOREINFO_SYMBOL(_) do {} while (0)
