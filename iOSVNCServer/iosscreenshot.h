//
//  iosscreenshot.h
//  iOSVNCServer
//
//  Created by Michal Pietras on 10/07/2016.
//  Copyright © 2016 TestingBot. All rights reserved.
//

#ifndef iosscreenshot_h
#define iosscreenshot_h

#include <stdlib.h>

typedef void *iosss_handle_t;

iosss_handle_t iosss_create();
int iosss_take(iosss_handle_t handle, void **imgdata, size_t *imgsize);
void iosss_free(iosss_handle_t handle);

#endif /* iosscreenshot_h */
