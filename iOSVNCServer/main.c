//
//  main.c
//  iOSVNCServer
//
//  Created by Michal Pietras on 28/06/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <time.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <png.h>
#include <curl/curl.h>

#include "iosscreenshot.h"

#define DEFAULT_PORT 5901
#define DEFAULT_SCALE_FACTOR 2.0

#define SCALE(coordinate) (int)((float)(coordinate) / screenData->scaleFactor)

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
    CURL *curlHandle;
    struct curl_slist *curlHeaders;
    char *tapURL;
    char *dragURL;
    char *keyURL;
    float scaleFactor;
    long int httpPort;
} ScreenData;

typedef struct {
    int lastX, lastY;
    TapRecognitionState tapRecognitionState;
    DragRecognitionState dragRecognitionState;
    int dragStartX, dragStartY;
} ClientData;

char keyboardChars[200] = "";

static int parseArgs(int argc, char **argv,
                     const char **UDID,
                     long int *port,
                     const char **HTTPHost, long int *HTTPPort, const char **HTTPSessionID,
                     float *scaleFactor);

static int extract_png(png_bytep png, png_size_t png_size,
                       png_uint_32 *width, png_uint_32 *height,
                       png_bytep *raw, png_size_t *raw_size);

static enum rfbNewClientAction initClient(rfbClientPtr client);
static void deinitClient(rfbClientPtr client);

static void ptrHandler(int buttonMask, int x, int y, rfbClientPtr client);
static void kbdHandler(rfbBool down, rfbKeySym key, rfbClientPtr client);
static void *keyboardQueuer();

static int recognizeTap(int buttonMask, int x, int y, ClientData *clientData);
static int recognizeDrag(int buttonMask, int x, int y, ClientData *clientData);

static char *createURL(const char *host, const char *sessionID, const char *actionPath);

