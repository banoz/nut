#include "usb/usb_host.h"
#include "espusb.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#define CLIENT_NUM_EVENT_MSG 5

#define DEV_MAX_COUNT 128

static const char *TAG = "CLASS";


// HID

typedef struct
{
    /* HID Host - Device related info */
    struct
    {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

QueueHandle_t espusb_event_queue = NULL;

hid_host_device_handle_t espusb_hid_device_handle = NULL;

/**
 * @brief Start USB Host install and handle common USB host library events while app pin not low
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event)
    {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class)
        {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto)
            {
                //           hid_host_keyboard_report_callback(data, data_length);
            }
            else if (HID_PROTOCOL_MOUSE == dev_params.proto)
            {
                //           hid_host_mouse_report_callback(data, data_length);
            }
        }
        else
        {
            //       hid_host_generic_report_callback(data, data_length);
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%d' DISCONNECTED",
                 dev_params.proto);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%d' TRANSFER_ERROR",
                 dev_params.proto);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%d' Unhandled event",
                 dev_params.proto);
        break;
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED)
    {
        hid_host_dev_params_t dev_params;
        ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

        ESP_LOGI(TAG, "HID Device, protocol '%d' CONNECTED", dev_params.proto);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL};

        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));

        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));

        espusb_hid_device_handle = hid_device_handle;

        ESP_LOGI(TAG, "HID Device, protocol '%d' STARTED", dev_params.proto);
        return;
    }

    const app_event_queue_t evt_queue = {
        // HID Host Device related info
        .hid_host_device.handle = hid_device_handle,
        .hid_host_device.event = event,
        .hid_host_device.arg = arg};

    if (espusb_event_queue)
    {
        xQueueSend(espusb_event_queue, &evt_queue, 0);
    }
}

void hidHostInstall()
{
    BaseType_t task_created;

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
     * - Handle USB Host events while APP pin in in HIGH state
     */
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    /*
     * HID host driver configuration
     * - create background task for handling low level event inside the HID driver
     * - provide the device callback to get new HID Device connection event
     */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    vTaskDelay(1000);
}