//
//  main.c
//  iOSVNCServer
//
//  Created by Michal Pietras on 28/06/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <png.h>

#include "iosscreenshot.h"

typedef enum {
    TapRecognitionStateNotRecognized = 0,
    TapRecognitionStateRecognizing
} TapRecognitionState;

typedef enum {
    DragRecognitionStateNotRecognized = 0,
    DragRecognitionStateRecognizing,
    DragRecognitionStateRecognized
} DragRecognitionState;

typedef struct {
    int lastX, lastY;
    TapRecognitionState tapRecognitionState;
    DragRecognitionState dragRecognitionState;
    int dragStartX, dragStartY;
} ClientData;

static int extract_png(png_bytep png, png_size_t png_size,
                       png_uint_32 *width, png_uint_32 *height,
                       png_bytep *raw, png_size_t *raw_size);

static enum rfbNewClientAction initClient(rfbClientPtr client);
static void deinitClient(rfbClientPtr client);

static void ptrHandler(int buttonMask, int x, int y, rfbClientPtr client);
static void kbdHandler(rfbBool down, rfbKeySym key, rfbClientPtr client);

static int recognizeTap(int buttonMask, int x, int y, ClientData *clientData);
static int recognizeDrag(int buttonMask, int x, int y, ClientData *clientData);

int main(int argc,char** argv) {
    rfbScreenInfoPtr rfbScreen;
    iosss_handle_t handle;
    void *imgData = NULL;
    size_t imgSize = 0;
    uint32_t width, height;
    void *rawData = NULL;
    size_t rawSize = 0;

    if (!(handle = iosss_create())) {
        fputs("ERROR: Cannot create an iosss_handle_t.\n", stderr);
        return -1;
    }
    if (iosss_take(handle, &imgData, &imgSize)) {
        fputs("ERROR: Cannot take a screenshot.\n", stderr);
        return -1;
    }
    if (extract_png(imgData, imgSize, &width, &height, (png_bytep *)&rawData, &rawSize)) {
        fputs("ERROR: Cannot decode PNG.\n", stderr);
        return -1;
    }
    free(imgData);

    if (!(rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4))) {
        fputs("ERROR: Cannot create a rfbScreenInfoPtr.\n", stderr);
        return -1;
    }
    rfbScreen->desktopName = "TestingBot";
    rfbScreen->alwaysShared = TRUE;
    rfbScreen->port = 5901;
    rfbScreen->newClientHook = initClient;
    rfbScreen->ptrAddEvent = ptrHandler;
    rfbScreen->kbdAddEvent = kbdHandler;

    if (!(rfbScreen->frameBuffer = malloc(rawSize))) {
        fputs("ERROR: Cannot allocate memory.\n", stderr);
        return -1;
    }
    memcpy(rfbScreen->frameBuffer, rawData, rawSize);
    free(rawData);

    rfbInitServer(rfbScreen);
    rfbRunEventLoop(rfbScreen, -1, TRUE);
    while (rfbIsActive(rfbScreen)) {
        if (iosss_take(handle, &imgData, &imgSize)) {
            fputs("ERROR: Cannot take a screenshot.\n", stderr);
            return -1;
        }
        if (extract_png(imgData, imgSize, &width, &height, (png_bytep *)&rawData, &rawSize)) {
            fputs("ERROR: Cannot decode PNG.\n", stderr);
            return -1;
        }
        free(imgData);
        if (width != rfbScreen->width ||
            height != rfbScreen->height) {
            fputs("ERROR: Unexpected change of the screen size.\n", stderr);
            return -1;
        }
        memcpy(rfbScreen->frameBuffer, rawData, rawSize);
        free(rawData);
        rfbMarkRectAsModified(rfbScreen, 0, 0, width, height);
    }

    return 0;
}

static int extract_png(png_bytep png, png_size_t png_size,
                       png_uint_32 *width, png_uint_32 *height,
                       png_bytep *raw, png_size_t *raw_size) {
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, png, png_size)) {
        image.format = PNG_FORMAT_RGBA;
        *raw_size = PNG_IMAGE_SIZE(image);
        *raw = malloc(*raw_size);
        if (*raw != NULL &&
            png_image_finish_read(&image, NULL, *raw, 0, NULL)) {
            *width = image.width;
            *height = image.height;
        } else {
            *width = 0;
            *height = 0;
            *raw_size = -1;
            free(*raw);
            *raw = NULL;
            return -1;
        }
        png_image_free(&image);
    } else {
        *width = 0;
        *height = 0;
        *raw_size = -1;
        *raw = NULL;
        return -1;
    }
    return 0;
}

static enum rfbNewClientAction initClient(rfbClientPtr client) {
    client->clientData = calloc(sizeof(ClientData), 1);
    client->clientGoneHook = deinitClient;
    return RFB_CLIENT_ACCEPT;
}

static void deinitClient(rfbClientPtr client) {
    free(client->clientData);
    client->clientData = NULL;
}

static void ptrHandler(int buttonMask, int x, int y, rfbClientPtr client) {
    ClientData *clientData = client->clientData;
    if (x >= 0 && x < client->screen->width &&
        y >= 0 && y < client->screen->height) {
        if (recognizeTap(buttonMask, x, y, clientData)) {
            puts("Tap Recognized");
        }
        if (recognizeDrag(buttonMask, x, y, clientData)) {
            puts("Drag Recognized");
        }

        clientData->lastX = x;
        clientData->lastY = y;
    }
}

static void kbdHandler(rfbBool down, rfbKeySym key, rfbClientPtr client) {
    if (down) {
        switch (key) {
            case XK_Return:
                puts("Return Key Pressed");
                break;
            case XK_BackSpace:
                puts("Back Space Key Pressed");
                break;
            default:
                printf("\"%c\" Key Pressed\n", key);
                break;
        }
    }
}

static int recognizeTap(int buttonMask, int x, int y, ClientData *clientData) {
    if (x == clientData->lastX &&
        y == clientData->lastY) {
        switch (clientData->tapRecognitionState) {
            case TapRecognitionStateNotRecognized:
                if (buttonMask) {
                    clientData->tapRecognitionState = TapRecognitionStateRecognizing;
                }
                break;
            case TapRecognitionStateRecognizing:
                if (!buttonMask) {
                    return 1;
                }
                break;
        }
    } else {
        clientData->tapRecognitionState = TapRecognitionStateNotRecognized;
    }
    return 0;
}

static int recognizeDrag(int buttonMask, int x, int y, ClientData *clientData) {
    if (buttonMask) {
        switch (clientData->dragRecognitionState) {
            case DragRecognitionStateNotRecognized:
                clientData->dragStartX = x;
                clientData->dragStartY = y;
                clientData->dragRecognitionState = DragRecognitionStateRecognizing;
                break;
            case DragRecognitionStateRecognizing:
                if (x != clientData->dragStartX ||
                    y != clientData->dragStartY) {
                    clientData->dragRecognitionState = DragRecognitionStateRecognized;
                }
                break;
            case DragRecognitionStateRecognized:
                break;
        }
    } else if (clientData->dragRecognitionState == DragRecognitionStateRecognized) {
        clientData->dragRecognitionState = DragRecognitionStateNotRecognized;
        return 1;
    } else {
        clientData->dragRecognitionState = DragRecognitionStateNotRecognized;
    }
    return 0;
}
