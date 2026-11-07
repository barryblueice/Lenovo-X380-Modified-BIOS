/* SPDX-License-Identifier: GPL-2.0-only */
/* Original signed PEI -> integrated coreboot ramstage, cold boot only. */
#include <bootmem.h>
#include <boot/coreboot_tables.h>
#include <cbfs.h>
#include <cbmem.h>
#include <commonlib/region.h>
#include <console/cbmem_console.h>
#include <console/console.h>
#include <console/flash.h>
#include <device/mmio.h>
#include <fsp/util.h>
#include <intelblocks/pmclib.h>
#include <romstage_handoff.h>
#include <soc/pm.h>
#include <string.h>
#include <timestamp.h>
#include <types.h>
#include <x380_ibb.h>
#include "Hybrid.h"
#include "EarlyTrace.h"

uintptr_t x380_ibb_handoff_ptr;
static struct cbfs_boot_device ram_cbfs;
static bool ready;
int cbmem_create_empty_bounded(size_t max_size);
const struct cbfs_boot_device *x380_ibb_cbfs(void);
void x380_ibb_runtime_init(void);

struct {
	u32 stage, cbmem_base, cbmem_size, spd_size, spd_fnv1a;
	u32 config_size, config_fnv1a, payload_size, fsps_size;
	u32 observed, state_cbmem, hob_cbmem, hob_bytes, main_returns, inherited_phases;
} x380_ibb_report;

static void checkpoint(u8 code)
{
	__asm__ volatile("outb %0,$0x80; outb %0,$0x86" : : "a"(code));
	x380_early_event(code, x380_ibb_report.stage, x380_ibb_handoff_ptr, x380_ibb_report.observed);
}

static u32 pci_cfg(u32 address)
{
	u32 value;
	__asm__ volatile("outl %0,%1" : : "a"(address), "Nd"((u16)0xcf8));
	__asm__ volatile("inl %1,%0" : "=a"(value) : "Nd"((u16)0xcfc));
	return value;
}

static void pci_setcfg(u32 address, u32 value)
{
	__asm__ volatile("outl %0,%1" : : "a"(address), "Nd"((u16)0xcf8));
	__asm__ volatile("outl %0,%1" : : "a"(value), "Nd"((u16)0xcfc));
}

static __noreturn void stop(u8 code)
{
	checkpoint(code);
	for (;;)
		__asm__ volatile("cli; hlt");
}

static void log_text(const char *s)
{
	while (*s)
		cbmemc_tx_byte(*s++);
}

static u32 fingerprint(const u8 *p, size_t size)
{
	u32 hash = 2166136261U;
	while (size--)
		hash = (hash ^ *p++) * 16777619U;
	return hash;
}

const struct cbfs_boot_device *x380_ibb_cbfs(void)
{
	return ready ? &ram_cbfs : NULL;
}

bool x380_ibb_inherit_silicon(void)
{
	return ready && x380_ibb_report.observed == 15;
}

/* Vendor record with an explicit RAM address, never a SPI-relative offset. */
struct x380_ram_cbfs_record {
	u32 tag, size, magic, version;
	u64 base, length;
	u32 fingerprint, flags;
} __packed;

void lb_board(struct lb_header *header)
{
	if (!x380_ibb_inherit_silicon())
		stop(0xd8);
	const struct hybrid_handoff *h = (void *)x380_ibb_handoff_ptr;
	struct x380_ram_cbfs_record *record = (void *)lb_new_record(header);
	*record = (struct x380_ram_cbfs_record) {
		.tag = 0x58434246, .size = sizeof(*record),
		.magic = 0x52434246, .version = 1,
		.base = h->cbfs_base, .length = h->cbfs_size,
		.fingerprint = fingerprint((void *)(uintptr_t)h->cbfs_base, h->cbfs_size),
		.flags = 0,
	};
}

void x380_ibb_inherited_phase(u32 phase)
{
	if (!x380_ibb_inherit_silicon())
		stop(0xd8);
	x380_ibb_report.inherited_phases |= phase;
	printk(BIOS_NOTICE, "X380 IBB: inherited vendor phase mask %#x; FSP API not called.\n", phase);
}

