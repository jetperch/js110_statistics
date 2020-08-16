/*
 * Copyright 2020 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "js110_statistics.h"
#include "device_change_notifier.h"
#include "usb_def.h"
#include <stdbool.h>
#include <Windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <string.h> // memset


/**
 * Get statistics from all connected Joulescopes.
 *
 * Module implemented as system-wide singleton.
 * Only run one instance at a time per host computer.
 */

// #include <stdio.h>
// #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#define DEBUG_PRINTF(...)
#define DEVICE_INTERFACE_DETAIL_SIZE ((1024 / sizeof(uint64_t)) * sizeof(uint64_t))
#define DEVICE_COUNT_MAX (128)
static const DWORD CONTROL_PIPE_TIMEOUT_MS = 500;


static inline uint16_t buf_decode_u16(uint8_t * buffer) {
    return ((uint16_t) (buffer[0])) |
           (((uint16_t) (buffer[1])) << 8);
}
static inline uint32_t buf_decode_u32(uint8_t * buffer) {
    return (((uint32_t) buf_decode_u16(buffer))          ) |
           (((uint32_t) buf_decode_u16(buffer + 2)) << 16);
}
static inline uint64_t buf_decode_u64(uint8_t * buffer) {
    return (((uint64_t) buf_decode_u32(buffer))          ) |
           (((uint64_t) buf_decode_u32(buffer + 4)) << 32);
}
#define buf_decode_i16(buffer)  ((int16_t) buf_decode_u16(buffer))
#define buf_decode_i32(buffer)  ((int32_t) buf_decode_u32(buffer))
#define buf_decode_i64(buffer)  ((int64_t) buf_decode_u64(buffer))


static js110_statistics_cbk cbk_fn_ = 0;
static void * cbk_user_data_;
static HANDLE thread_;
static DWORD thread_id_;
static volatile bool thread_exit_ = false;
static volatile int device_change_ = 0;

/// The state of a single Joulescope device "slot" in the devices_ array.
enum device_state_e {
    ST_EMPTY,
    ST_PRESENT,
    ST_OPEN,
    ST_MISSING,
};

/**
 * @brief Store data associated with a single Joulescope instrument.
 */
struct device_s {
    int id;
    int32_t serial_number;
    HANDLE file;
    HANDLE winusb;
    enum device_state_e state;
    int mark;  // for scan & detect remove
    union {
        SP_DEVICE_INTERFACE_DETAIL_DATA_W s;
        // uint64_t rather than uint8_t to force alignment
        uint64_t data[DEVICE_INTERFACE_DETAIL_SIZE / sizeof(uint64_t)];
    } device_interface_detail;

    // The sensor-side statistics accumulate indefinitely.
    // We only want statistics over the duration of this program.
    // The following variables to allow collection from start and
    // resume if the instrument reboots (disconnects / reconnects).
    int resync;
    int64_t samples_total_offset;
    int64_t samples_total_accum;
    double charge_offset;
    double charge_accum;
    double energy_offset;
    double energy_accum;
};

/// Array to hold all possible connected Joulescopes.
static struct device_s devices_[DEVICE_COUNT_MAX];  // 0 is reserved for invalid

// The Joulescope WinUSB interface's GUID.
// {576d606f-f3de-4e4e-8a87-065b9fd21eb0}
static const GUID guid = {0x576d606f, 0xf3de, 0x4e4e, {0x8a, 0x87, 0x06, 0x5b, 0x9f, 0xd2, 0x1e, 0xb0}};

void on_device_change(void *cookie) {
    (void) cookie;
    device_change_ = 1;  // signal main loop to perform scan
}

static int device_lookup(SP_DEVICE_INTERFACE_DETAIL_DATA_W * dev_interface_detail) {
    for (int i = 1; i < DEVICE_COUNT_MAX; ++i) {
        if (ST_EMPTY != devices_[i].state) {
            if (0 == wcscmp(dev_interface_detail->DevicePath, devices_[i].device_interface_detail.s.DevicePath)) {
                return i;
            }
        }
    }
    return 0;  // not found
}

