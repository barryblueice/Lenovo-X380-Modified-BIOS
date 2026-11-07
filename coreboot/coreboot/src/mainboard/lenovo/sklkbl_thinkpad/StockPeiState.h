/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_STOCK_PEI_STATE_H
#define X380_STOCK_PEI_STATE_H
typedef unsigned int pei_u32;
typedef unsigned long long pei_u64;
struct stock_pei_ram_range { pei_u64 base, size; };
enum stock_pei_observed {
	STOCK_PEI_SI_DONE_PPI = 1,
	STOCK_PEI_END_PPI = 2,
	STOCK_PEI_MP_PPI = 4,
	STOCK_PEI_POLICY_PPI = 8,
};
struct stock_pei_snapshot {
	pei_u32 magic, version, size, error;
	pei_u32 services_pointer, private_pointer, ppi_count, observed;
};
/* Read-only inspection for this exact stock PeiCore layout. Marker presence is
 * not proof that the underlying hardware operations succeeded. The caller owns
 * ranges/out and supplies tested RAM ranges from an already validated HOB list.
 * DxeIpl puts the original PEI-services pointer at its new IDT base minus 4.
 * This routine never calls 32-bit PEI services from the x64 environment.
 */
int CaptureStockPei(pei_u32 idt_base, const struct stock_pei_ram_range *ranges,
		   pei_u32 count, struct stock_pei_snapshot *out);
#endif
