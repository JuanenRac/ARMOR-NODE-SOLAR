// ARMOR-NODE-SOLAR - the USB host of an inverter's USB port, with Espressif's usb_host library and usb_host_hid component.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// Only the s3-wifi board: the s3-eth board has one socket, on the native USB, and no second way to flash it. A question goes out as SET_REPORT requests (output report, id 0) of 8 bytes and the
// answer comes in the input reports of the interrupt endpoint. A second client of the library only looks at whatever is plugged in, so the panel can tell the VID, PID and class of a device that is
// not HID. No hub: the inverter is plugged straight in. Nothing here has run on a board.
#include "usb_port.hpp"

#include "core/usb_hid_link.hpp"

#ifdef ARMOR_BOARD_S3_ETH

namespace armor::usbhost {
std::unique_ptr<ports::Port> open(std::uint16_t, std::uint16_t, std::string& error) { error = "pins"; return nullptr; }
bool connected() { return false; }
std::string device_id() { return std::string(); }
std::string diagnosis() { return std::string(); }
}  // namespace armor::usbhost

#else

#include <mutex>
extern "C" {
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"
}

namespace armor::usbhost {
namespace {
constexpr char kTag[] = "armor-usb";

std::mutex g_lock;   // guards the link, the handle and the wanted ids
usbhid::Link g_link;
hid_host_device_handle_t g_handle = nullptr;
std::uint16_t g_want_vid = 0, g_want_pid = 0;
bool g_started = false, g_failed = false;
std::mutex g_send_lock;   // a SET_REPORT is under way: the device is not closed meanwhile

void library_task(void* argument) {
  usb_host_config_t host{};
  host.intr_flags = ESP_INTR_FLAG_LEVEL1;
  const esp_err_t result = usb_host_install(&host);
  xTaskNotify(static_cast<TaskHandle_t>(argument), result == ESP_OK ? 1 : 2, eSetValueWithOverwrite);
  if (result != ESP_OK) { vTaskDelete(nullptr); return; }
  for (;;) {
    std::uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}

// ---- looking at whatever is plugged in (HID or not) ---------------------------------------------------------------------------

usb_host_client_handle_t g_client = nullptr;
std::uint8_t g_seen_address = 0;
std::uint8_t g_new_address = 0;
bool g_gone = false;

void client_event(const usb_host_client_event_msg_t* message, void*) {
  if (message->event == USB_HOST_CLIENT_EVENT_NEW_DEV) g_new_address = message->new_dev.address;
  else if (message->event == USB_HOST_CLIENT_EVENT_DEV_GONE) g_gone = true;
}

void look_at(std::uint8_t address) {
  usb_device_handle_t device = nullptr;
  if (usb_host_device_open(g_client, address, &device) != ESP_OK) return;
  usbhid::DeviceInfo info;
  info.present = true;
  const usb_device_desc_t* description = nullptr;
  if (usb_host_get_device_descriptor(device, &description) == ESP_OK && description != nullptr) {
    info.vid = description->idVendor;
    info.pid = description->idProduct;
    info.device_class = description->bDeviceClass;
  }
  const usb_config_desc_t* config = nullptr;
  if (usb_host_get_active_config_descriptor(device, &config) == ESP_OK && config != nullptr) {
    // the first interface descriptor (type 4) of the configuration
    const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(config);
    for (std::size_t at = 0; at + 9 <= config->wTotalLength && bytes[at] != 0; at += bytes[at]) {
      if (bytes[at + 1] != 0x04) continue;
      info.interface_class = bytes[at + 5];
      info.interface_subclass = bytes[at + 6];
      info.interface_protocol = bytes[at + 7];
      break;
    }
  }
  usb_host_device_close(g_client, device);
  ESP_LOGW(kTag, "a device was plugged in: %04x:%04x, class %02x, interface class %02x", info.vid, info.pid, info.device_class, info.interface_class);
  g_seen_address = address;
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_link.ready()) g_link.connected(info);   // the HID side may already have taken it
}

void watch_task(void*) {
  usb_host_client_config_t config{};
  config.is_synchronous = false;
  config.max_num_event_msg = 5;
  config.async.client_event_callback = client_event;
  if (usb_host_client_register(&config, &g_client) != ESP_OK) { vTaskDelete(nullptr); return; }
  for (;;) {
    usb_host_client_handle_events(g_client, pdMS_TO_TICKS(500));
    if (g_gone) {
      g_gone = false;
      g_seen_address = 0;
      std::lock_guard<std::mutex> guard(g_lock);
      if (g_handle == nullptr) g_link.disconnected();
    }
    if (g_new_address != 0) {
      const std::uint8_t address = g_new_address;
      g_new_address = 0;
      look_at(address);
    }
  }
}

// ---- the HID side ------------------------------------------------------------------------------------------------------------

void interface_event(hid_host_device_handle_t device, const hid_host_interface_event_t event, void*) {
  if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
    std::uint8_t data[64];
    std::size_t length = 0;
    if (hid_host_device_get_raw_input_report_data(device, data, sizeof data, &length) != ESP_OK) return;
    std::lock_guard<std::mutex> guard(g_lock);
    if (device == g_handle) g_link.on_report(data, length);
  } else if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
    ESP_LOGW(kTag, "the device was unplugged");
    {
      std::lock_guard<std::mutex> guard(g_lock);
      if (device == g_handle) { g_handle = nullptr; g_link.disconnected(); }
    }
    std::lock_guard<std::mutex> sending(g_send_lock);
    hid_host_device_close(device);
  } else if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_link.on_receive_error();
  }
}

