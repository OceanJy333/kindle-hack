// input: kernel and native loader; output: fixed ARM32 boot contract; pos: shared RAM boot ABI.
// SPDX-License-Identifier: GPL-2.0-only
#ifndef OCEAN_KEXEC_PROTOCOL_H
#define OCEAN_KEXEC_PROTOCOL_H
#ifdef __KERNEL__
#include <linux/types.h>
typedef __u32 ocean_u32;
#else
#include <stdint.h>
typedef uint32_t ocean_u32;
#endif
#define OCEAN_KEXEC_ABI 1
#define OCEAN_KEXEC_INFO 0x4f430001UL
#define OCEAN_KEXEC_LOAD 0x45584542UL
#define OCEAN_KEXEC_EXEC 0x45584543UL
#define OCEAN_KEXEC_ARM (40UL << 16)
struct ocean_boot_info {
 ocean_u32 version,machine,revision,serial_low,serial_high,post,ddr;
 unsigned char serial[16],board[16],mac[12],secret[20],boot[16],postmode[16],btmac[12];
};
#endif
