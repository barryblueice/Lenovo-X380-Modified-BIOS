/* SPDX-License-Identifier: GPL-2.0-only */

#include <build.h>
#include <bootstate.h>
#include <console/console.h>
#include <device/pci_ops.h>
#include <device/mmio.h>
#include <intelblocks/lpc_lib.h>
#include <intelblocks/gpmr.h>
#include <soc/pci_devs.h>
#include <smbios.h>
#include <string.h>
#include <soc/ramstage.h>
#include "variant.h"

const char *smbios_mainboard_bios_version(void)
{
	/* Satisfy thinkpad_acpi. */
	if (strlen(CONFIG_LOCALVERSION))
		return "CBET4000 " CONFIG_LOCALVERSION;

	return "CBET4000 " COREBOOT_VERSION;
}

void mainboard_silicon_init_params(FSP_SIL_UPD *params)
{
	// Setup GPIOs
	variant_config_gpios();

	if (CONFIG(BOARD_LENOVO_X380_YOGA)) {
		/* Allow an installed NVMe endpoint more time to leave Detect. */
		params->PcieDetectTimeoutMs = 1000;

		/* LA-F421P: I2C0 serves the sensor board and KXCJ9;
		 * I2C1 connects the EC sensor bridge. GP0..2 are sensor IRQs.
		 * Keep lid/tablet GPIO ownership and unused pads unchanged.
		 * PchIshEnable is derived from the board devicetree by the SoC.
		 */
		params->PchIshI2c0GpioAssign = 1;
		params->PchIshI2c1GpioAssign = 1;
		params->PchIshI2c2GpioAssign = 0;
		params->PchIshSpiGpioAssign = 0;
		params->PchIshUart0GpioAssign = 0;
		params->PchIshUart1GpioAssign = 0;
		params->PchIshGp0GpioAssign = 1;
		params->PchIshGp1GpioAssign = 1;
		params->PchIshGp2GpioAssign = 1;
		params->PchIshGp3GpioAssign = 0;
		params->PchIshGp4GpioAssign = 0;
		params->PchIshGp5GpioAssign = 0;
		params->PchIshGp6GpioAssign = 0;
		params->PchIshGp7GpioAssign = 0;
		params->PchIshPdtUnlock = 0;
	}
}

static void mainboard_enable(struct device *dev)
{
	if (CONFIG(VARIANT_HAS_DGPU))
		dgpu_detect();
}

struct chip_operations mainboard_ops = {
	.enable_dev = mainboard_enable,
};

/* The vendor maps EC shared RAM at FF000100. Open the window before
 * resource allocation and the common BS_DEV_RESOURCES exit lockdown.
 * The LPC driver then reserves the complete 64-KiB window for the OS. */
static void x380_battery_mmio_window(void *unused)
{
	if (!CONFIG(BOARD_LENOVO_X380_YOGA))
		return;
	u32 lgmr = pci_read_config32(PCH_DEV_LPC, 0x98);
	if (!(lgmr & 1))
		lpc_open_mmio_window(0xff000000, 0x10000);
	if (pci_read_config32(PCH_DEV_LPC, 0x98) != 0xff000001 ||
	    gpmr_read32(0x2740) != 0xff000001) {
		printk(BIOS_ERR, "X380 battery MMIO decode unavailable\n");
		return;
	}

	/* EcIoDxe in the vendor X380 capsule uses MMIO for config2:
	 * protocol slots 0x2d00/0x2d08 -> RVA 0x61c/0x628;
	 * RVA 0xd38..0xd4f: (old & 0x5f) | 0x20.
	 * Preserve option-dependent bits. Bit 7's function is not established.
	 * Do not issue the generic 0xa8 command-port write beforehand. */
	if (read32((void *)0xff0001f0) != 0x48533052) {
		printk(BIOS_ERR, "X380 factory EC config2: identity mismatch\n");
		return;
	}
	u8 before = read8((void *)0xff000102);
	u8 value = (before & 0x5f) | 0x20;
	write8((void *)0xff000102, value);
	u8 after = read8((void *)0xff000102);
	printk(BIOS_INFO, "X380 factory EC config2: %02x -> %02x (read %02x)\n",
	       before, value, after);
	if (after != value)
		printk(BIOS_ERR, "X380 factory EC config2: readback mismatch\n");
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_ENTRY, x380_battery_mmio_window, NULL);