void driver_event(hid_host_device_handle_t device, const hid_host_driver_event_t event, void*) {
  if (event != HID_HOST_DRIVER_EVENT_CONNECTED) return;
  hid_host_dev_params_t params{};
  if (hid_host_device_get_params(device, &params) != ESP_OK) return;
  {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_handle != nullptr) return;   // the first HID interface only
  }
  hid_host_device_config_t config{};
  config.callback = interface_event;
  if (hid_host_device_open(device, &config) != ESP_OK) { ESP_LOGW(kTag, "the HID device could not be opened"); return; }
  hid_host_dev_info_t about{};
  const bool known = hid_host_get_device_info(device, &about) == ESP_OK;
  std::uint16_t want_vid = 0, want_pid = 0;
  { std::lock_guard<std::mutex> guard(g_lock); want_vid = g_want_vid; want_pid = g_want_pid; }
  if (known && !usbhid::wanted(want_vid, want_pid, about.VID, about.PID)) {
    ESP_LOGW(kTag, "HID device %04x:%04x is not the one set (%04x:%04x)", about.VID, about.PID, want_vid, want_pid);
    hid_host_device_close(device);
    return;
  }
  if (hid_host_device_start(device) != ESP_OK) { ESP_LOGW(kTag, "the HID device could not be started"); hid_host_device_close(device); return; }
  std::lock_guard<std::mutex> guard(g_lock);
  usbhid::DeviceInfo info = g_link.info();
  info.present = true;
  info.hid = true;
  if (known) { info.vid = about.VID; info.pid = about.PID; }
  info.interface_class = 0x03;
  info.interface_subclass = params.sub_class;
  info.interface_protocol = params.proto;
  g_handle = device;
  g_link.connected(info);
  ESP_LOGW(kTag, "HID device %04x:%04x taken", info.vid, info.pid);
}

bool start_host() {
  if (g_started) return !g_failed;
  g_started = true;
  ESP_LOGW(kTag, "the native USB becomes a host now: no console and no flashing on that socket until the setting is turned off");
  g_failed = true;
  if (xTaskCreate(library_task, "usb-lib", 4096, xTaskGetCurrentTaskHandle(), 10, nullptr) != pdPASS) return false;
  std::uint32_t said = 0;
  if (xTaskNotifyWait(0, UINT32_MAX, &said, pdMS_TO_TICKS(2000)) != pdTRUE || said != 1) return false;
  if (xTaskCreate(watch_task, "usb-watch", 4096, nullptr, 5, nullptr) != pdPASS) return false;
  hid_host_driver_config_t driver{};
  driver.create_background_task = true;
  driver.task_priority = 6;
  driver.stack_size = 4096;
  driver.core_id = tskNO_AFFINITY;
  driver.callback = driver_event;
  if (hid_host_install(&driver) != ESP_OK) return false;
  g_failed = false;
  return true;
}

class UsbPort : public ports::Port {
 public:
  std::size_t read(std::uint8_t* out, std::size_t capacity, unsigned wait_ms) override {
    for (unsigned waited = 0;; waited += 10) {
      {
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_link.waiting() > 0) return g_link.read(out, capacity);
      }
      if (waited >= wait_ms) return 0;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  bool write(const std::uint8_t* data, std::size_t length) override {
    std::lock_guard<std::mutex> sending(g_send_lock);
    hid_host_device_handle_t device = nullptr;
    usbhid::Link cut;   // the reports are made outside the lock: a SET_REPORT waits for the device, and the input reports need the lock meanwhile
    {
      std::lock_guard<std::mutex> guard(g_lock);
      device = g_handle;
      if (device == nullptr) return false;
      g_link.discard();
      cut.connected(g_link.info());
    }
    const bool sent = cut.transmit(data, length, [device](const std::uint8_t* report, std::size_t size) {
      std::uint8_t copy[usbhid::kReportSize];
      std::copy(report, report + size, copy);
      return hid_class_request_set_report(device, HID_REPORT_TYPE_OUTPUT, 0, copy, size) == ESP_OK;
    });
    std::lock_guard<std::mutex> guard(g_lock);
    g_link.counters.reports_out += cut.counters.reports_out;
    g_link.counters.bytes_out += cut.counters.bytes_out;
    g_link.counters.send_failures += cut.counters.send_failures;
    return sent;
  }
  bool soft() const override { return false; }
  std::uint32_t overruns() const override { std::lock_guard<std::mutex> guard(g_lock); return g_link.counters.dropped; }
  std::uint32_t framing_errors() const override { return 0; }
  void discard() override { std::lock_guard<std::mutex> guard(g_lock); g_link.discard(); }
};

}  // namespace

std::unique_ptr<ports::Port> open(std::uint16_t vid, std::uint16_t pid, std::string& error) {
  { std::lock_guard<std::mutex> guard(g_lock); g_want_vid = vid; g_want_pid = pid; }
  if (!start_host()) { error = "driver"; return nullptr; }
  return std::make_unique<UsbPort>();
}

bool connected() { std::lock_guard<std::mutex> guard(g_lock); return g_link.ready(); }
std::string device_id() { std::lock_guard<std::mutex> guard(g_lock); return g_link.id_text(); }
std::string diagnosis() { std::lock_guard<std::mutex> guard(g_lock); return g_link.diagnosis(); }

}  // namespace armor::usbhost

#endif
