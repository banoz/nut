# Explicit source list (no GLOB). NUT stays in the repo root.

get_filename_component(NUT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(NUT_COMMON_SRCS
    ${NUT_ROOT}/common/atexit.c
    ${NUT_ROOT}/common/common-nut_version.c
    ${NUT_ROOT}/common/common.c
    ${NUT_ROOT}/common/parseconf.c
    ${NUT_ROOT}/common/setenv.c
    ${NUT_ROOT}/common/state.c
    ${NUT_ROOT}/common/str.c
    ${NUT_ROOT}/common/strsep.c
    ${NUT_ROOT}/common/timegm_fallback.c
    ${NUT_ROOT}/common/unsetenv.c
    ${NUT_ROOT}/common/upsconf.c
)

set(NUT_DRIVER_SRCS
    ${NUT_ROOT}/drivers/main.c
    ${NUT_ROOT}/drivers/dstate.c
    ${NUT_ROOT}/drivers/usbhid-ups.c
    ${NUT_ROOT}/drivers/usb-common.c
    ${NUT_ROOT}/drivers/libhid.c
    ${NUT_ROOT}/drivers/hidparser.c
    ${NUT_ROOT}/drivers/apc-hid.c
    ${NUT_ROOT}/drivers/explore-hid.c
    ${NUT_ROOT}/drivers/upsdrvquery.c
)

set(NUT_SERVER_SRCS
    ${NUT_ROOT}/server/conf.c
    ${NUT_ROOT}/server/desc.c
    ${NUT_ROOT}/server/netget.c
    ${NUT_ROOT}/server/netinstcmd.c
    ${NUT_ROOT}/server/netlist.c
    ${NUT_ROOT}/server/netmisc.c
    ${NUT_ROOT}/server/netset.c
    ${NUT_ROOT}/server/netssl.c
    ${NUT_ROOT}/server/netuser.c
    ${NUT_ROOT}/server/sstate.c
    ${NUT_ROOT}/server/upsd.c
    ${NUT_ROOT}/server/user.c
)

set(NUT_SRCS ${NUT_COMMON_SRCS} ${NUT_DRIVER_SRCS} ${NUT_SERVER_SRCS})

idf_component_register(
    SRCS
        app_main.c
        usb.c
        wifi.c
        espusb.c
        compat/stubs.c
        compat/syslog.c
        compat/af_unix.c
        compat/regex.c
        ${NUT_SRCS}
    INCLUDE_DIRS
        .
        compat
        ${NUT_ROOT}/include
        ${NUT_ROOT}/drivers
        ${NUT_ROOT}/server
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        fatfs
        vfs
)

# Driver and server both historically export main() / do_upsconf_args().
# Rename only the driver translation unit so both can live in one firmware.
set_source_files_properties(${NUT_ROOT}/drivers/main.c PROPERTIES
    COMPILE_DEFINITIONS "main=drv_main;do_upsconf_args=do_upsconf_args_driver"
)

set_source_files_properties(${NUT_ROOT}/drivers/usbhid-ups.c PROPERTIES
    COMPILE_DEFINITIONS "NUT_ESP32_MINIMAL=1"
)

# posix_wrap.h still supplies kill() etc. AF_UNIX remap is --wrap of lwip_*.
set_source_files_properties(${NUT_SRCS} PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/compat/posix_wrap.h;-std=gnu17;-Wno-error;-Wno-nonnull"
)

target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=lwip_socket"
    "-Wl,--wrap=lwip_bind"
    "-Wl,--wrap=lwip_connect"
    "-Wl,--wrap=lwip_accept"
    "-Wl,--wrap=lwip_listen"
    "-Wl,--wrap=abort"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unknown-pragmas
    -Wno-missing-field-initializers
    -Wno-error
    -Wno-error=all
    -Wno-error=unknown-pragmas
    -Wno-error=format
    -Wno-error=format-security
    -Wno-error=nonnull
)

fatfs_create_spiflash_image(var ${CMAKE_CURRENT_LIST_DIR}/../fatfs/var FLASH_IN_PROJECT)
fatfs_create_spiflash_image(usr ${CMAKE_CURRENT_LIST_DIR}/../fatfs/usr FLASH_IN_PROJECT)
