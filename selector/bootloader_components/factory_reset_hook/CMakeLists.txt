idf_component_register(SRCS "hooks.c"
                        PRIV_REQUIRES bootloader_support)

# spi_flash was required by the old bootloader_flash_priv.h-based
# implementation (bootloader_flash_erase_range with a hardcoded offset).
# Now using bootloader_common_erase_part_type_data(), declared in
# bootloader_support's own public bootloader_common.h, so spi_flash's
# include path is no longer needed.

# Bootloader hooks are declared weak, so the linker drops them unless
# something forces this file's symbols to be included - see
# bootloader_hooks_include() in hooks.c.
