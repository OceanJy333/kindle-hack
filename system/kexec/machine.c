// input: loaded ARM image; output: cache-clean, MMU-off kernel handoff; pos: Voyage RAM boot.
// SPDX-License-Identifier: GPL-2.0-only
#define machine_kexec original_machine_kexec
#include "orig/machine_kexec.c"
#undef machine_kexec


void machine_kexec(struct kimage *image)
{
 unsigned long physical = page_to_pfn(image->control_code_page) << PAGE_SHIFT;
 void *buffer = page_address(image->control_code_page);
 void (*restart)(unsigned long, void (*)(void)) = buffer;
 /* The upper half of the control page is the temporary reset stack. */
 BUG_ON(relocate_new_kernel_size > KEXEC_CONTROL_PAGE_SIZE / 2);
 kexec_start_address = image->start;
 kexec_indirection_page = image->head & PAGE_MASK;
 kexec_mach_type = machine_arch_type;
 kexec_boot_atags = image->start - KEXEC_ARM_ZIMAGE_OFFSET + KEXEC_ARM_ATAGS_OFFSET;
 memcpy(buffer, relocate_new_kernel, relocate_new_kernel_size);
 flush_icache_range((unsigned long)buffer, (unsigned long)buffer + KEXEC_CONTROL_PAGE_SIZE);
 printk(KERN_INFO "Ocean handoff: dedicated stack, physical MMU-off relocation\n");
 if (kexec_reinit) kexec_reinit();
 local_irq_disable();
 local_fiq_disable();
 setup_mm_for_reboot(0);
 printk(KERN_INFO "Ocean handoff: identity map ready\n");
 flush_cache_all();
 outer_flush_all();
 printk(KERN_INFO "Ocean handoff: outer cache flushed\n");
 outer_disable();
 printk(KERN_INFO "Ocean handoff: outer cache disabled\n");
 restart(physical, __cpuc_flush_kern_all);
 BUG();
}