bool x380_ibb_spi_console_prepare(void)
{
	/* Stock PEI and this board use FE010000. No BAR relocation/unhide here. */
	if (!ready || pci_cfg(0x8000fd00) != 0x9d248086 ||
	    !(pci_cfg(0x8000fd04) & 2) ||
	    (pci_cfg(0x8000fd10) & ~0xfffU) != 0xfe010000)
		return false;
	u32 control = pci_cfg(0x8000fddc);
	if (control & (1U << 5)) /* SMM-only writes cannot support this console. */
		return false;
	if (!(control & 1) && (control & 2))
		return false;
	/* Read/write permission for BIOS, exact region and no protecting PR. */
	if ((read32p(0xfe010050) & 0x202) != 0x202 ||
	    read32p(0xfe010058) != 0x0fff0700)
		return false;
	for (u32 i = 0; i < 5; ++i) {
		u32 pr = read32p(0xfe010084 + i * 4);
		u32 low = (pr & 0x7fff) << 12;
		u32 high = (((pr >> 16) & 0x7fff) << 12) | 0xfff;
		if ((pr & 0x80008000) && low <= high && low < 0x800000 && high >= 0x7c0000)
			return false;
	}
	if (!(control & 1)) {
		/* Only unlocked WPD is enabled, after validating the permitted range.
		 * Write the low byte only, preserving RW1C status above it. */
		__asm__ volatile("outl %0,%1" : : "a"(0x8000fddcU), "Nd"((u16)0xcf8));
		__asm__ volatile("outb %0,%1" : : "a"((u8)(control | 1)), "Nd"((u16)0xcfc));
		if (!(pci_cfg(0x8000fddc) & 1))
			return false;
	}
	return true;
}

void x380_ibb_log_summary(void)
{
	x380_early_event(0xf3, x380_ibb_report.stage, x380_ibb_handoff_ptr, x380_ibb_report.observed);
	printk(BIOS_NOTICE, "X380 IBB: coreboot handoff complete; SPI console requested, level 7.\n");
	printk(BIOS_DEBUG, "X380 IBB: stage=%u observed=%#x CBMEM=%#x/%#x HOB=%#x/%#x CBFS SPD=%u payload=%u.\n",
	       x380_ibb_report.stage, x380_ibb_report.observed,
	       x380_ibb_report.cbmem_base, x380_ibb_report.cbmem_size,
	       x380_ibb_report.hob_cbmem, x380_ibb_report.hob_bytes,
	       x380_ibb_report.spd_size, x380_ibb_report.payload_size);
	flashconsole_tx_flush();
}

/* Validate the bounded copied PI chain before generic coreboot HOB consumers use it. */
static bool valid_hobs(const u8 *base, u32 bytes)
{
	u32 pos = 0;
	if (bytes < 64 || bytes > 0x10000 || *(const u16 *)base != 1 ||
        *(const u16 *)(base + 2) < 56 || *(const u32 *)(base + 12) > 4)
		return false;
	for (u32 n = 0; n < 512 && pos + 8 <= bytes; ++n) {
		const u8 *p = base + pos;
		u16 type = *(const u16 *)p, length = *(const u16 *)(p + 2);
		if (length < 8 || (length & 7) || length > bytes - pos)
			return false;
		if (type == 0xffff)
			return length == 8 && pos + length == bytes;
		if ((type == 2 || type == 3) && length < 48)
			return false;
		if (type == 4 && length < 24)
			return false;
		if (type == 2 || type == 3) {
			u32 offset = type == 2 ? 24 : 32;
			u64 address = *(const u64 *)(p + offset), size = *(const u64 *)(p + offset + 8);
			if (address + size < address)
				return false;
		}
		if (type == 5 && length < 24)
			return false;
		pos += length;
	}
	return false;
}

