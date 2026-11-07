/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_IBB_RUNTIME_API_H
#define X380_IBB_RUNTIME_API_H
#include <types.h>
bool x380_ibb_inherit_silicon(void);
void x380_ibb_inherited_phase(u32 phase);
bool x380_ibb_spi_console_prepare(void);
void x380_ibb_log_summary(void);
#endif
