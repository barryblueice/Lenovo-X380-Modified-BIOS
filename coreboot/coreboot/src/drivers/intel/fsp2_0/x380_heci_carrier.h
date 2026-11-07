/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_HECI_CARRIER_H
#define X380_HECI_CARRIER_H
#ifndef X380_CARRIER_HOST_TEST
#include <device/mmio.h>
#endif

/* Called only after exact host/LPC/HECI IDs match. No writes or polling.
 * Pre-memory has no UEFI memory map: restrict MMIO to the platform's fixed
 * HECI1_BASE_ADDRESS (soc/intel/skylake/include/soc/iomap.h).
 * Status: 1 sampled, 2 decode off, 3 other BAR, 4 no PM list, 5 D0 unproved.
 */
struct x380_heci_carrier {
	uint32_t command, bar0, bar1, pmcsr, host_csr, cse_csr, status;
};

static inline void x380_heci_carrier_sample(struct x380_heci_carrier *s)
{
	const unsigned int dev = PCI_DEV(0, 22, 0);
	memset(s, 0, sizeof(*s));
	s->command = pci_s_read_config32(dev, 4);
	s->bar0 = pci_s_read_config32(dev, 0x10);
	s->bar1 = pci_s_read_config32(dev, 0x14);
	s->status = 2;
	if (!(s->command & 2))
		return;
	s->status = 3;
	unsigned int type = (s->bar0 >> 1) & 3;
	if ((s->bar0 & 1) || (type != 0 && type != 2) ||
	    (type == 2 && s->bar1) || (s->bar0 & ~15u) != 0xfed1a000u)
		return;
	s->status = 4;
	if (!(s->command & (1u << 20)))
		return;
	unsigned int cap = pci_s_read_config32(dev, 0x34) & 255;
	uint64_t seen = 0;
	s->status = 5;
	for (unsigned int i = 0; i < 48 && cap; i++) {
		if (cap < 0x40 || cap > 0xf8 || (cap & 3) || (seen & (1ULL << (cap / 4))))
			return;
		seen |= 1ULL << (cap / 4);
		uint32_t header = pci_s_read_config32(dev, cap);
		if ((header & 255) == 1) {
			s->pmcsr = pci_s_read_config32(dev, cap + 4);
			if (s->pmcsr == 0xffffffffu || (s->pmcsr & 3))
				return;
			s->host_csr = read32((const void *)(uintptr_t)0xfed1a004u);
			s->cse_csr = read32((const void *)(uintptr_t)0xfed1a00cu);
			s->status = 1;
			return;
		}
		cap = (header >> 8) & 255;
	}
}

static inline void x380_heci_carrier_print(const char *phase, const struct x380_heci_carrier *s)
{
	printk(BIOS_EMERG, "TRACE014 %s HC_STATUS=%u CMD=%08x BAR0=%08x BAR1=%08x PMCSR=%08x HOST_CSR=%08x CSE_CSR=%08x\n",
	       phase, s->status, s->command, s->bar0, s->bar1, s->pmcsr, s->host_csr, s->cse_csr);
}
#endif
