/*
 * Dual-boot: always land on the selector after a genuine hardware reset (RST
 * button / power-on), never on a GPIO-hold timing race against the ROM's own
 * download-mode strapping check (holding PRG through a reset always wins
 * that race on this board - the custom bootloader's own long-hold
 * factory-reset detection never gets a chance to run).
 *
 * The selector's own esp_restart() - the one it calls after you pick
 * MeshCore or Meshtastic - reports a SOFTWARE reset reason on the next boot.
 * That's the one case we deliberately let through, so picking a firmware
 * doesn't just bounce straight back to the selector again. Every OTHER
 * reset reason (RST button, power-on, brownout, etc.) erases otadata, and
 * the stock bootloader already falls back to the factory partition (the
 * selector) whenever otadata is blank/invalid - see
 * bootloader_utility_get_selected_boot_partition() in ESP-IDF's own
 * bootloader_utility.c - so this needs no new boot-selection logic, just
 * nudging that existing fallback to fire on every hardware reset.
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
    if (reason == RESET_REASON_CORE_SW || reason == RESET_REASON_CPU0_SW) {
        // This boot was triggered by the selector's own esp_restart() -
        // honor the firmware choice it just made, don't force it back here.
        return;
    }

    ESP_LOGI(TAG, "Hardware reset (reason=%d) - erasing otadata so the selector always shows", reason);
    esp_err_t err = bootloader_flash_erase_range(OTADATA_OFFSET, OTADATA_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "otadata erase failed: %d", err);
    }
}