static int device_add(SP_DEVICE_INTERFACE_DETAIL_DATA_W * dev_interface_detail) {
    struct device_s * d;
    for (int i = 1; i < DEVICE_COUNT_MAX; ++i) {
        if (devices_[i].state) {
            DEBUG_PRINTF("device_add(%d) taken %d\n", i, devices_[i].state);
            continue;
        }
        d = &devices_[i];
        memset(d, 0, sizeof(*d));
        d->id = i;
        d->state = ST_PRESENT;
        memcpy(d->device_interface_detail.data, dev_interface_detail, DEVICE_INTERFACE_DETAIL_SIZE);
        DEBUG_PRINTF("device_add(%ls)\n", dev_interface_detail->DevicePath);
        return i;
    }
    DEBUG_PRINTF("Could not add device: %ls\n", dev_interface_detail->DevicePath);
    return 0;
}

static int device_close(int dev_id) {
    if ((dev_id <= 0) || (dev_id > DEVICE_COUNT_MAX)) {
        DEBUG_PRINTF("dev_id out of range: %d\n", dev_id);
        return 1;
    }

    if (ST_OPEN != devices_[dev_id].state) {
        return 0;
    }

    struct device_s * d = &devices_[dev_id];
    d->state = ST_MISSING;

    if (d->winusb) {
        WinUsb_Free(d->winusb);
        d->winusb = 0;
    }
    if (d->file) {
        CloseHandle(d->file);
        d->file = 0;
    }

    return 0;
}

static char * str_next_section(char * ch) {
    while (ch && *ch) {
        if (*ch == '#') {
            return ch;
        }
        ++ch;
    }
    return 0;
}

// CAUTION: parsing the device path is not recommended by Microsoft.
// However, it's also the easiest (only?) way to get the device serial
// number without opening the device.
static int32_t extract_serial_number(char * str) {
    char * start = str_next_section(str);
    if (!start) {
        return 0;
    }
    start = str_next_section(start + 1);
    if (!start) {
        return 0;
    }
    ++start;
    char * end = str_next_section(start);
    if (!end) {
        return 0;
    }
    *end = 0;
    return atoi(start);
}

static int device_open_(int dev_id) {
    char device_str[DEVICE_INTERFACE_DETAIL_SIZE];

    ULONG length_transferred = 0;
    struct device_s * d = &devices_[dev_id];
    d->file = CreateFileW(
            d->device_interface_detail.s.DevicePath,
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            NULL);
    if (d->file == INVALID_HANDLE_VALUE) {
        DEBUG_PRINTF("device_open_ Device CreateFileW failed\n");
        return 1;
    }

    if (!WinUsb_Initialize(d->file, &d->winusb)) {
        DEBUG_PRINTF("device_open_ Device WinUsb_Initialize failed\n");
        device_close(dev_id);
        return 1;
    }

    // Reduce control endpoint timeout
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification
    if (!WinUsb_SetPipePolicy(d->winusb, 0, PIPE_TRANSFER_TIMEOUT,
                              sizeof(CONTROL_PIPE_TIMEOUT_MS), (void *) &CONTROL_PIPE_TIMEOUT_MS)) {
        DEBUG_PRINTF("WinUsb_SetPipePolicy failed\n");
    }
    wcstombs_s(0, device_str, sizeof(device_str), d->device_interface_detail.s.DevicePath, _TRUNCATE);
    DEBUG_PRINTF("device_open(%s)\n", device_str);
    d->serial_number = extract_serial_number(device_str);
    d->resync = 1;

    // Configure the Joulescope for normal operation.
    WINUSB_SETUP_PACKET setup_pkt;
    setup_pkt.RequestType = USB_REQUEST_TYPE(DEVICE, VENDOR, OUT);
    setup_pkt.Request = JS110_USBREQ_SETTINGS;
    setup_pkt.Value = 0;
    setup_pkt.Index = 0;
    setup_pkt.Length = 0;

    uint8_t pkt[16];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 1;     // packet format version
    pkt[1] = 16;    // length (bytes)
    pkt[2] = 1;     // settings
    pkt[8] = 1;     // sensor power on
    pkt[9] = 0x80;  // auto current ranging
    pkt[10] = 0xC0; // normal operation
    pkt[11] = 0x00; // 15V range
    pkt[12] = 0x00; // no streaming
    if (WinUsb_ControlTransfer(d->winusb, setup_pkt, pkt, sizeof(pkt), &length_transferred, 0)) {
        d->state = ST_OPEN;
        return 0;
    } else {
        DEBUG_PRINTF("WinUsb_ControlTransfer settings failed\n");
        return 1;
    }
}

