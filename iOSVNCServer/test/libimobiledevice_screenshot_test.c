//
//  libimobiledevice_screenshot_test.c
//  iOSVNCServer
//
//  Created by Michal Pietras on 28/06/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>

#define SAMPLE_SIZE 100
#define SAVE_SCREENSHOTS

int main(void) {
    idevice_t device = NULL;
    lockdownd_client_t lckd = NULL;
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    screenshotr_client_t shotr = NULL;
    lockdownd_service_descriptor_t service = NULL;
    int i;
    time_t st, et;
    double dt;
#ifdef SAVE_SCREENSHOTS
    char *filename = NULL;
#endif

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
#ifdef SAVE_SCREENSHOTS
            filename = malloc(8 * sizeof(char));
#endif
            for (i = 0; i < SAMPLE_SIZE; ++i) {
                char *imgdata = NULL;
                uint64_t imgsize = 0;
                if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS) {
                    printf("Screenshot #%03d is %lld bytes long.\n", i + 1, imgsize);

#ifdef SAVE_SCREENSHOTS
                    sprintf(filename, "%03d.tiff", i + 1);
                    FILE *f = fopen(filename, "wb");
                    if (f) {
                        if (fwrite(imgdata, 1, (size_t)imgsize, f) == (size_t)imgsize) {
                            printf("Screenshot saved to %s.\n", filename);
                        } else {
                            printf("Could not save screenshot to file %s!\n", filename);
                            return -1;
                        }
                        fclose(f);
                    } else {
                        printf("Could not open %s for writing: %s\n", filename, strerror(errno));
                        return -1;
                    }
#endif
                } else {
                    printf("Could not get screenshot!\n");
                    return -1;
                }
            }
            et = time(NULL);
            dt = difftime(et, st);
            printf("Took %d screenshots in %lf second(s) (%lf screenshots/s).\n",
                   SAMPLE_SIZE,
                   dt,
                   (double)SAMPLE_SIZE / dt);

#ifdef SAVE_SCREENSHOTS
            free(filename);
#endif
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
