/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef X380_AUTOPATCHER_H
#define X380_AUTOPATCHER_H

#include <stddef.h>
#include <stdint.h>

#define X380_AP_BANK_SIZE 256
#define X380_AP_DUMP_SIZE (4 * X380_AP_BANK_SIZE + 8)

enum x380_ap_result {
	X380_AP_OK = 0,
	X380_AP_INVALID = -1,
	X380_AP_IO = -2,
	X380_AP_TIMEOUT = -3,
	X380_AP_VERIFY = -4,
};

/* transfer returns zero only when the complete transaction succeeds. */
struct x380_ap_io {
	void *context;
	int (*transfer)(void *context, uint8_t command, const uint8_t *tx,
			size_t tx_size, uint8_t *rx, size_t rx_size);
};

int x380_ap_read(const struct x380_ap_io *io, unsigned int bank,
		 unsigned int offset, uint8_t *value);
int x380_ap_write(const struct x380_ap_io *io, unsigned int bank,
		  unsigned int offset, uint8_t value);
int x380_ap_snapshot(const struct x380_ap_io *io, uint8_t dump[X380_AP_DUMP_SIZE]);

/*
 * Explicitly overwrite bank 0x57 with the original 256-byte patch template.
 * backup must point to X380_AP_BANK_SIZE writable bytes. The entire backup is
 * read before the first write. On read failure it is only partially valid.
 * completed counts acknowledged byte writes, not guaranteed durable writes.
 * On failure, stop immediately; partial changes are possible. No rollback.
 * Caller must retain the backup and prevent concurrent EC access throughout.
 */
int x380_ap_apply_template(const struct x380_ap_io *io,
			   uint8_t backup[X380_AP_BANK_SIZE], size_t *completed);

/*
 * Native 0x66/0x62 transport. Obtaining this interface performs no I/O.
 * Use only in an exclusive maintenance context with competing EC users
 * (including SMM) quiesced. No stock BIOS protocol or permission bypass.
 */
const struct x380_ap_io *x380_ap_native_io(void);

#endif
