/*!
 * @file tinyusb.c
 * @brief Generic USB communication backend (using tinyusb)
 *
 * -------------------------------------------------------------------------- */

// #include "config.h" /* for HAVE_LIBUSB_DETACH_KERNEL_DRIVER flag */
#include "common.h" /* for xmalloc, upsdebugx prototypes */
#include "usb-common.h"
#include "nut_libusb.h"
#include "nut_stdint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "usb/usb_host.h"
#include "usb/hid_host.h"

#include "esp_check.h"
#include "esp_log.h"

#include "espusb.h"

#define TAG "ESPUSB"

#define USB_DRIVER_NAME "USB communication driver (espusb)"
#define USB_DRIVER_VERSION "0.01"

#define MAX_REPORT_SIZE 0x1800

struct espusb_device
{
    hid_host_device_handle_t parent_dev;
    hid_host_dev_params_t dev_params;
    hid_host_dev_info_t dev_info;
    char interface[4];
    char device[4];
    char vendor[32];
    char product[32];
    char serial[32];
};

struct espusb_device_handle
{
    struct espusb_device *dev;
};

// typedef struct hid_host_device_handle_t libusb_device_handle;

/* driver description structure */
upsdrv_info_t comm_upsdrv_info = {
    USB_DRIVER_NAME,
    USB_DRIVER_VERSION,
    NULL,
    0,
    {NULL}};

// HID

size_t __wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
    int count = 0;

    if (n != 0)
    {
        do
        {
            uint16_t wc = *pwcs++;
            if (wc == 0)
                break;
            if (wc > 0xFF)
            {
                *s++ = '?';
            }
            else
            {
                *s++ = (uint8_t)wc;
            }
            count++;
        } while (--n != 0);
    }

    return count;
}

extern QueueHandle_t espusb_event_queue;

extern hid_host_device_handle_t espusb_hid_device_handle;

void nut_usb_addvars(void)
{
    addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
    addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
    addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

    addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
    addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

    addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
    addvar(VAR_VALUE, "device", "Regular expression to match USB device name");
    addvar(VAR_VALUE, "busport", "Regular expression to match USB bus port name");

    addvar(VAR_FLAG, "allow_duplicates",
           "If you have several UPS devices which may not be uniquely "
           "identified by options above, allow each driver instance with this "
           "option to take the first match if available, or try another "
           "(association of driver to device may vary between runs)");

    addvar(VAR_VALUE, "usb_set_altinterface", "Force redundant call to usb_set_altinterface() (value=bAlternateSetting; default=0)");

    addvar(VAR_VALUE, "usb_config_index", "Deeper tuning of USB communications for complex devices");
    addvar(VAR_VALUE, "usb_hid_rep_index", "Deeper tuning of USB communications for complex devices");
    addvar(VAR_VALUE, "usb_hid_desc_index", "Deeper tuning of USB communications for complex devices");
    addvar(VAR_VALUE, "usb_hid_ep_in", "Deeper tuning of USB communications for complex devices");
    addvar(VAR_VALUE, "usb_hid_ep_out", "Deeper tuning of USB communications for complex devices");

    dstate_setinfo("driver.version.usb", "espusb-%u.%u.%u", 0, 0, 1);
}

/* invoke matcher against device */
static inline int matches(USBDeviceMatcher_t *matcher, USBDevice_t *device)
{
    if (!matcher)
    {
        return 1;
    }
    return matcher->match_function(device, matcher->privdata);
}

