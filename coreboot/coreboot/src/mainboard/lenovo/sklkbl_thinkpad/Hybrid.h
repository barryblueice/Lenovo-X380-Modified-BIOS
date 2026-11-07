/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_IBB_HYBRID_H
#define X380_IBB_HYBRID_H
#include "StockPeiState.h"
#define HYBRID_MAGIC 0x38494258U
#define HYBRID_VERSION 3U
/* 0: entered from DxeIpl with a captured stock-PEI snapshot (Test025 path).
 * 1: entered from a DXE driver; pei is zeroed and HOBs are synthesized by the
 *    driver from the UEFI memory map (Test026 path). */
#define HYBRID_ORIGIN_PEI 0U
#define HYBRID_ORIGIN_DXE 1U
#define HYBRID_CBMEM_STATE 0x58504549U
#define HYBRID_CBMEM_HOBS 0x58484f42U
#define HYBRID_CBMEM_HANDOFF 0x58424948U
struct hybrid_handoff {
    pei_u32 magic, version, size, base, memory_size, cbmem_low, cbmem_top;
    pei_u32 cbfs_base, cbfs_size, hob_base, hob_size, bridge_base, bridge_size, boot_mode;
    pei_u32 origin;
    struct stock_pei_snapshot pei;
};
#endif
