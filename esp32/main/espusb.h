#pragma once

#include "usb/usb_host.h"
#include "usb/hid_host.h"

#include "nut_stdint.h" /* for uint16_t, UINT16_MAX, PRIsize, etc. */
#include "common.h" /* for fatalx() etc. */

#ifndef LIBUSB_SUCCESS
#define LIBUSB_SUCCESS 0
#define LIBUSB_ERROR_IO -1
#define LIBUSB_ERROR_INVALID_PARAM -2
#define LIBUSB_ERROR_ACCESS -3
#define LIBUSB_ERROR_NO_DEVICE -4
#define LIBUSB_ERROR_NOT_FOUND -5
#define LIBUSB_ERROR_BUSY -6
#define LIBUSB_ERROR_TIMEOUT -7
#define LIBUSB_ERROR_OVERFLOW -8
#define LIBUSB_ERROR_PIPE -9
#define LIBUSB_ERROR_INTERRUPTED -10
#define LIBUSB_ERROR_NO_MEM -11
#define LIBUSB_ERROR_NOT_SUPPORTED -12
#define LIBUSB_ERROR_OTHER -99
#define LIBUSB_DT_STRING 0x03
#endif

/*
typedef enum
{
  ACTION_OPEN_DEV = (1 << 0),
  ACTION_GET_DEV_INFO = (1 << 1),
  ACTION_GET_DEV_DESC = (1 << 2),
  ACTION_GET_CONFIG_DESC = (1 << 3),
  ACTION_GET_STR_DESC = (1 << 4),
  ACTION_CLOSE_DEV = (1 << 5),
} action_t;

typedef struct
{
  usb_host_client_handle_t client_hdl;
  uint8_t dev_addr;
  usb_device_handle_t dev_hdl;
  action_t actions;
} usb_device_t;
*/

typedef struct espusb_device_handle espusb_device_handle;

typedef struct espusb_device espusb_device;

/* Structures */
/* #define usb_dev_handle libusb_device_handle */
// typedef espusb_device_handle usb_dev_handle;
/* These typedefs are also named in libshut.h, so we can consistenly
 * handle the "ifdef SHUT_MODE" handling in libhid.c and some drivers.
 * These symbolic names are used in all the headers and are expected to
 * match binary code of object files at (monolithic) driver build time.
 *
 * The MIN/MAX definitions here are primarily to generalize range-check
 * code (especially if anything is done outside the libraries).
 * FIXME: It may make sense to constrain the limits to lowest common
 * denominator that should fit all of libusb-0.1, libusb-1.0 and libshut,
 * so that any build of the practical (driver) code knows to not exceed
 * any use-case.
 */
typedef uint8_t usb_ctrl_requesttype;
#define USB_CTRL_REQUESTTYPE_MIN 0
#define USB_CTRL_REQUESTTYPE_MAX UINT8_MAX

typedef uint8_t usb_ctrl_request;
#define USB_CTRL_REQUEST_MIN 0
#define USB_CTRL_REQUEST_MAX UINT8_MAX

typedef unsigned char usb_ctrl_endpoint;
#define USB_CTRL_ENDPOINT_MIN 0
#define USB_CTRL_ENDPOINT_MAX UCHAR_MAX

typedef uint16_t usb_ctrl_msgvalue;
#define USB_CTRL_MSGVALUE_MIN 0
#define USB_CTRL_MSGVALUE_MAX UINT16_MAX

typedef uint8_t usb_ctrl_cfgindex;
#define USB_CTRL_CFGINDEX_MIN 0
#define USB_CTRL_CFGINDEX_MAX UINT8_MAX

typedef uint16_t usb_ctrl_repindex;
#define USB_CTRL_REPINDEX_MIN 0
#define USB_CTRL_REPINDEX_MAX UINT16_MAX

typedef uint8_t usb_ctrl_strindex;
#define USB_CTRL_STRINDEX_MIN 0
#define USB_CTRL_STRINDEX_MAX UINT8_MAX

typedef uint8_t usb_ctrl_descindex;
#define USB_CTRL_DESCINDEX_MIN 0
#define USB_CTRL_DESCINDEX_MAX UINT8_MAX

typedef unsigned char *usb_ctrl_charbuf;
typedef unsigned char usb_ctrl_char;
#define USB_CTRL_CHAR_MIN 0
#define USB_CTRL_CHAR_MAX UCHAR_MAX

