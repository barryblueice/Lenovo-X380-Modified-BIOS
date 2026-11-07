/* SPDX-License-Identifier: GPL-2.0-only */

#define BRIGHTNESS_UP \_SB.PCI0.GFX0.INCB
#define BRIGHTNESS_DOWN \_SB.PCI0.GFX0.DECB
#define EC_LENOVO_H8_EXTENDED_FN_LAYOUT 1
#define THINKPAD_EC_GPE 22

#if CONFIG(BOARD_LENOVO_X380_YOGA)
/* Match the vendor's battery-page settling and completion protocol. */
#define BATTERY_PAGE_DELAY_MS 50
#define BATTERY_PAGE_READY_CHECK
#define X380_BATTERY_MMIO
#define X380_VENDOR_BATTERY
#endif

Name(\TCRT, 100)
Name(\TPSV, 90)
Name(\FLVL, 0)

#include <ec/lenovo/h8/acpi/ec.asl>
#ifndef X380_VENDOR_BATTERY
#include <ec/lenovo/h8/acpi/thinkpad_bat_thresholds_b0.asl>
#endif
