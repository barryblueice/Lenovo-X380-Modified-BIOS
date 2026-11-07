/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <delay.h>
#include <ec/acpi/ec.h>

#include "autopatcher.h"

/* CompalECRfidDxe 0xb7c: bounded IBF/OBF polling at approximately 30 us. */
#define AP_POLLS 20000
#define AP_DELAY_US 30

static int wait_send(void)
{
	for (unsigned int i = 0; i < AP_POLLS; i++) {
		uint8_t status = inb(EC_SC);

		/* An unexpected response during transmission is a protocol error. */
		if (status & EC_OBF)
			return X380_AP_IO;
		if (!(status & EC_IBF))
			return X380_AP_OK;
		udelay(AP_DELAY_US);
	}
	return X380_AP_TIMEOUT;
}

static int drain_output(void)
{
	for (unsigned int i = 0; i < AP_POLLS; i++) {
		uint8_t status = inb(EC_SC);

		if (status & EC_OBF)
			inb(EC_DATA);
		else if (!(status & EC_IBF))
			return X380_AP_OK;
		udelay(AP_DELAY_US);
	}
	return X380_AP_TIMEOUT;
}

static int native_transfer(void *context, uint8_t command, const uint8_t *tx,
			   size_t tx_size, uint8_t *rx, size_t rx_size)
{
	int ret;

	(void)context;
	/* Restrict this interface to the two reconstructed EEPROM commands. */
	if (!tx || !((command == 0x5e && tx_size == 2 && rx_size == 1 && rx) ||
		     (command == 0x5f && tx_size == 3 && rx_size == 0)))
		return X380_AP_INVALID;
	if (tx[0] >= 8 || tx[1] >= 128)
		return X380_AP_INVALID;
	ret = drain_output();
	if (ret)
		return ret;
	outb(command, EC_SC);
	for (size_t i = 0; i < tx_size; i++) {
		ret = wait_send();
		if (ret)
			return ret;
		outb(tx[i], EC_DATA);
	}
	if (!rx_size)
		return wait_send();
	for (unsigned int i = 0; i < AP_POLLS; i++) {
		if (inb(EC_SC) & EC_OBF) {
			*rx = inb(EC_DATA);
			return X380_AP_OK;
		}
		udelay(AP_DELAY_US);
	}
	return X380_AP_TIMEOUT;
}

const struct x380_ap_io *x380_ap_native_io(void)
{
	static const struct x380_ap_io io = { .transfer = native_transfer };

	return &io;
}