void x380_ibb_runtime_init(void)
{
	struct hybrid_handoff *h = (void *)x380_ibb_handoff_ptr;
	checkpoint(0xf0);
	if (x380_ibb_handoff_ptr < 0x100000 || (x380_ibb_handoff_ptr & 3) ||
	    h->magic != HYBRID_MAGIC || h->version != HYBRID_VERSION || h->size != sizeof(*h))
		stop(0xd4);
	if (h->base < 0x100000 || h->base > 0xdfc00000 || h->memory_size > 0x280000 ||
	    h->cbmem_top != h->base + 0x3d0000 || cbmem_top() != h->cbmem_top ||
	    (h->cbmem_low & 4095) || h->cbmem_low < h->base + h->memory_size ||
        h->cbmem_low >= h->cbmem_top || h->boot_mode > 4 ||
	    h->origin > HYBRID_ORIGIN_DXE ||
	    (h->origin == HYBRID_ORIGIN_PEI &&
	     (h->pei.magic != 0x49504553 || h->pei.version != 1 || h->pei.size != sizeof(h->pei) ||
	      h->pei.error || h->pei.observed != 15)) ||
	    h->cbfs_base < 0x100000 || h->cbfs_base >= 0xe0000000 ||
	    h->cbfs_size < 64 || h->cbfs_size > 0x3c0000 || h->cbfs_size > 0xe0000000 - h->cbfs_base ||
	    (h->cbfs_base < h->base + 0x400000 && h->cbfs_base + h->cbfs_size > h->base) ||
	    h->hob_base > 0xffff0000 || !valid_hobs((void *)(uintptr_t)h->hob_base, h->hob_size))
		stop(0xd5);
	struct hybrid_handoff saved = *h;
	/* DXE origin: pei snapshot is unreachable from DXE; the driver supplies synthesized HOBs. */
	/* Native ramstage uses this board's fixed ACPI decode, so reject a mismatch. */
	if (pci_cfg(0x8000f800) != 0x9d4e8086 || pci_cfg(0x8000fa00) != 0x9d218086 ||
	    (pci_cfg(0x8000fa40) & 0xff80) != 0x1800 || !(pci_cfg(0x8000fa44) & 0x80))
		stop(0xd9);
	/* Coreboot and the retained payload use E0000000/256 buses. Do only the
	 * PCIEXBAR part of normal bootblock setup; never clear the live TSEG here. */
	u32 tolud = pci_cfg(0x800000bc) & 0xfff00000;
	u32 ecam = pci_cfg(0x80000060);
	if ((pci_cfg(0x80000000) & 0xffff) != 0x8086 || !tolud || tolud > CONFIG_ECAM_MMCONF_BASE_ADDRESS ||
	    !(ecam & 1) || pci_cfg(0x80000064))
		stop(0xd9);
	if (ecam != (CONFIG_ECAM_MMCONF_BASE_ADDRESS | 1U)) {
		pci_setcfg(0x80000064, 0);
		pci_setcfg(0x80000060, CONFIG_ECAM_MMCONF_BASE_ADDRESS | 1U);
		if (pci_cfg(0x80000060) != (CONFIG_ECAM_MMCONF_BASE_ADDRESS | 1U) || pci_cfg(0x80000064))
			stop(0xd9);
	}
	size_t bound = h->cbmem_top - h->cbmem_low;
	if (cbmem_create_empty_bounded(bound))
		stop(0xd6);
	h = cbmem_add(HYBRID_CBMEM_HANDOFF, sizeof(*h));
	struct stock_pei_snapshot *state = cbmem_add(HYBRID_CBMEM_STATE, sizeof(*state));
	u8 *hobs = cbmem_add(HYBRID_CBMEM_HOBS, saved.hob_size);
	u32 *hob_pointer = cbmem_add(CBMEM_ID_FSP_RUNTIME, sizeof(*hob_pointer));
	struct timestamp_table *ts = cbmem_add(CBMEM_ID_TIMESTAMP,
		sizeof(*ts) + 192 * sizeof(struct timestamp_entry));
	struct chipset_power_state *ps = cbmem_add(CBMEM_ID_POWER_STATE, sizeof(*ps));
	if (!h || !state || !hobs || !hob_pointer || !ts || !ps)
		stop(0xd6);
	*h = saved;
	*state = saved.pei;
	memcpy(hobs, (void *)(uintptr_t)saved.hob_base, saved.hob_size);
	/* PHIT end pointer follows the copy. Other producer-owned data remains reserved. */
	*(u64 *)(hobs + 48) = (uintptr_t)hobs + saved.hob_size - 8;
	*hob_pointer = (uintptr_t)hobs;
	memset(ts, 0, sizeof(*ts) + 192 * sizeof(struct timestamp_entry));
	ts->max_entries = 192;
	memset(ps, 0, sizeof(*ps));
	/* Only cold boot is admitted. Capture current PM registers before normal ramstage. */
	pmc_fill_power_state(ps);
	ps->prev_sleep_state = 0;
	if (romstage_handoff_init(0) || cbmem_initialize() ||
	    cbmem_find(HYBRID_CBMEM_STATE) != state || cbmem_add(0x58424f56, bound + 4096))
		stop(0xd6);
	x380_ibb_handoff_ptr = (uintptr_t)h;
	x380_ibb_report.observed = state->observed;
	x380_ibb_report.state_cbmem = (uintptr_t)state;
	x380_ibb_report.hob_cbmem = (uintptr_t)hobs;
	x380_ibb_report.hob_bytes = saved.hob_size;
	void *region;
	size_t used;
	if (cbmem_get_region(&region, &used) || (uintptr_t)region < h->cbmem_low ||
	    (uintptr_t)region + used != h->cbmem_top)
		stop(0xd6);
	x380_ibb_report.cbmem_base = (uintptr_t)region;
	x380_ibb_report.cbmem_size = used;
	x380_ibb_report.stage = 1;
	checkpoint(0xf1);
	if (rdev_chain_mem(&ram_cbfs.rdev, (void *)(uintptr_t)h->cbfs_base, h->cbfs_size) ||
	    cbfs_init_boot_device(&ram_cbfs, NULL))
		stop(0xd7);
	ready = true;
	size_t size;
	const u8 *data = cbfs_map("spd.bin", &size);
	if (!data || size != 512)
		stop(0xd7);
	x380_ibb_report.spd_size = size;
	x380_ibb_report.spd_fnv1a = fingerprint(data, size);
	cbfs_unmap((void *)data);
	data = cbfs_map("config", &size);
	if (!data || size < 1024 || size > 0x10000)
		stop(0xd7);
	x380_ibb_report.config_size = size;
	x380_ibb_report.config_fnv1a = fingerprint(data, size);
	cbfs_unmap((void *)data);
	x380_ibb_report.payload_size = cbfs_get_size("fallback/payload");
	x380_ibb_report.fsps_size = cbfs_get_size("fsps.bin");
	if (!x380_ibb_report.payload_size || !x380_ibb_report.fsps_size)
		stop(0xd7);
	x380_ibb_report.stage = 2;
	checkpoint(0xf2);
	log_text("X380 stock IBB: state and HOBs adopted; returning to coreboot main.\n");
	x380_ibb_report.main_returns++;
	/* No diagnostic halt or timed shutdown: normal hardwaremain continues. */
}