static int device_open(int dev_id) {
    if ((dev_id <= 0) || (dev_id > DEVICE_COUNT_MAX)) {
        DEBUG_PRINTF("dev_id out of range: %d\n", dev_id);
        return 1;
    }

    switch (devices_[dev_id].state) {
        case ST_EMPTY:
            DEBUG_PRINTF("device_open(%d) but not found\n", dev_id);
            return 2;
        case ST_OPEN:
            DEBUG_PRINTF("device_open(%d) duplicate\n", dev_id);
            return 0;
        case ST_MISSING:  /* intentional fall-through */
        case ST_PRESENT:
            return device_open_(dev_id);
        default:
            DEBUG_PRINTF("device_open(%d) but invalid state %d\n", dev_id, devices_[dev_id].state);
            return 1;
    }
}

int js110_scan(void) {
    DWORD member_index = 0;
    int device_id = 0;
    uint64_t data[DEVICE_INTERFACE_DETAIL_SIZE / sizeof(uint64_t)];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W * dev_interface_detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *) data;
    SP_DEVICE_INTERFACE_DATA dev_interface;
    HANDLE handle = SetupDiGetClassDevsW(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!handle) {
        return 1;
    }

    for (int i = 1; i < DEVICE_COUNT_MAX; ++i) {
        devices_[i].mark = 0;  // clear
    }

    memset(&dev_interface, 0, sizeof(dev_interface));
    dev_interface.cbSize = sizeof(dev_interface);
    while (SetupDiEnumDeviceInterfaces(handle, NULL, &guid, member_index, &dev_interface)) {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailW(
                handle,
                &dev_interface,
                0, 0, &required_size, 0);
        memset(dev_interface_detail, 0, DEVICE_INTERFACE_DETAIL_SIZE);
        dev_interface_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(
                handle,
                &dev_interface,
                dev_interface_detail, required_size, &required_size, 0)) {
            DEBUG_PRINTF("SetupDiGetDeviceInterfaceDetailW failed\n");
            continue;
        }
        // DEBUG_PRINTF("scan %d: found %ls, %d\n", member_index, dev_interface_detail->DevicePath, required_size);

        device_id = device_lookup(dev_interface_detail);
        if (!device_id) {
            // New device, never seen before.
            device_id = device_add(dev_interface_detail);
            device_open(device_id);
        } else if (ST_MISSING == devices_[device_id].state) {
            // Known device, must have disconnected, but now reconnecting.
            device_open(device_id);
        }
        devices_[device_id].mark = 1;
        ++member_index;
    }
    SetupDiDestroyDeviceInfoList(handle);

    for (int i = 1; i < DEVICE_COUNT_MAX; ++i) {
        if (!devices_[i].mark) {  // unmarked, device removed
            device_close(i);
        }
    }

    return 0;
}