int main(int argc, char **argv) {
    const char *UDID = NULL;
    long int port = DEFAULT_PORT;
    const char *HTTPHost = NULL;
    long int HTTPPort = -1;
    const char *HTTPSessionID = NULL;
    float scaleFactor = DEFAULT_SCALE_FACTOR;

    rfbScreenInfoPtr rfbScreen;
    iosss_handle_t handle;
    void *imgData = NULL;
    size_t imgSize = 0;
    uint32_t width, height;
    void *rawData = NULL;
    size_t rawSize = 0;
    ScreenData *screenData;

    if (parseArgs(argc, argv,
                  &UDID,
                  &port,
                  &HTTPHost, &HTTPPort, &HTTPSessionID,
                  &scaleFactor)) {
        exit(-1);
    }

    if (!(handle = iosss_create(UDID))) {
        fputs("ERROR: Cannot create an iosss_handle_t.\n", stderr);
        exit(-1);
    }
    if (iosss_take(handle, &imgData, &imgSize)) {
        fputs("ERROR: Cannot take a screenshot.\n", stderr);
        exit(-1);
    }
    if (extract_png(imgData, imgSize, &width, &height, (png_bytep *)&rawData, &rawSize)) {
        fputs("ERROR: Cannot decode PNG.\n", stderr);
        exit(-1);
    }
    free(imgData);

    if (!(rfbScreen = rfbGetScreen(&argc, argv, width, height, 8, 3, 4))) {
        fputs("ERROR: Cannot create a rfbScreenInfoPtr.\n", stderr);
        exit(-1);
    }
    rfbScreen->desktopName = "TestingBot";
    rfbScreen->alwaysShared = TRUE;
    rfbScreen->port = port;
    rfbScreen->newClientHook = initClient;
    rfbScreen->ptrAddEvent = ptrHandler;
    rfbScreen->kbdAddEvent = kbdHandler;

    if (!(rfbScreen->frameBuffer = malloc(rawSize))) {
        fputs("ERROR: Cannot allocate memory.\n", stderr);
        exit(-1);
    }
    memcpy(rfbScreen->frameBuffer, rawData, rawSize);
    free(rawData);

    if (!(screenData = malloc(sizeof(ScreenData)))) {
        fputs("ERROR: Cannot allocate memory.\n", stderr);
        exit(-1);
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fputs("ERROR: Cannot initialize libcurl.\n", stderr);
        exit(-1);
    }
    if (!(screenData->curlHandle = curl_easy_init())){
        fputs("ERROR: Cannot create a libcurl handle.\n", stderr);
        exit(-1);
    }
    if (!(screenData->curlHeaders = curl_slist_append(NULL, "Content-Type: application/json"))) {
        fputs("ERROR: Cannot initialize HTTP headers.\n", stderr);
        exit(-1);
    }
    if (curl_easy_setopt(screenData->curlHandle, CURLOPT_HTTPHEADER, screenData->curlHeaders) != CURLE_OK) {
        fputs("ERROR: Cannot set HTTP headers.\n", stderr);
        exit(-1);
    }
    if (curl_easy_setopt(screenData->curlHandle, CURLOPT_PORT, HTTPPort) != CURLE_OK) {
        fputs("ERROR: Cannot set a port.\n", stderr);
        exit(-1);
    }

    if (!(screenData->tapURL = createURL(HTTPHost, HTTPSessionID, "tap/0"))) {
        fputs("ERROR: Cannot create a tap URL.\n", stderr);
        exit(-1);
    }
    if (!(screenData->dragURL = createURL(HTTPHost, HTTPSessionID, "uiaTarget/0/dragfromtoforduration"))){
        fputs("ERROR: Cannot create a drag URL.\n", stderr);
        exit(-1);
    }
    if (!(screenData->keyURL = createURL(HTTPHost, HTTPSessionID, "keys"))) {
        fputs("ERROR: Cannot create a keys URL.\n", stderr);
        exit(-1);
    }
    screenData->scaleFactor = scaleFactor;
    rfbScreen->screenData = screenData;
    fprintf(stderr, "Current key %s", screenData->keyURL);
    screenData->httpPort = HTTPPort;
    
    pthread_t tid;
    pthread_create(&tid, NULL, &keyboardQueuer, screenData);
    
    rfbInitServer(rfbScreen);
    rfbRunEventLoop(rfbScreen, -1, TRUE);
    while (rfbIsActive(rfbScreen)) {
        if (iosss_take(handle, &imgData, &imgSize)) {
            fputs("ERROR: Cannot take a screenshot.\n", stderr);
            exit(-1);
        }
        if (extract_png(imgData, imgSize, &width, &height, (png_bytep *)&rawData, &rawSize)) {
            fputs("ERROR: Cannot decode PNG.\n", stderr);
            exit(-1);
        }
        free(imgData);
        if (width != rfbScreen->width ||
            height != rfbScreen->height) {
            fputs("ERROR: Unexpected change of the screen size.\n", stderr);
            exit(-1);
        }
        memcpy(rfbScreen->frameBuffer, rawData, rawSize);
        free(rawData);
        rfbMarkRectAsModified(rfbScreen, 0, 0, width, height);
    }

    free(screenData->tapURL);
    screenData->tapURL = NULL;
    free(screenData->dragURL);
    screenData->dragURL = NULL;
    free(screenData->keyURL);
    screenData->keyURL = NULL;
    curl_slist_free_all(screenData->curlHeaders);
    curl_easy_cleanup(screenData->curlHandle);
    free(screenData);
    screenData = NULL;
    curl_global_cleanup();

    exit(0);
}

static void printHelp() {
    fputs("\nArguments below with * are required!\n\n-u *               UDID of the device\n-p                 VNC Port 5901 by default\n-H *               HttpHost that does the interactions\n-P *               HttpPort that does the interactions\n-S *               HttpSessionID\n-s                 ScaleFactor, default 2\n", stderr);
}

