/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_ME_TRACE_H
#define X380_ME_TRACE_H
#include "x380_heci_carrier.h"

/* Read-only, fixed-address observation. No unhide, reset, delay or HECI send. */
struct x380_me_sample {
	uint32_t host, lpc, mei;
	uint32_t hb[4], me[7];
	tsc_t ticks;
	struct x380_heci_carrier carrier;
	bool platform_valid, mei_visible;
};

static inline void x380_me_sample(struct x380_me_sample *s)
{
	static const unsigned int hb_offsets[] = { 0x70, 0x74, 0x78, 0x7c };
	static const unsigned int me_offsets[] = { 0x40, 0x44, 0x48, 0x60, 0x64, 0x68, 0x6c };
	memset(s, 0, sizeof(*s));
	s->ticks = rdtsc();
	s->host = pci_s_read_config32(PCI_DEV(0, 0, 0), 0);
	s->lpc = pci_s_read_config32(PCI_DEV(0, 31, 0), 0);
	s->platform_valid = s->host == 0x59148086 && s->lpc == 0x9d4e8086;
	if (!s->platform_valid)
		return;
	for (unsigned int i = 0; i < 4; i++)
		s->hb[i] = pci_s_read_config32(PCI_DEV(0, 0, 0), hb_offsets[i]);
	s->mei = pci_s_read_config32(PCI_DEV(0, 22, 0), 0);
	s->mei_visible = s->mei == 0x9d3a8086;
	if (!s->mei_visible)
		return;
	for (unsigned int i = 0; i < 7; i++)
		s->me[i] = pci_s_read_config32(PCI_DEV(0, 22, 0), me_offsets[i]);
	x380_heci_carrier_sample(&s->carrier);
}

static inline void x380_me_print(const char *phase, const struct x380_me_sample *s)
{
	printk(BIOS_EMERG, "TRACE014 %s tsc=%08x%08x host=%08x lpc=%08x platform_valid=%u\n",
		phase, s->ticks.hi, s->ticks.lo, s->host, s->lpc, s->platform_valid);
	if (!s->platform_valid)
		return;
	printk(BIOS_EMERG, "TRACE014 %s HB70=%08x HB74=%08x HB78=%08x HB7C=%08x\n",
		phase, s->hb[0], s->hb[1], s->hb[2], s->hb[3]);
	printk(BIOS_EMERG, "TRACE014 %s MEI=%08x mei_visible=%u\n", phase, s->mei, s->mei_visible);
	if (!s->mei_visible)
		return;
	printk(BIOS_EMERG, "TRACE014 %s ME40=%08x ME44=%08x ME48=%08x ME60=%08x ME64=%08x ME68=%08x ME6C=%08x\n",
		phase, s->me[0], s->me[1], s->me[2], s->me[3], s->me[4], s->me[5], s->me[6]);
	x380_heci_carrier_print(phase, &s->carrier);
}
#endif
