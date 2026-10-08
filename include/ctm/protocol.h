#pragma once

#if defined(_NTDDK_)
typedef UCHAR CTM_UINT8;
typedef USHORT CTM_UINT16;
typedef ULONG CTM_UINT32;
typedef ULONGLONG CTM_UINT64;
#else
#include <stdint.h>
typedef uint8_t CTM_UINT8;
typedef uint16_t CTM_UINT16;
typedef uint32_t CTM_UINT32;
typedef uint64_t CTM_UINT64;
#endif

#define CTM_MAX_DESCRIPTOR_BYTES (64u * 1024u)
#define CTM_MAX_PROFILE_ID 64u
#define CTM_MAX_PATH_CHARS 260u
#define CTM_MAX_BT_DEVICES 32u

// The shared-memory channel to a kernel driver that the first import
// described (CTM_SHARED_CHANNEL, its rings and its register call) is not used
// by this listener, which serves USB/IP instead, and its definitions were
// taken out (code review, 2026-10-05). The feature report size below kept its
// name from it.
#define CTM_SHARED_FEATURE_REPORT_BYTES 64u
#define CTM_USB_RESPONSE_DATA_BYTES 4096u

typedef enum CTM_STATUS_CODE {
    CTM_STATUS_OK = 0,
    CTM_STATUS_UNAVAILABLE = 1,
    CTM_STATUS_INVALID_DESCRIPTOR = 2,
    CTM_STATUS_ALREADY_PLUGGED = 3,
    CTM_STATUS_NOT_PLUGGED = 4,
    CTM_STATUS_BUFFER_TOO_SMALL = 5,
    CTM_STATUS_INTERNAL_ERROR = 6
} CTM_STATUS_CODE;

typedef enum CTM_USB_EVENT_TYPE {
    CTM_USB_EVENT_NONE = 0,
    CTM_USB_EVENT_HID_OUTPUT = 1,
    CTM_USB_EVENT_FEATURE_GET = 2,
    CTM_USB_EVENT_FEATURE_SET = 3,
    CTM_USB_EVENT_ISO_OUT = 4,
    CTM_USB_EVENT_CONTROL = 5,
    CTM_USB_EVENT_ISO_IN = 6
} CTM_USB_EVENT_TYPE;

typedef enum CTM_USB_RESPONSE_STATUS {
    CTM_USB_RESPONSE_SUCCESS = 0,
    CTM_USB_RESPONSE_STALL = 1
} CTM_USB_RESPONSE_STATUS;

typedef struct CTM_PLUGIN_REQUEST {
    char profile_id[CTM_MAX_PROFILE_ID];
    CTM_UINT32 device_descriptor_offset;
    CTM_UINT32 device_descriptor_length;
    CTM_UINT32 configuration_descriptor_offset;
    CTM_UINT32 configuration_descriptor_length;
    CTM_UINT32 hid_report_descriptor_offset;
    CTM_UINT32 hid_report_descriptor_length;
    CTM_UINT32 string_descriptor_offset;
    CTM_UINT32 string_descriptor_length;
    CTM_UINT8 descriptor_blob[CTM_MAX_DESCRIPTOR_BYTES];
} CTM_PLUGIN_REQUEST;

typedef struct CTM_PLUGIN_RESPONSE {
    CTM_UINT32 status;
    CTM_UINT32 instance_id;
    char message[128];
} CTM_PLUGIN_RESPONSE;

typedef struct CTM_PLUGOUT_REQUEST {
    CTM_UINT32 instance_id;
} CTM_PLUGOUT_REQUEST;

typedef struct CTM_INPUT_REPORT {
    CTM_UINT32 instance_id;
    CTM_UINT8 endpoint_address;
    CTM_UINT16 length;
    CTM_UINT8 data[1024];
} CTM_INPUT_REPORT;

typedef struct CTM_USB_EVENT {
    CTM_UINT32 instance_id;
    CTM_UINT32 event_type;
    CTM_UINT32 request_id;
    CTM_UINT8 endpoint_address;
    CTM_UINT8 report_id;
    CTM_UINT16 length;
    CTM_UINT8 data[4096];
} CTM_USB_EVENT;

typedef struct CTM_USB_RESPONSE {
    CTM_UINT32 request_id;
    CTM_UINT32 status;
    CTM_UINT16 length;
    CTM_UINT16 reserved;
    CTM_UINT8 data[CTM_USB_RESPONSE_DATA_BYTES];
} CTM_USB_RESPONSE;

typedef struct CTM_CONTROLLER_STATUS {
    CTM_UINT32 status;
    CTM_UINT32 active_instance_id;
    CTM_UINT32 is_plugged;
    char active_profile_id[CTM_MAX_PROFILE_ID];
} CTM_CONTROLLER_STATUS;
