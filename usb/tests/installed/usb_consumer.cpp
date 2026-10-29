#include <cflow/usb.h>
#include <salts/error_codes.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#ifdef LIBUSB_API_VERSION
#error "The USB public header must not expose private libusb headers"
#endif

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "USB C++17 consumer failed at line %d: %s\n", \
                     __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (false)

#define REQUIRE_SIGNATURE(function, ...) \
    static_assert(std::is_same_v<decltype(&(function)), __VA_ARGS__>, \
                  #function " changed its public signature")

REQUIRE_SIGNATURE(cflow_usb_context_init,
    int (*)(cflow_usb_context *, const cflow_usb_context_config *));
REQUIRE_SIGNATURE(cflow_usb_enumerate,
    int (*)(cflow_usb_context *, cflow_usb_device_info *, size_t, size_t *));
REQUIRE_SIGNATURE(cflow_usb_device_open,
    int (*)(cflow_usb_context *, const cflow_usb_device_identity *, cflow_usb_device *));
REQUIRE_SIGNATURE(cflow_usb_device_set_configuration, int (*)(cflow_usb_device *, int));
REQUIRE_SIGNATURE(cflow_usb_device_claim_interface, int (*)(cflow_usb_device *, uint8_t));
REQUIRE_SIGNATURE(cflow_usb_device_release_interface, int (*)(cflow_usb_device *, uint8_t));
REQUIRE_SIGNATURE(cflow_usb_device_close, int (*)(cflow_usb_device *));
REQUIRE_SIGNATURE(cflow_usb_submit,
    int (*)(cflow_usb_context *, cflow_usb_device *,
            const cflow_usb_transfer_request *, cflow_usb_transfer_id *));
REQUIRE_SIGNATURE(cflow_usb_cancel, int (*)(cflow_usb_context *, cflow_usb_transfer_id));
REQUIRE_SIGNATURE(cflow_usb_run_ready, int (*)(cflow_usb_context *, size_t, size_t *));
REQUIRE_SIGNATURE(cflow_usb_acknowledge_hotplug_rescan, int (*)(cflow_usb_context *));
REQUIRE_SIGNATURE(cflow_usb_get_stats, bool (*)(const cflow_usb_context *, cflow_usb_stats *));
REQUIRE_SIGNATURE(cflow_usb_context_destroy, int (*)(cflow_usb_context *));

static_assert(sizeof(cflow_usb_context) == sizeof(void *));
static_assert(sizeof(cflow_usb_device) == sizeof(void *));
static_assert(std::is_standard_layout_v<cflow_usb_context>);
static_assert(std::is_standard_layout_v<cflow_usb_device>);
static_assert(std::is_standard_layout_v<cflow_usb_context_config>);
static_assert(std::is_standard_layout_v<cflow_usb_transfer_request>);
static_assert(std::is_standard_layout_v<cflow_usb_transfer_result>);

int main() {
    cflow_usb_context context{};
    cflow_usb_device device{};
    cflow_usb_context_config config{};
    cflow_usb_device_identity identity{};
    cflow_usb_transfer_request request{};
    cflow_usb_transfer_id id = 99u;
    cflow_usb_stats stats;
    cflow_usb_stats untouched;
    size_t count = 99u;

    // Calls resolve all thirteen unmangled C exports from the installed DSO.
    std::memset(&stats, 0x57, sizeof(stats));
    std::memcpy(&untouched, &stats, sizeof(stats));
    REQUIRE(cflow_usb_context_init(&context, &config) == SALTS_EINVAL);
    REQUIRE(context.impl == nullptr);
    REQUIRE(cflow_usb_enumerate(&context, nullptr, 0u, &count) == SALTS_EINVAL);
    REQUIRE(count == 99u);
    REQUIRE(cflow_usb_device_open(&context, &identity, &device) == SALTS_EINVAL);
    REQUIRE(device.impl == nullptr);
    REQUIRE(cflow_usb_device_set_configuration(&device, 1) == SALTS_EINVAL);
    REQUIRE(cflow_usb_device_claim_interface(&device, 0u) == SALTS_EINVAL);
    REQUIRE(cflow_usb_device_release_interface(&device, 0u) == SALTS_EINVAL);
    REQUIRE(cflow_usb_device_close(&device) == SALTS_EINVAL);
    REQUIRE(cflow_usb_submit(&context, &device, &request, &id) == SALTS_EINVAL);
    REQUIRE(id == CFLOW_USB_INVALID_TRANSFER_ID);
    REQUIRE(cflow_usb_cancel(&context, id) == SALTS_EINVAL);
    REQUIRE(cflow_usb_run_ready(&context, 1u, &count) == SALTS_EINVAL);
    REQUIRE(count == 99u);
    REQUIRE(cflow_usb_acknowledge_hotplug_rescan(&context) == SALTS_EINVAL);
    REQUIRE(!cflow_usb_get_stats(&context, &stats));
    REQUIRE(std::memcmp(&stats, &untouched, sizeof(stats)) == 0);
    REQUIRE(cflow_usb_context_destroy(&context) == SALTS_EINVAL);

    config.device_capacity = 4096u;
    config.transfer_capacity = 1u;
    config.control_payload_capacity = 64u;
    config.hotplug_capacity = 1u;
    config.event_poll_timeout_ms = 1u;
    REQUIRE(cflow_usb_context_init(&context, &config) == SALTS_OK);
    REQUIRE(cflow_usb_get_stats(&context, &stats));
    REQUIRE(stats.active_devices == 0u && stats.active_transfers == 0u);
    REQUIRE(cflow_usb_run_ready(&context, 1u, &count) == SALTS_OK);
    REQUIRE(count == 0u);
    REQUIRE(cflow_usb_context_destroy(&context) == SALTS_OK);
    REQUIRE(context.impl == nullptr);
    return EXIT_SUCCESS;
}
