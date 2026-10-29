#include <cflow/usb.h>
#include <salts/error_codes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LIBUSB_API_VERSION
#error "The USB public header must not expose private libusb headers"
#endif

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "USB C11 consumer failed at line %d: %s\n", \
                __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

#define REQUIRE_SIGNATURE(function, ...) \
    _Static_assert(_Generic(&(function), __VA_ARGS__: 1, default: 0), \
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

_Static_assert(sizeof(cflow_usb_context) == sizeof(void *), "context carrier ABI");
_Static_assert(sizeof(cflow_usb_device) == sizeof(void *), "device carrier ABI");
_Static_assert(sizeof(cflow_usb_transfer_id) == sizeof(uint64_t), "request ID ABI");

int main(void) {
    cflow_usb_context context = {0};
    cflow_usb_device device = {0};
    cflow_usb_device_identity identity = {0};
    cflow_usb_context_config config = {0};
    cflow_usb_transfer_request request = {0};
    cflow_usb_transfer_id id = 99u;
    cflow_usb_stats stats;
    cflow_usb_stats untouched;
    size_t count = 99u;
    size_t iteration;

    /* Every original exported function is actually called. Link-time garbage
     * collection cannot turn a missing symbol into a successful ABI check. */
    memset(&stats, 0x57, sizeof(stats));
    memcpy(&untouched, &stats, sizeof(stats));
    REQUIRE(cflow_usb_context_init(&context, &config) == SALTS_EINVAL);
    REQUIRE(context.impl == NULL);
    REQUIRE(cflow_usb_enumerate(&context, NULL, 0u, &count) == SALTS_EINVAL);
    REQUIRE(count == 99u);
    REQUIRE(cflow_usb_device_open(&context, &identity, &device) == SALTS_EINVAL);
    REQUIRE(device.impl == NULL);
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
    REQUIRE(memcmp(&stats, &untouched, sizeof(stats)) == 0);
    REQUIRE(cflow_usb_context_destroy(&context) == SALTS_EINVAL);

    config.device_capacity = 4096u;
    config.transfer_capacity = 2u;
    config.control_payload_capacity = 64u;
    config.hotplug_capacity = 2u;
    config.event_poll_timeout_ms = 1u;
    for (iteration = 0u; iteration < 3u; ++iteration) {
        int status;
        REQUIRE(cflow_usb_context_init(&context, &config) == SALTS_OK);
        REQUIRE(context.impl != NULL);
        count = 99u;
        status = cflow_usb_enumerate(&context, NULL, 0u, &count);
        REQUIRE(status == (count == 0u ? SALTS_OK : SALTS_ENOBUFS));
        REQUIRE(cflow_usb_get_stats(&context, &stats));
        REQUIRE(stats.enumerations == 1u);
        REQUIRE(stats.active_devices == 0u && stats.active_transfers == 0u);
        REQUIRE(cflow_usb_run_ready(&context, 1u, &count) == SALTS_OK);
        REQUIRE(count == 0u);
        REQUIRE(cflow_usb_context_destroy(&context) == SALTS_OK);
        REQUIRE(context.impl == NULL);
    }
    return EXIT_SUCCESS;
}
