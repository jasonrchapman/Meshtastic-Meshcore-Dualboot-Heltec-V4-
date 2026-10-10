idf_component_register(SRCS "hooks.c"
                        PRIV_REQUIRES bootloader_support spi_flash)

# Bootloader hooks are declared weak, so the linker drops them unless
# something forces this file's symbols to be included - see
# bootloader_hooks_include() in hooks.c.
