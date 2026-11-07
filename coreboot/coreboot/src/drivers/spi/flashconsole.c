/* SPDX-License-Identifier: GPL-2.0-only */
/* X380 diagnostic console: append only, BSP only, no erase or SMM traffic. */
#include <commonlib/helpers.h>
#include <commonlib/region.h>
#include <console/flash.h>
#include <fmap.h>
#include <smp/node.h>
#include <types.h>
#include <x380_ibb.h>

#if ENV_RAMSTAGE

#define LOG_START 0x7c0000
#define LOG_SIZE 0x40000
#define LINE_SIZE 128

static struct region_device log_device;
static bool initialized, enabled, busy;
static size_t offset, pending;
static u8 line[LINE_SIZE];

bool flashconsole_is_busy(void)
{
	return busy;
}

static bool allowed_cpu(void)
{
	return ENV_RAMSTAGE && !ENV_SMM && boot_cpu();
}

void flashconsole_init(void)
{
	u8 buffer[256];
	struct region area;
	if (!allowed_cpu() || initialized || busy)
		return;
	initialized = true;
	busy = true;
	/* Do not probe an absent/hidden SPI PCI function or change locked policy. */
	if (!x380_ibb_spi_console_prepare() || fmap_locate_area("CONSOLE", &area) ||
	    region_offset(&area) != LOG_START || region_sz(&area) != LOG_SIZE ||
	    fmap_locate_area_as_rdev_rw("CONSOLE", &log_device))
		goto out;
	/* Use the end of the last programmed byte, including torn writes. Never
	 * reuse an erased hole before later data after an interrupted boot. */
	offset = 0;
	for (size_t at = 0; at < LOG_SIZE; at += sizeof(buffer)) {
		if (rdev_readat(&log_device, buffer, at, sizeof(buffer)) != sizeof(buffer))
			goto out;
		for (size_t i = 0; i < sizeof(buffer); ++i)
			if (buffer[i] != 0xff)
				offset = at + i + 1;
	}
	enabled = offset < LOG_SIZE;
out:
	busy = false;
}

void flashconsole_tx_byte(unsigned char c)
{
	if (!allowed_cpu() || busy || !enabled)
		return;
	line[pending++] = c;
	if (pending == LINE_SIZE || c == '\n' || pending == LOG_SIZE - offset)
		flashconsole_tx_flush();
}

void flashconsole_tx_flush(void)
{
	if (!allowed_cpu() || busy || !enabled || !pending)
		return;
	busy = true;
	/* Disable on errors: do not retry a possibly partly programmed line. */
	if (rdev_writeat(&log_device, line, offset, pending) != (ssize_t)pending) {
		enabled = false;
	} else {
		offset += pending;
		enabled = offset < LOG_SIZE;
	}
	pending = 0;
	busy = false;
}
#endif
