// input: root-owned kexec ioctls; output: loaded image or kernel handoff; pos: Voyage RAM boot interface.
// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/kexec.h>
#include <linux/kallsyms.h>
#include <linux/capability.h>
#include <linux/kmsg_dump.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include "protocol.h"

extern unsigned int system_rev,system_serial_low,system_serial_high,system_post,ddr_mfgid;
extern unsigned char system_serial16[16],system_rev16[16],system_mac_addr[12],system_mac_sec[20];
extern unsigned char system_bootmode[16],system_postmode[16],system_btmac_addr[12];

static long boot_info(unsigned long argument){
 struct ocean_boot_info info={.version=OCEAN_KEXEC_ABI,.machine=machine_arch_type,
  .revision=system_rev,.serial_low=system_serial_low,.serial_high=system_serial_high,
  .post=system_post,.ddr=ddr_mfgid};
 memcpy(info.serial,system_serial16,sizeof(info.serial));memcpy(info.board,system_rev16,sizeof(info.board));
 memcpy(info.mac,system_mac_addr,sizeof(info.mac));memcpy(info.secret,system_mac_sec,sizeof(info.secret));
 memcpy(info.boot,system_bootmode,sizeof(info.boot));memcpy(info.postmode,system_postmode,sizeof(info.postmode));
 memcpy(info.btmac,system_btmac_addr,sizeof(info.btmac));
 return copy_to_user((void __user *)argument,&info,sizeof(info))?-EFAULT:0;
}

static void (*shutdown_kernel)(void);
static void (*prepare_restart)(char *);
static void (*prepare_mmu)(char);
static void (*reset_cpu)(unsigned long);
static void (*finish_cpu)(void);
static void (*dump_log)(enum kmsg_dump_reason);

void machine_shutdown(void){shutdown_kernel();}
void kernel_restart_prepare(char *command){prepare_restart(command);}
void setup_mm_for_reboot(char mode){prepare_mmu(mode);}
void cpu_v7_reset(unsigned long address){reset_cpu(address);}
void cpu_v7_proc_fin(void){finish_cpu();}
void kmsg_dump(enum kmsg_dump_reason reason){dump_log(reason);}

static long control(struct file *file,unsigned request,unsigned long argument){
 struct {unsigned long entry,count;struct kexec_segment __user *segments;unsigned long flags;} load;
 if(!capable(CAP_SYS_BOOT))return -EPERM;
 if(request==OCEAN_KEXEC_INFO)return boot_info(argument);
 if(request==LINUX_REBOOT_CMD_KEXEC-1){
  if(copy_from_user(&load,(void __user *)argument,sizeof(load)))return -EFAULT;
  /* This bridge supports a normal ARM reboot, not crash dump loading. */
  if(load.flags!=0&&load.flags!=KEXEC_ARCH_ARM)return -EINVAL;
  return sys_kexec_load(load.entry,load.count,load.segments,load.flags);
 }
 if(request==LINUX_REBOOT_CMD_KEXEC)return kernel_kexec();
 return -EINVAL;
}
static const struct file_operations operations={.owner=THIS_MODULE,.unlocked_ioctl=control};
static struct miscdevice loader={.minor=MISC_DYNAMIC_MINOR,.name="kexec",.fops=&operations,.mode=0600};

static int __init start(void){
 shutdown_kernel=(void *)kallsyms_lookup_name("machine_shutdown");
 prepare_restart=(void *)kallsyms_lookup_name("kernel_restart_prepare");
 prepare_mmu=(void *)kallsyms_lookup_name("setup_mm_for_reboot");
 reset_cpu=(void *)kallsyms_lookup_name("cpu_v7_reset");
 finish_cpu=(void *)kallsyms_lookup_name("cpu_v7_proc_fin");
 dump_log=(void *)kallsyms_lookup_name("kmsg_dump");
 if(!shutdown_kernel||!prepare_restart||!prepare_mmu||!reset_cpu||!finish_cpu||!dump_log)return -ENOENT;
 return misc_register(&loader);
}
static void __exit stop(void){
 sys_kexec_load(0,0,NULL,0);
 misc_deregister(&loader);
}
module_init(start);
module_exit(stop);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Temporary Voyage ARM kernel boot interface");
