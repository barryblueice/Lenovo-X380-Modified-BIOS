/* SPDX-License-Identifier: GPL-2.0-only */
#include <bootstate.h>
#include <cbmem.h>
#include <console/console.h>
#include <cpu/x86/tsc.h>
#include <device/pci_ops.h>
#include <string.h>
#include <types.h>
#include <drivers/intel/fsp2_0/x380_me_retained.h>

static void replay_early_me_trace(void *unused)
{
	(void)unused;
	x380_me_replay();
}

BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, replay_early_me_trace, NULL);