int js110_statistics(int dev_id) {
    uint8_t pkt[128];
    ULONG length_transferred = 0;
    struct js110_statistics_s statistics;
    memset(&statistics, 0, sizeof(statistics));
    if ((dev_id <= 0) || (dev_id > DEVICE_COUNT_MAX)) {
        DEBUG_PRINTF("dev_id out of range: %d\n", dev_id);
        return 1;
    }
    struct device_s * d = &devices_[dev_id];
    if (d->state != ST_OPEN) {
        return 0;
    }

    // Request statistics from the Joulescope instrumnet
    WINUSB_SETUP_PACKET setup_pkt;
    setup_pkt.RequestType = USB_REQUEST_TYPE(DEVICE, VENDOR, IN);
    setup_pkt.Request = JS110_USBREQ_STATUS;
    setup_pkt.Value = 0;
    setup_pkt.Index = 0;
    setup_pkt.Length = 0;

    if (!WinUsb_ControlTransfer(d->winusb, setup_pkt, pkt, sizeof(pkt), &length_transferred, 0)) {
        DEBUG_PRINTF("WinUsb_ControlTransfer settings failed\n");
        return 1;
    }
    if (104 != length_transferred) {
        DEBUG_PRINTF("unexpected length = %lu\n", length_transferred);
        return 1;
    }

    statistics.samples_this = buf_decode_i32(pkt + 56);
    if (0 == statistics.samples_this) {
        return 0;  // no new statistics available
    }

    // parse statistics message
    statistics.serial_number = d->serial_number;
    statistics.samples_total = buf_decode_u64(pkt + 24);
    statistics.power_mean = ((double) buf_decode_i64(pkt + 32)) / (1LLU << 34);
    statistics.charge = ((double) buf_decode_i64(pkt + 40)) / (1LLU << 27);
    statistics.energy = ((double) buf_decode_i64(pkt + 48)) / (1LLU << 27);
    statistics.samples_per_update = buf_decode_i32(pkt + 60);
    statistics.samples_per_second = buf_decode_i32(pkt + 64);
    statistics.current_mean = ((double) buf_decode_i32(pkt + 68)) / (1LU << 27);
    statistics.current_min = ((double) buf_decode_i32(pkt + 72)) / (1LU << 27);
    statistics.current_max = ((double) buf_decode_i32(pkt + 76)) / (1LU << 27);
    statistics.voltage_mean = ((double) buf_decode_i32(pkt + 80)) / (1LU << 17);
    statistics.voltage_min = ((double) buf_decode_i32(pkt + 84)) / (1LU << 17);
    statistics.voltage_max = ((double) buf_decode_i32(pkt + 88)) / (1LU << 17);
    statistics.power_min = ((double) buf_decode_i32(pkt + 92)) / (1LU << 21);
    statistics.power_max = ((double) buf_decode_i32(pkt + 96)) / (1LU << 21);

    // Adjust accumulated values.
    // Zero on first sample after program starts.
    // Continue accumulation following device reboot (disconnect / reconnect).
    if (d->resync) {
        d->samples_total_offset = statistics.samples_total - d->samples_total_accum;
        d->charge_offset = statistics.charge - d->charge_accum;
        d->energy_offset = statistics.energy - d->energy_accum;
        d->resync = 0;
    }
    statistics.samples_total -= d->samples_total_offset;
    statistics.charge -= d->charge_offset;
    statistics.energy -= d->energy_offset;
    d->samples_total_accum = statistics.samples_total;
    d->charge_accum = statistics.charge;
    d->energy_accum = statistics.energy;

    if (cbk_fn_) {
        cbk_fn_(cbk_user_data_, &statistics);
    }

    return 0;
}

static DWORD WINAPI js110_thread(LPVOID lpParam) {
    (void) lpParam;
    DEBUG_PRINTF("js110_thread start\n");
    device_change_ = 1;
    int rc = js110_device_change_notifier_initialize(on_device_change, 0);
    if (rc) {
        DEBUG_PRINTF("js110_device_change_notifier_initialize returned %d\n", rc);
    }
    while (!thread_exit_) {
        for (int i = 1; i < DEVICE_COUNT_MAX; ++i) {
            if (devices_[i].state == ST_OPEN) {
                js110_statistics(i);
            }
        }

        if (device_change_) {
            DEBUG_PRINTF("js110_scan\n");
            device_change_ = 0;
            js110_scan();
        }
        Sleep(100);
    }
    js110_device_change_notifier_finalize();
    DEBUG_PRINTF("js110_thread exit\n");
    return 0;
}

int js110_initialize(js110_statistics_cbk cbk_fn, void * cbk_user_data) {
    if (cbk_fn_) {
        js110_finalize();
    }
    if (!cbk_fn) {
        return 1;
    }

    memset(devices_, 0, sizeof(devices_));
    cbk_user_data_ = cbk_user_data;
    cbk_fn_ = cbk_fn;
    thread_ = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            js110_thread,           // thread function name
            NULL,                   // argument to thread function
            0,                      // use default creation flags
            &thread_id_);           // returns the thread identifier
    if (thread_ == NULL) {
        DEBUG_PRINTF("js110_initialize could not create thread\n");
        cbk_fn_ = 0;
        return 1;
    }

    return 0;
}

int js110_finalize(void) {
    thread_exit_ = true;
    if (thread_) {
        if (WAIT_OBJECT_0 != WaitForSingleObject(thread_, 1000)) {
            DEBUG_PRINTF("thread - not closed cleanly.\n");
        }
        CloseHandle(thread_);
        thread_ = 0;
    }

    cbk_fn_ = 0;
    cbk_user_data_ = 0;
    return 0;
}
