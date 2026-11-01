#include <cflow/usb.h>
#include <libusb.h>
#include <error_codes.h>
#include <thread.h>
#include <tinytest.h>

#include <string.h>

typedef struct completion_probe {
    size_t calls;
    cflow_usb_transfer_result result;
} completion_probe;

typedef struct hotplug_probe {
    size_t calls;
    cflow_usb_hotplug_kind kinds[2];
} hotplug_probe;

static const cflow_usb_device_identity present_device = {
    1u, 2u, 0x1234u, 0x5678u
};

static cflow_usb_context_config bounded_config(size_t transfers) {
    cflow_usb_context_config config = {0};
    config.device_capacity = 1u;
    config.transfer_capacity = transfers;
    config.control_payload_capacity = 16u;
    config.hotplug_capacity = 1u;
    config.event_poll_timeout_ms = 1u;
    return config;
}

static void record_completion(void *user,
                              const cflow_usb_transfer_result *result) {
    completion_probe *probe = (completion_probe *)user;
    ++probe->calls;
    probe->result = *result;
}

static void record_hotplug(void *user, const cflow_usb_hotplug_event *event) {
    hotplug_probe *probe = (hotplug_probe *)user;
    if (probe->calls < 2u)
        probe->kinds[probe->calls] = event->kind;
    ++probe->calls;
}

static void deliver(cflow_usb_context *context, size_t expected) {
    size_t total = 0u;
    size_t attempt;
    for (attempt = 0u; attempt < 2000u && total < expected; ++attempt) {
        size_t count = 0u;
        check_equal(cflow_usb_run_ready(context, expected - total, &count),
                    SALTS_OK);
        total += count;
        if (total < expected)
            cmeta_sleep_ms(1u);
    }
    check_equal(total, expected);
}

typedef struct callback_owner_probe {
    cflow_usb_context *context;
    cflow_usb_device *device;
    cflow_usb_transfer_request request;
    unsigned char guarded[10];
    size_t calls;
    cflow_usb_transfer_id previous_id;
    cflow_usb_transfer_id resubmitted_id;
} callback_owner_probe;

static void inspect_and_resubmit(void *user,
                                 const cflow_usb_transfer_result *result) {
    callback_owner_probe *probe = (callback_owner_probe *)user;
    cflow_usb_stats stats = {0};
    size_t reentrant_count = 99u;
    size_t index;
    ++probe->calls;
    check_equal(result->status, SALTS_OK);
    check_equal(result->kind, CFLOW_USB_TRANSFER_CONTROL);
    check_equal(result->bytes_transferred, (size_t)8u);
    check(result->id != probe->previous_id);
    probe->previous_id = result->id;
    check_equal(probe->guarded[0], (unsigned char)0x31u);
    check_equal(probe->guarded[9], (unsigned char)0x79u);
    for (index = 1u; index < 9u; ++index)
        check_equal(probe->guarded[index], (unsigned char)0xa5u);
    check_true(cflow_usb_get_stats(probe->context, &stats));
    check_equal(stats.active_transfers, (size_t)0u);
    check_equal(cflow_usb_run_ready(probe->context, 1u, &reentrant_count),
                SALTS_EBUSY);
    check_equal(reentrant_count, (size_t)0u);
    check_equal(cflow_usb_context_destroy(probe->context), SALTS_EBUSY);
    check_not_null(probe->context->impl);
    if (probe->calls == 1u) {
        /* The one slot is reusable from its terminal callback. The callback
         * stops accessing this buffer before handing it to the next request. */
        check_equal(cflow_usb_submit(probe->context, probe->device,
                    &probe->request, &probe->resubmitted_id), SALTS_OK);
        check(probe->resubmitted_id != result->id);
    }
}

