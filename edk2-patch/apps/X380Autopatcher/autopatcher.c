/* SPDX-License-Identifier: GPL-2.0-only */

#include "autopatcher.h"

/*
 * BootOption.ffs PE RVA 0x1040. SHA-256 of all 256 bytes:
 * dce1a98eb599ac2a68a4b587d575c4ff6905c2d218fd24aad4db0880339aa286
 * Includes unrelated fields; this is faithful template replacement, not a
 * proven minimal SVP-only edit. Unspecified bytes are zero.
 */
static const uint8_t patch_template[X380_AP_BANK_SIZE] = {
	[0x30] = 0x8b, 0xc5, 0x04, 0x41,
	[0x50] = 0x01,
	[0x53] = 0xa2, 0xce, 0x46, 0x8e,
	[0x5c] = 0xd9, 0x75, 0x86, 0xe4,
	[0x62] = 0xd1, 0xf0, 0x88, 0xf7, 0x11, 0xea, 0xa6, 0xaa,
	[0xc8] = 0xd9,
};

static int valid_address(unsigned int bank, unsigned int offset)
{
	return (bank == 0x5c && offset < 8) ||
		(bank >= 0x54 && bank <= 0x57 && offset < X380_AP_BANK_SIZE);
}

static int valid_io(const struct x380_ap_io *io)
{
	return io && io->transfer;
}

int x380_ap_read(const struct x380_ap_io *io, unsigned int bank,
		 unsigned int offset, uint8_t *value)
{
	uint8_t tx[2];

	if (!valid_io(io) || !value || !valid_address(bank, offset))
		return X380_AP_INVALID;
	*value = 0xff;
	/* CompalECRfidDxe treats this legacy bank as a stub on X380. */
	if (bank == 0x5c)
		return X380_AP_OK;
	tx[0] = 2 * (bank - 0x54) + offset / 128;
	tx[1] = offset % 128;
	return io->transfer(io->context, 0x5e, tx, sizeof(tx), value, 1);
}

int x380_ap_write(const struct x380_ap_io *io, unsigned int bank,
		  unsigned int offset, uint8_t value)
{
	uint8_t tx[3];

	if (!valid_io(io) || !valid_address(bank, offset))
		return X380_AP_INVALID;
	/* Do not turn the stock no-op into an EC protection-register write. */
	if (bank == 0x5c)
		return X380_AP_OK;
	tx[0] = 2 * (bank - 0x54) + offset / 128;
	tx[1] = offset % 128;
	tx[2] = value;
	return io->transfer(io->context, 0x5f, tx, sizeof(tx), NULL, 0);
}

int x380_ap_snapshot(const struct x380_ap_io *io, uint8_t dump[X380_AP_DUMP_SIZE])
{
	int ret;

	if (!valid_io(io) || !dump)
		return X380_AP_INVALID;
	for (unsigned int i = 0; i < X380_AP_DUMP_SIZE; i++) {
		unsigned int bank = i < 1024 ? 0x54 + i / 256 : 0x5c;
		unsigned int offset = i < 1024 ? i % 256 : i - 1024;

		ret = x380_ap_read(io, bank, offset, &dump[i]);
		if (ret)
			return ret;
	}
	return X380_AP_OK;
}

int x380_ap_apply_template(const struct x380_ap_io *io,
			   uint8_t backup[X380_AP_BANK_SIZE], size_t *completed)
{
	uint8_t value;
	int ret;

	if (!completed)
		return X380_AP_INVALID;
	*completed = 0;
	if (!valid_io(io) || !backup)
		return X380_AP_INVALID;
	for (unsigned int i = 0; i < X380_AP_BANK_SIZE; i++) {
		ret = x380_ap_read(io, 0x57, i, &backup[i]);
		if (ret)
			return ret;
	}
	for (unsigned int i = 0; i < X380_AP_BANK_SIZE; i++) {
		ret = x380_ap_write(io, 0x57, i, patch_template[i]);
		if (ret)
			return ret;
		(*completed)++;
	}
	for (unsigned int i = 0; i < X380_AP_BANK_SIZE; i++) {
		ret = x380_ap_read(io, 0x57, i, &value);
		if (ret)
			return ret;
		if (value != patch_template[i])
			return X380_AP_VERIFY;
	}
	return X380_AP_OK;
}
