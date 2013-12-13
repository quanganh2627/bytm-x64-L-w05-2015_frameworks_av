/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "IMSPlayerLoader"
#include <utils/Log.h>
#include <dlfcn.h>
#include "IMSPlayerLoader.h"
#include "AwesomePlayer.h"

namespace android {

typedef bool(*isIMSEnabledFunc_t)();
typedef android::AwesomePlayerBase* (*getPlayerFunc_t)();
typedef void (*deletePlayerFunc_t)(android::AwesomePlayerBase*);

IMSPlayerLoader::IMSPlayerLoader()
    : mIMSLibHandler(NULL),
      mIsIMSPlayerLoaded(false) {
    ALOGV("IMSPlayerLoader");
}

IMSPlayerLoader::~IMSPlayerLoader() {
    ALOGV("~IMSPlayerLoader");
    if (NULL != mIMSLibHandler) {
        dlclose(mIMSLibHandler);
        mIMSLibHandler = NULL;
    }
}

AwesomePlayer* IMSPlayerLoader::load() {
    ALOGV("load");
    AwesomePlayer *pAwesomePlayer = NULL;

    do {
        if (NULL == mIMSLibHandler) {
            mIMSLibHandler = dlopen("libimsplayer.so", RTLD_NOW);
            if (!mIMSLibHandler) {
                ALOGV("Cannot open libimsplayer.so");
                break;
            }
        }

        // call dlerror before dlsym is to read / clear the last error caused by other codes.
        const char* error = dlerror();

        isIMSEnabledFunc_t isIMSEnabledFunc =
                (isIMSEnabledFunc_t)dlsym(mIMSLibHandler, "isIMSEnabled");
        error = dlerror();
        if ((error != NULL) || (NULL == isIMSEnabledFunc)) {
            ALOGE("Cannot get function isIMSEnabled() from libimsplayer library");
            break;
        }

        bool isIMSEnabled = (bool)((*isIMSEnabledFunc)());
        if (!isIMSEnabled) {
            ALOGV("IMS was configured to disabled");
            break;
        }

        getPlayerFunc_t getPlayerFunc = (getPlayerFunc_t) dlsym(mIMSLibHandler, "getPlayer");
        error = dlerror();
        if ((error != NULL) || (NULL == getPlayerFunc)) {
            ALOGE("IMS: Cannot get function getPlayer() from libimsplayer library");
            break;
        }

        pAwesomePlayer = (AwesomePlayer *)((*getPlayerFunc)());
        if (NULL == pAwesomePlayer) {
            ALOGE("IMS: Cannot initialize IMS player instance");
            break;
        }

        mIsIMSPlayerLoaded = true;
    } while (false);

    return pAwesomePlayer;
}

bool IMSPlayerLoader::unload(AwesomePlayer *pAwesomePlayer) {
    ALOGV("unload");
    bool ret = false;

    do {
        if ((NULL == pAwesomePlayer) || (NULL == mIMSLibHandler)) {
            ALOGE("Cannot unload IMS player. The input is null or load() was not called before");
            break;
        }

        // call dlerror before dlsym is to read / clear the last error caused by other codes.
        const char* error = dlerror();

        deletePlayerFunc_t deletePlayerFunc =
                (deletePlayerFunc_t) dlsym(mIMSLibHandler, "deletePlayer");
        error = dlerror();
        if ((error != NULL) || (NULL == deletePlayerFunc)) {
            ALOGE("Cannot get function deletePlayer() from libimsplayer library");
            break;
        }

        (*deletePlayerFunc)(pAwesomePlayer);
        ret = true;
    } while (false);

    return ret;
}

}  // namespace android