/* Here MIN/MAX should not matter much, type mostly used for casting */
typedef uint16_t usb_ctrl_charbufsize;
#define USB_CTRL_CHARBUFSIZE_MIN 0
#define USB_CTRL_CHARBUFSIZE_MAX UINT16_MAX
#define PRI_NUT_USB_CTRL_CHARBUFSIZE PRIu16

typedef unsigned int usb_ctrl_timeout_msec; /* in milliseconds */
                                            /* Note: there does not seem to be a standard type
                                             * for milliseconds, like there is an useconds_t */
#define USB_CTRL_TIMEOUTMSEC_MIN 0
#define USB_CTRL_TIMEOUTMSEC_MAX UINT_MAX

/* defines */
// #define USB_CLASS_PER_INTERFACE LIBUSB_CLASS_PER_INTERFACE
// #define USB_ENDPOINT_IN LIBUSB_ENDPOINT_IN
// #define USB_ENDPOINT_OUT LIBUSB_ENDPOINT_OUT
// #define USB_RECIP_ENDPOINT LIBUSB_RECIPIENT_ENDPOINT
// #define USB_RECIP_INTERFACE LIBUSB_RECIPIENT_INTERFACE
// #define USB_REQ_SET_DESCRIPTOR LIBUSB_REQUEST_SET_DESCRIPTOR
// #define USB_TYPE_CLASS LIBUSB_REQUEST_TYPE_CLASS
// #define USB_TYPE_VENDOR LIBUSB_REQUEST_TYPE_VENDOR

/* Codebase updated to use LIBUSB_* tokens:
# define ERROR_ACCESS		LIBUSB_ERROR_ACCESS
# define ERROR_BUSY			LIBUSB_ERROR_BUSY
# define ERROR_IO			LIBUSB_ERROR_IO
# define ERROR_NO_DEVICE	LIBUSB_ERROR_NO_DEVICE
# define ERROR_NOT_FOUND	LIBUSB_ERROR_NOT_FOUND
# define ERROR_OVERFLOW		LIBUSB_ERROR_OVERFLOW
# define ERROR_PIPE			LIBUSB_ERROR_PIPE
# define ERROR_TIMEOUT		LIBUSB_ERROR_TIMEOUT
# define ERROR_NO_MEM   LIBUSB_ERROR_NO_MEM
# define ERROR_INVALID_PARAM  LIBUSB_ERROR_INVALID_PARAM
# define ERROR_INTERRUPTED    LIBUSB_ERROR_INTERRUPTED
# define ERROR_NOT_SUPPORTED  LIBUSB_ERROR_NOT_SUPPORTED
# define ERROR_OTHER          LIBUSB_ERROR_OTHER
*/
/*
# define LIBUSB_ERROR_ACCESS        ESPUSB_ERROR_ACCESS
# define LIBUSB_ERROR_BUSY          ESPUSB_ERROR_BUSY
# define LIBUSB_ERROR_IO            ESPUSB_ERROR_IO
# define LIBUSB_ERROR_NO_DEVICE     ESPUSB_ERROR_NO_DEVICE
# define LIBUSB_ERROR_NOT_FOUND     ESPUSB_ERROR_NOT_FOUND
# define LIBUSB_ERROR_OVERFLOW      ESPUSB_ERROR_OVERFLOW
# define LIBUSB_ERROR_PIPE          ESPUSB_ERROR_PIPE
# define LIBUSB_ERROR_TIMEOUT       ESPUSB_ERROR_TIMEOUT
# define LIBUSB_ERROR_NO_MEM        ESPUSB_ERROR_NO_MEM
# define LIBUSB_ERROR_INVALID_PARAM ESPUSB_ERROR_INVALID_PARAM
# define LIBUSB_ERROR_INTERRUPTED   ESPUSB_ERROR_INTERRUPTED
# define LIBUSB_ERROR_NOT_SUPPORTED ESPUSB_ERROR_NOT_SUPPORTED
# define LIBUSB_ERROR_OTHER         ESPUSB_ERROR_OTHER
*/

/* Functions for which simple mappings seem to suffice (no build warnings emitted): */
#define usb_claim_interface espusb_claim_interface
#define usb_clear_halt espusb_clear_halt
#define usb_close espusb_close
#define usb_set_configuration espusb_set_configuration
#define usb_release_interface espusb_release_interface
#define usb_reset espusb_reset_device

/* FIXME: some original libusb1.c code cast the (int) argument
 * as (enum libusb_error) - should we force that in the macro? */
#define nut_usb_strerror(a) espusb_strerror(a)

int espusb_get_string_descriptor(espusb_device_handle *dev_handle,
                                 uint8_t desc_index, uint16_t langid, unsigned char *data, int length);