static int nut_espusb_open(espusb_device_handle **udevp,
                           USBDevice_t *curDevice, USBDeviceMatcher_t *matcher,
                           int (*callback)(espusb_device_handle *udev,
                                           USBDevice_t *hd, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen))
{
    usb_host_lib_info_t lib_info;
    ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));

    if (espusb_hid_device_handle != NULL)
    {
        espusb_device *cur_device = (espusb_device *)calloc(1, sizeof(espusb_device));
        if (cur_device == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for device");
            return -1;
        }
        cur_device->parent_dev = espusb_hid_device_handle;

        ESP_ERROR_CHECK(hid_host_device_get_params(espusb_hid_device_handle, &cur_device->dev_params));
        ESP_ERROR_CHECK(hid_host_get_device_info(espusb_hid_device_handle, &cur_device->dev_info));

        snprintf(cur_device->interface, 4, "%03u", cur_device->dev_params.iface_num);
        snprintf(cur_device->device, 4, "%03u", cur_device->dev_params.addr);

        __wcstombs(cur_device->vendor, cur_device->dev_info.iManufacturer, 32);
        __wcstombs(cur_device->product, cur_device->dev_info.iProduct, 32);
        __wcstombs(cur_device->serial, cur_device->dev_info.iSerialNumber, 32);

        curDevice->Vendor = cur_device->vendor;
        curDevice->Product = cur_device->product;
        curDevice->Serial = cur_device->serial;

        curDevice->VendorID = cur_device->dev_info.VID;
        curDevice->ProductID = cur_device->dev_info.PID;

        curDevice->Bus = cur_device->interface;
        curDevice->Device = cur_device->device;
        curDevice->bcdDevice = 0x0100;

        upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
        upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
        upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
        upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
        upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
        upsdebugx(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
        upsdebugx(2, "- Bus Port: %s", curDevice->BusPort ? curDevice->BusPort : "unknown");
#endif
        upsdebugx(2, "- Device: %s", curDevice->Device ? curDevice->Device : "unknown");
        upsdebugx(2, "- Device release number: %04x", curDevice->bcdDevice);

        upsdebugx(2, "Trying to match device");

        USBDeviceMatcher_t *m;
        int ret, res;

        /* report descriptor */
        uint8_t *rdbuf;
        size_t rdlen;

        for (m = matcher; m; m = m->next)
        {
            ret = matches(m, curDevice);
            if (ret == 0)
            {
                upsdebugx(2, "Device does not match - skipping");
                return -1;
            }
            else if (ret == -1)
            {
                fatal_with_errno(EXIT_FAILURE, "matcher");
            }
            else if (ret == -2)
            {
                upsdebugx(2, "matcher: unspecified error");
                return -1;
            }
        }

        /* If we got here, none of the matchers said
         * that the device is not what we want. */
        upsdebugx(2, "Device matches");

        espusb_device_handle *curHandle = (espusb_device_handle *)malloc(sizeof(espusb_device_handle));
        if (curHandle == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for device handle");
            free(cur_device);
            return -1;
        }
        curHandle->dev = cur_device;

        rdbuf = hid_host_get_report_descriptor(
            espusb_hid_device_handle,
            &rdlen);

        res = callback(curHandle, curDevice, rdbuf, (usb_ctrl_charbufsize)rdlen);
        if (res < 1)
        {
            upsdebugx(2, "Caller doesn't like this device");
            free(curHandle);
            free(cur_device);
            return -1;
        }

        udevp[0] = curHandle;
    }
    else
    {
        ESP_LOGE(TAG, "No device attached");
    }

    return lib_info.num_devices;
}

static void nut_espusb_close(espusb_device_handle *udev)
{
    hid_host_device_close(udev->dev->parent_dev);
    free(udev->dev);
}

static int nut_espusb_get_report(
    espusb_device_handle *udev,
    usb_ctrl_repindex ReportId,
    usb_ctrl_charbuf raw_buf,
    usb_ctrl_charbufsize ReportSize)
{
    size_t report_length = ReportSize;

    ESP_ERROR_CHECK(hid_class_request_get_report(
        udev->dev->parent_dev,
        0x03,
        ReportId,
        raw_buf,
        &report_length));

    return report_length;
}

static int nut_espusb_set_report(
    espusb_device_handle *udev,
    usb_ctrl_repindex ReportId,
    usb_ctrl_charbuf raw_buf,
    usb_ctrl_charbufsize ReportSize)
{
    ESP_ERROR_CHECK(hid_class_request_set_report(
        udev->dev->parent_dev,
        0x03,
        ReportId,
        raw_buf,
        ReportSize));

    return 0;
}

/**
 * @brief Get a string descriptor from the USB device
 *
 * @note ESP32 stub implementation - not currently supported
 * @param udev USB device handle
 * @param StringIdx String descriptor index
 * @param buf Buffer to store the string
 * @param buflen Buffer length
 * @return 0 (stub implementation)
 */
static int nut_espusb_get_string(
    espusb_device_handle *udev,
    usb_ctrl_strindex StringIdx,
    char *buf,
    usb_ctrl_charbufsize buflen)
{
    (void)udev;
    (void)StringIdx;
    (void)buf;
    (void)buflen;
    return 0;
}

/**
 * @brief Get an interrupt transfer from the USB device
 *
 * @note ESP32 stub implementation - not currently supported
 * @param udev USB device handle
 * @param buf Buffer to store the data
 * @param bufsize Buffer size
 * @param timeout Timeout in milliseconds
 * @return 0 (stub implementation)
 */
static int nut_espusb_get_interrupt(
    espusb_device_handle *udev,
    usb_ctrl_charbuf buf,
    usb_ctrl_charbufsize bufsize,
    usb_ctrl_timeout_msec timeout)
{
    (void)udev;
    (void)buf;
    (void)bufsize;
    (void)timeout;
    return 0;
}

int espusb_get_string_descriptor(espusb_device_handle *dev_handle,
                                 uint8_t desc_index, uint16_t langid,
                                 unsigned char *data, int length)
{
    (void)dev_handle;
    (void)desc_index;
    (void)langid;
    if (data && length > 0) {
        data[0] = 0;
    }
    return 0;
}

usb_communication_subdriver_t usb_subdriver = {
    USB_DRIVER_NAME,
    USB_DRIVER_VERSION,
    nut_espusb_open,
    nut_espusb_close,
    nut_espusb_get_report,
    nut_espusb_set_report,
    nut_espusb_get_string,
    nut_espusb_get_interrupt,
    LIBUSB_DEFAULT_CONF_INDEX,
    LIBUSB_DEFAULT_INTERFACE,
    LIBUSB_DEFAULT_DESC_INDEX,
    LIBUSB_DEFAULT_HID_EP_IN,
    LIBUSB_DEFAULT_HID_EP_OUT};