//
//  libimobiledevice_screenshot_test.c
//  iOSVNCServer
//
//  Created by Michal Pietras on 28/06/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#include <stdio.h>
#include <time.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>

#define SAMPLE_SIZE 100

int main(void) {
    idevice_t device = NULL;
    lockdownd_client_t lckd = NULL;
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    screenshotr_client_t shotr = NULL;
    lockdownd_service_descriptor_t service = NULL;
    int i;
    time_t st, et;
    double dt;

    if (IDEVICE_E_SUCCESS != idevice_new(&device, NULL)) {
        printf("No device found, is it plugged in?\n");
        return -1;
    }

    if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(device, &lckd, NULL))) {
        idevice_free(device);
        printf("ERROR: Could not connect to lockdownd, error code %d\n", ldret);
        return -1;
    }

    lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", &service);
    lockdownd_client_free(lckd);
    if (service && service->port > 0) {
        if (screenshotr_client_new(device, service, &shotr) != SCREENSHOTR_E_SUCCESS) {
            printf("Could not connect to screenshotr!\n");
            return -1;
        } else {
            st = time(NULL);
            for (i = 0; i < SAMPLE_SIZE; ++i) {
                char *imgdata = NULL;
                uint64_t imgsize = 0;
                if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) != SCREENSHOTR_E_SUCCESS) {
                    printf("Could not get screenshot!\n");
                    return -1;
                } else {
                    printf("Screenshot #%03d is %lld bytes long.\n", i + 1, imgsize);
                }
            }
            et = time(NULL);
            dt = difftime(et, st);
            printf("Took %d screenshots in %lf second(s) (%lf screenshots/s).\n",
                   SAMPLE_SIZE,
                   dt,
                   (double)SAMPLE_SIZE / dt);

            screenshotr_client_free(shotr);
        }
    } else {
        printf("Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
        return -1;
    }
    
    if (service) {
        lockdownd_service_descriptor_free(service);
    }
    
    idevice_free(device);

    return 0;
}
