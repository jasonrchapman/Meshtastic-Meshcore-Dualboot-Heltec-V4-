/*
 * Dual-boot: always land on the selector after a genuine hardware reset (RST
 * button / power-on), never on a GPIO-hold timing race against the ROM's own
 * download-mode strapping check (holding PRG through a reset always wins
 * that race on this board - the custom bootloader's own long-hold
 * factory-reset detection never gets a chance to run).
 *
 * Only RESET_REASON_CHIP_POWER_ON erases otadata (an allow-list, not a
 * deny-list) - on the S3 that value also covers a brief brownout and the
 * super-watchdog, which the hardware reset-cause register can't tell apart
 * from the RST button; that's as narrow as this chip's reset reporting
 * allows. Every other reason - the selector's own esp_restart() after you
 * pick a firmware (SOFTWARE reset), deep-sleep wake, any watchdog, etc. -
 * is explicitly left alone, so neither picking a firmware nor a sleep/wake
 * cycle bounces back to the selector. The stock bootloader already falls
 * back to the factory partition (the selector) whenever otadata is
 * blank/invalid - see bootloader_utility_get_selected_boot_partition() in
 * ESP-IDF's own bootloader_utility.c - so this needs no new boot-selection
 * logic, just nudging that existing fallback to fire on a real
 * power-on-class reset.
 *
 * The otadata partition is located by reading the real partition table off
 * flash at bootloader_after_init() time, via
 * bootloader_common_erase_part_type_data() - the same public function
 * ESP-IDF's own CONFIG_BOOTLOADER_FACTORY_RESET feature uses internally
 * (see bootloader/subproject/main/bootloader_start.c). No offset/size is
 * hardcoded here, so this hook works with whatever partitions_dualboot.csv
 * layout a project actually builds with - it was previously hardcoded to
 * one specific offset, which silently broke if the partition table the
 * selector was built against didn't match.
 */
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "bootloader_common.h"

static const char *TAG = "dualboot-hook";

/* Forces the linker to include this file's symbols - the hooks below are
 * declared weak in ESP-IDF, so without this they'd silently be dropped. */
void bootloader_hooks_include(void) {
}

void bootloader_before_init(void) {
    /* Flash isn't initialized yet at this point - nothing to do here. */
}

void bootloader_after_init(void) {
    soc_reset_reason_t reason = esp_rom_get_reset_reason(0);

    /* Allow-list, not deny-list: only RESET_REASON_CHIP_POWER_ON shows the
     * selector. On the S3 that value is also shared with
     * RESET_REASON_CHIP_BROWN_OUT and RESET_REASON_CHIP_SUPER_WDT - the
     * hardware reset-cause register genuinely can't tell the RST button
     * apart from a true cold power-on or a brief brownout, they're the same
     * value by design (see soc/esp32s3/include/soc/reset_reasons.h). That's
     * as narrow as this chip allows.
     *
     * Everything else is explicitly left alone and boots straight into
     * whichever firmware was last selected - this includes
     * RESET_REASON_CORE_DEEP_SLEEP (a sleep/wake board would otherwise
     * reshow the selector and erase otadata on every single wake, which is
     * both disruptive and needless flash wear), every watchdog reason, the
     * selector's own esp_restart() (RESET_REASON_CORE_SW/CPU0_SW), and
     * anything else not explicitly power-on-class. */
    if (reason != RESET_REASON_CHIP_POWER_ON) {
        return;
    }

    ESP_LOGI(TAG, "Power-on-class reset (reason=%d) - erasing otadata so the selector always shows", reason);
    /* list_erase="" - erase nothing by label, just the otadata partition
     * (ota_data_erase=true), wherever the real partition table says it is. */
    if (!bootloader_common_erase_part_type_data("", true)) {
        ESP_LOGE(TAG, "otadata erase failed");
    }
}
