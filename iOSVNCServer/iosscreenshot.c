//
//  iosscreenshot.c
//  iOSVNCServer
//
//  Created by Michal Pietras on 10/07/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#include "iosscreenshot.h"

#include <stdio.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>

struct iosss_handle_t {
    idevice_t device;
    screenshotr_client_t screenshotr;
};

iosss_handle_t iosss_create(const char *UDID) {
    struct iosss_handle_t *handle_struct = malloc(sizeof(struct iosss_handle_t));
    if (IDEVICE_E_SUCCESS != idevice_new(&handle_struct->device, UDID)) {
        fputs("ERROR: No device found, is it plugged in?\n", stderr);
        return NULL;
    }

    if (screenshotr_client_start_service(handle_struct->device, &handle_struct->screenshotr, "iosscreenshot")
        != SCREENSHOTR_E_SUCCESS) {
        idevice_free(handle_struct->device);
        fputs("ERROR: Could not start screenshotr service.\n", stderr);
        return NULL;
    }

    return handle_struct;
}

int iosss_take(iosss_handle_t handle, void **imgdata, size_t *imgsize) {
    struct iosss_handle_t *handle_struct = handle;
    return (screenshotr_take_screenshot(handle_struct->screenshotr, (char **)imgdata, (uint64_t *)imgsize)
            == SCREENSHOTR_E_SUCCESS ? 0 : -1);
}

void iosss_free(iosss_handle_t handle) {
    struct iosss_handle_t *handle_struct = handle;
    screenshotr_client_free(handle_struct->screenshotr);
    idevice_free(handle_struct->device);
}
