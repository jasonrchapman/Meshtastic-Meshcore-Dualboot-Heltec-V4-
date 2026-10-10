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
 * otadata offset/size are hardcoded from this project's
 * partitions_dualboot.csv (otadata, 0x1C000, 0x2000) - flash isn't
 * partition-table-aware yet at this point in boot. If that layout ever
 * changes, update these two constants to match.
 */
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "bootloader_flash_priv.h"

#define OTADATA_OFFSET 0x1C000
#define OTADATA_SIZE   0x2000

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
    esp_err_t err = bootloader_flash_erase_range(OTADATA_OFFSET, OTADATA_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "otadata erase failed: %d", err);
    }
}