static int parseArgs(int argc, char **argv,
                     const char **UDID,
                     long int *port,
                     const char **HTTPHost, long int *HTTPPort, const char **HTTPSessionID,
                     float *scaleFactor) {
    struct option longOptions[] = {
        {"UDID",          required_argument, NULL, 'u'},
        {"port",          required_argument, NULL, 'p'},
        {"HTTPHost",      required_argument, NULL, 'H'},
        {"HTTPPort",      required_argument, NULL, 'P'},
        {"HTTPSessionID", required_argument, NULL, 'S'},
        {"scaleFactor",   required_argument, NULL, 's'},
        {0, 0, 0, 0}
    };
    int option;
    int option_index = 0;

    while ((option = getopt_long(argc, argv,
                                 "u:p:H:P:S:s:",
                                 longOptions, &option_index)) != -1) {
        switch (option) {
            case 'u':
                *UDID = optarg;
                break;
            case 'p':
                *port = atol(optarg);
                break;
            case 'H':
                *HTTPHost = optarg;
                break;
            case 'P':
                *HTTPPort = atol(optarg);
                break;
            case 'S':
                *HTTPSessionID = optarg;
                break;
            case 's':
                *scaleFactor = atof(optarg);
                break;
            case '?':
            case 'h':
                printHelp();
                return -1;
            default:
                fputs("ERROR: Arguments parsing error.\n", stderr);
                return -1;
        }
    }

    if (*HTTPPort == -1) {
        fputs("Invalid arguments! Please see the required arguments:\n", stderr);
        printHelp();
        exit(-1);
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
    if (buttonMask > 1) {
        return;
    }
    ScreenData *screenData = client->screen->screenData;
    ClientData *clientData = client->clientData;
    if (x >= 0 && x < client->screen->width &&
        y >= 0 && y < client->screen->height) {
        if (recognizeTap(buttonMask, x, y, clientData)) {
            char json[11 + 4 + 4 + 1];
            if (curl_easy_setopt(screenData->curlHandle, CURLOPT_URL, screenData->tapURL) != CURLE_OK) {
                fputs("ERROR: Cannot set an URL.\n", stderr);
                exit(-1);
            }

            if (sprintf(json, "{\"x\":%d,\"y\":%d}", SCALE(x), SCALE(y)) < 0) {
                fputs("ERROR: Cannot create JSON.\n", stderr);
                exit(-1);
            }
            if (curl_easy_setopt(screenData->curlHandle, CURLOPT_POSTFIELDS, json) != CURLE_OK) {
                fputs("ERROR: Cannot set JSON.\n", stderr);
                exit(-1);
            }
            if (curl_easy_perform(screenData->curlHandle) != CURLE_OK) {
                fputs("ERROR: Cannot send a request.\n", stderr);
                exit(-1);
            }
        }
        if (recognizeDrag(buttonMask, x, y, clientData)) {
            char json[46 + 4 + 4 + 4 + 4 + 1];
            if (curl_easy_setopt(screenData->curlHandle, CURLOPT_URL, screenData->dragURL) != CURLE_OK) {
                fputs("ERROR: Cannot set an URL.\n", stderr);
                exit(-1);
            }

            if (sprintf(json, "{\"fromX\":%d,\"fromY\":%d,\"toX\":%d,\"toY\":%d,\"duration\":0}",
                        SCALE(clientData->dragStartX), SCALE(clientData->dragStartY),
                        SCALE(x), SCALE(y)) < 0) {
                fputs("ERROR: Cannot create JSON.\n", stderr);
                exit(-1);
            }
            if (curl_easy_setopt(screenData->curlHandle, CURLOPT_POSTFIELDS, json) != CURLE_OK) {
                fputs("ERROR: Cannot set JSON.\n", stderr);
                exit(-1);
            }
            if (curl_easy_perform(screenData->curlHandle) != CURLE_OK) {
                fputs("ERROR: Cannot send a request.\n", stderr);
                exit(-1);
            }
        }

        clientData->lastX = x;
        clientData->lastY = y;
    }
}

static void *keyboardQueuer(void *args)
{
    ScreenData *screenData;
    screenData = args;
    CURL *curlHandle;
    int charLen = 0;
    char json[140 + 2 + 1];
    while(TRUE)
    {
        usleep(300000);
        charLen = strlen(keyboardChars);
        if (charLen > 0) {
            
            curlHandle = curl_easy_init();
            fprintf(stderr, "Waiting... %s\n", keyboardChars);
            
            if (curl_easy_setopt(curlHandle, CURLOPT_URL, screenData->keyURL) != CURLE_OK) {
                fputs("ERROR: Cannot set an URL.\n", stderr);
            }

            if (sprintf(json, "{\"value\":[\"%s\"]}", keyboardChars) < 0) {
                fputs("ERROR: Cannot create JSON.\n", stderr);
            }
            
            if (curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, curl_slist_append(NULL, "Content-Type: application/json")) != CURLE_OK) {
                fputs("ERROR: Cannot set HTTP headers.\n", stderr);
            }
            if (curl_easy_setopt(curlHandle, CURLOPT_PORT, screenData->httpPort) != CURLE_OK) {
                fputs("ERROR: Cannot set a port.\n", stderr);
            }
            
            if (curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, json) != CURLE_OK) {
                fputs("ERROR: Cannot set JSON.\n", stderr);
            }
            CURLcode res;
            res = curl_easy_perform(curlHandle);
            
            if (res != CURLE_OK) {
                fprintf(stderr, "%s", curl_easy_strerror(res));
                fputs("ERROR: Cannot send a request.\n", stderr);
            }
            if (strlen(keyboardChars) > charLen) {
                int len = (strlen(keyboardChars) - charLen);
                fprintf(stderr, "Added keys %d , %d", charLen, len);
                int charsize = (strlen(keyboardChars) + charLen)+1;
                char substr[charsize];
                strncpy(substr, keyboardChars + charLen, len);
                
                substr[len] = '\0';
                memset(&keyboardChars[0], 0, sizeof(keyboardChars));
                strncpy(keyboardChars, substr, strlen(substr));
                fprintf(stderr, "Added keys while processing %s\n", substr);
                fprintf(stderr, "Added keys while processing %s\n", keyboardChars);
            } else {
                memset(&keyboardChars[0], 0, sizeof(keyboardChars));
            }
            json[0] = '\0';
        }
    }
    
    curl_easy_cleanup(curlHandle);
}