spec("USB native owner lifetime") {
    it("retains exact identities and owners on bounded admission errors") {
        cflow_usb_context context = {0};
        cflow_usb_context_config config = bounded_config(1u);
        cflow_usb_device first = {0};
        cflow_usb_device rejected = {0};
        cflow_usb_device_identity missing = present_device;
        cflow_usb_device_info snapshot = {0};
        cflow_usb_stats stats = {0};
        void *owner;
        size_t count = 99u;

        config.control_payload_capacity = (size_t)UINT16_MAX + 1u;
        check_equal(cflow_usb_context_init(&context, &config), SALTS_EINVAL);
        check_null(context.impl);
        config = bounded_config(1u);
        check_equal(cflow_usb_context_init(&context, &config), SALTS_OK);
        owner = context.impl;
        check_equal(cflow_usb_context_init(&context, &config), SALTS_EINVAL);
        check(context.impl == owner);
        check_equal(cflow_usb_enumerate(&context, NULL, 0u, &count),
                    SALTS_ENOBUFS);
        check_equal(count, (size_t)1u);
        check_equal(cflow_usb_enumerate(&context, &snapshot, 1u, &count),
                    SALTS_OK);
        check_equal(snapshot.vendor_id, present_device.vendor_id);
        check_equal(snapshot.product_id, present_device.product_id);
        ++missing.product_id;
        check_equal(cflow_usb_device_open(&context, &missing, &rejected),
                    SALTS_ENODEV);
        check_null(rejected.impl);
        check_equal(cflow_usb_device_open(&context, &present_device, &first),
                    SALTS_OK);
        check_equal(cflow_usb_device_open(&context, &present_device, &rejected),
                    SALTS_ENOBUFS);
        check_null(rejected.impl);
        check_true(cflow_usb_get_stats(&context, &stats));
        check_equal(stats.active_devices, (size_t)1u);
        check_equal(cflow_usb_context_destroy(&context), SALTS_EBUSY);
        check(context.impl == owner);
        check_equal(cflow_usb_device_claim_interface(&first, 3u), SALTS_OK);
        check_equal(cflow_usb_device_set_configuration(&first, 1), SALTS_EBUSY);
        check_equal(cflow_usb_device_close(&first), SALTS_EBUSY);
        check_not_null(first.impl);
        check_equal(cflow_usb_device_release_interface(&first, 2u), SALTS_ENOENT);
        check_equal(cflow_usb_device_release_interface(&first, 3u), SALTS_OK);
        check_equal(cflow_usb_device_close(&first), SALTS_OK);
        check_null(first.impl);
        check_equal(cflow_usb_context_destroy(&context), SALTS_OK);
        check_null(context.impl);
    }

    it("copies control IN before callback and permits slot reuse without reentry") {
        cflow_usb_context context = {0};
        cflow_usb_device device = {0};
        cflow_usb_context_config config = bounded_config(1u);
        callback_owner_probe probe = {0};
        cflow_usb_transfer_id id;
        cflow_usb_stats stats = {0};

        check_equal(cflow_usb_context_init(&context, &config), SALTS_OK);
        check_equal(cflow_usb_device_open(&context, &present_device, &device),
                    SALTS_OK);
        probe.context = &context;
        probe.device = &device;
        probe.guarded[0] = 0x31u;
        probe.guarded[9] = 0x79u;
        probe.request.kind = CFLOW_USB_TRANSFER_CONTROL;
        probe.request.control.request_type = 0x80u;
        probe.request.buffer = &probe.guarded[1];
        probe.request.length = 8u;
        probe.request.complete = inspect_and_resubmit;
        probe.request.complete_user = &probe;
        fake_libusb_allow_completion(0);
        check_equal(cflow_usb_submit(&context, &device, &probe.request, &id),
                    SALTS_OK);
        check_equal(cflow_usb_device_close(&device), SALTS_EBUSY);
        fake_libusb_allow_completion(1);
        deliver(&context, 2u);
        check_equal(probe.calls, (size_t)2u);
        check_true(cflow_usb_get_stats(&context, &stats));
        check_equal(stats.active_transfers, (size_t)0u);
        check_equal(stats.transfers_delivered, (size_t)2u);
        check_equal(cflow_usb_cancel(&context, id), SALTS_ENOENT);
        check_equal(cflow_usb_device_close(&device), SALTS_OK);
        check_equal(cflow_usb_context_destroy(&context), SALTS_OK);
    }

    it("cancels lost devices even when the detailed hotplug queue is full") {
        cflow_usb_context context = {0};
        cflow_usb_device device = {0};
        cflow_usb_context_config config = bounded_config(1u);
        completion_probe transfer = {0};
        hotplug_probe hotplug = {0};
        unsigned char buffer[4] = {1u, 2u, 3u, 4u};
        cflow_usb_transfer_request request = {0};
        cflow_usb_transfer_id id;
        cflow_usb_transfer_id rejected_id = 99u;

        config.hotplug = record_hotplug;
        config.hotplug_user = &hotplug;
        check_equal(cflow_usb_context_init(&context, &config), SALTS_OK);
        check_equal(cflow_usb_device_open(&context, &present_device, &device),
                    SALTS_OK);
        check_equal(cflow_usb_device_claim_interface(&device, 1u), SALTS_OK);
        request.kind = CFLOW_USB_TRANSFER_INTERRUPT;
        request.endpoint = 0x81u;
        request.buffer = buffer;
        request.length = sizeof(buffer);
        request.complete = record_completion;
        request.complete_user = &transfer;
        fake_libusb_allow_completion(0);
        check_equal(cflow_usb_submit(&context, &device, &request, &id), SALTS_OK);
        fake_libusb_emit_hotplug(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED);
        fake_libusb_emit_hotplug(LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT);
        check_equal(cflow_usb_submit(&context, &device, &request, &rejected_id),
                    SALTS_ENODEV);
        check_equal(rejected_id, CFLOW_USB_INVALID_TRANSFER_ID);
        check_equal(cflow_usb_device_set_configuration(&device, 1), SALTS_ENODEV);
        check_equal(cflow_usb_device_claim_interface(&device, 2u), SALTS_ENODEV);
        check_equal(cflow_usb_device_release_interface(&device, 1u), SALTS_EBUSY);
        check_equal(cflow_usb_device_close(&device), SALTS_EBUSY);
        check_equal(cflow_usb_context_destroy(&context), SALTS_EBUSY);
        check_equal(cflow_usb_acknowledge_hotplug_rescan(&context), SALTS_EALREADY);
        check_equal(transfer.calls, (size_t)0u);
        deliver(&context, 3u);
        check_equal(transfer.calls, (size_t)1u);
        check_equal(transfer.result.id, id);
        check_equal(transfer.result.status, SALTS_ECANCELED);
        check_equal(transfer.result.kind, CFLOW_USB_TRANSFER_INTERRUPT);
        check_equal(hotplug.calls, (size_t)2u);
        check_equal(hotplug.kinds[0], CFLOW_USB_HOTPLUG_ARRIVED);
        check_equal(hotplug.kinds[1], CFLOW_USB_HOTPLUG_RESCAN_REQUIRED);
        check_equal(cflow_usb_acknowledge_hotplug_rescan(&context), SALTS_OK);
        check_equal(cflow_usb_device_release_interface(&device, 1u), SALTS_OK);
        check_equal(cflow_usb_device_close(&device), SALTS_OK);
        check_equal(cflow_usb_context_destroy(&context), SALTS_OK);
    }

    it("rolls back a native submit rejection without accepting the buffer") {
        enum { ACCEPTED = 16, REQUESTS = ACCEPTED + 1 };
        cflow_usb_context context = {0};
        cflow_usb_device device = {0};
        cflow_usb_context_config config = bounded_config(REQUESTS);
        completion_probe probes[REQUESTS] = {{0}};
        unsigned char buffers[REQUESTS][4] = {{0}};
        cflow_usb_transfer_request request = {0};
        cflow_usb_transfer_id ids[REQUESTS] = {0};
        cflow_usb_stats stats = {0};
        size_t index;

        check_equal(cflow_usb_context_init(&context, &config), SALTS_OK);
        check_equal(cflow_usb_device_open(&context, &present_device, &device),
                    SALTS_OK);
        request.kind = CFLOW_USB_TRANSFER_BULK;
        request.endpoint = 1u;
        request.length = sizeof(buffers[0]);
        request.complete = record_completion;
        fake_libusb_allow_completion(0);
        /* The unchanged frozen fake has sixteen native queue entries. The
         * seventeenth request reaches libusb and fails after slot reservation. */
        for (index = 0u; index < REQUESTS; ++index) {
            request.buffer = buffers[index];
            request.complete_user = &probes[index];
            ids[index] = 99u;
            check_equal(cflow_usb_submit(&context, &device, &request, &ids[index]),
                        index < ACCEPTED ? SALTS_OK : SALTS_EBUSY);
        }
        check_equal(ids[ACCEPTED], CFLOW_USB_INVALID_TRANSFER_ID);
        check_true(cflow_usb_get_stats(&context, &stats));
        check_equal(stats.active_transfers, (size_t)ACCEPTED);
        memset(buffers[ACCEPTED], 0x52, sizeof(buffers[ACCEPTED]));
        fake_libusb_allow_completion(1);
        deliver(&context, ACCEPTED);
        for (index = 0u; index < ACCEPTED; ++index) {
            check_equal(probes[index].calls, (size_t)1u);
            check_equal(probes[index].result.id, ids[index]);
            check_equal(probes[index].result.status, SALTS_OK);
        }
        check_equal(probes[ACCEPTED].calls, (size_t)0u);
        check_true(cflow_usb_get_stats(&context, &stats));
        check_equal(stats.active_transfers, (size_t)0u);
        check_equal(cflow_usb_submit(&context, &device, &request, &ids[ACCEPTED]),
                    SALTS_OK);
        deliver(&context, 1u);
        check_equal(probes[ACCEPTED].calls, (size_t)1u);
        check_equal(cflow_usb_device_close(&device), SALTS_OK);
        check_equal(cflow_usb_context_destroy(&context), SALTS_OK);
    }
}