/* Keep all producer-owned allocations and the RAM archive out of payload/OS RAM. */
void bootmem_platform_add_ranges(void)
{
	const struct hybrid_handoff *h = (void *)x380_ibb_handoff_ptr;
	if (!x380_ibb_inherit_silicon())
		stop(0xd8);
	bootmem_add_range(h->base, 0x400000, BM_MEM_RESERVED);
	bootmem_add_range(h->cbfs_base, h->cbfs_size, BM_MEM_RESERVED);
	bootmem_add_range(h->bridge_base, h->bridge_size, BM_MEM_RESERVED);
	bootmem_add_range(h->hob_base, h->hob_size, BM_MEM_RESERVED);
	const u8 *base = cbmem_find(HYBRID_CBMEM_HOBS);
	for (u32 pos = 0; pos + 8 <= h->hob_size;) {
		const u8 *p = base + pos;
		u16 type = *(const u16 *)p, length = *(const u16 *)(p + 2);
		if (type == 0xffff)
			break;
		u64 address = 0, bytes = 0;
		if (type == 2) {
			address = *(const u64 *)(p + 24);
			bytes = *(const u64 *)(p + 32);
		} else if (type == 3 && *(const u32 *)(p + 24) != 0) {
			address = *(const u64 *)(p + 32);
			bytes = *(const u64 *)(p + 40);
		}
		if (bytes && address + bytes >= address)
			bootmem_add_range(address, bytes, BM_MEM_RESERVED);
		pos += length;
	}
	/* Restore the CBMEM table type after the containing workspace reservation.
	 * EDK2 ParseCbMemTable needs it to discover ACPI/SMBIOS and other tables. */
	cbmem_add_bootmem();
}