static void kbdHandler(rfbBool down, rfbKeySym key, rfbClientPtr client) {
    if (down) {
        char character[2 + 1];
        int validSpecialKey = 0;

        switch (key) {
            case XK_Return:
                validSpecialKey = 1;
                strcpy(character, "\\n");
                break;
            case XK_BackSpace:
                validSpecialKey = 1;
                strcpy(character, "\\b");
                break;
            case XK_Num_Lock:
                strcpy(character, "");
                break;
            default:
                sprintf(character, "%c", key);
                break;
        }
        if (!(key>=' ' && key<0x100) && validSpecialKey == 0) {
            fprintf(stderr, "Invalid key %c", key);
            return;
        }
        if (strlen(character) > 0) {
            strcat(keyboardChars, character);
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
                    clientData->tapRecognitionState = TapRecognitionStateNotRecognized;
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

static char *createURL(const char *host, const char *sessionID, const char *actionPath) {
    char *URL;
    URL = malloc((17 + strlen(host) + strlen(sessionID) + strlen(actionPath) + 1)
                 * sizeof(char));
    if (URL) {
        if (sprintf(URL, "http://%s/session/%s/%s", host, sessionID, actionPath) < 0) {
            free(URL);
            URL = NULL;
        }
    }
    return URL;
}
