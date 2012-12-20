/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
* Portions contributed by: Intel Corporation
*/

#define LOG_TAG "mediaserver"
//#define LOG_NDEBUG 0

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include <dlfcn.h>

// from LOCAL_C_INCLUDES
#include "AudioFlinger.h"
#include "CameraService.h"
#include "MediaPlayerService.h"
#include "AudioPolicyService.h"

using namespace android;

int main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    ALOGI("ServiceManager: %p", sm.get());
    AudioFlinger::instantiate();
    MediaPlayerService::instantiate();
    CameraService::instantiate();
    AudioPolicyService::instantiate();

#ifdef INTEL_WIDI
    void *hlibintelwidi = dlopen("libwidiservice.so", RTLD_NOW);
    if (hlibintelwidi) {
        dlerror(); // Clear existing errors
        typedef bool (*instantiateFunc_t)();
        instantiateFunc_t instantiate = (instantiateFunc_t) dlsym(hlibintelwidi, "instantiate");
        const char* error = dlerror();
        if(error == NULL) {
            bool ret = (*instantiate)();
            if(!ret) {
                ALOGI("Could not invoke instantiate() on libwidiservice.so! Intel widi will not be used.");
            }
        }
        else {
            ALOGI("dlsym(instantiate) failed with error %s! Intel widi will not be used.", error);
        }
    }
    else {
        ALOGI("dlopen(libwidiservice) failed! Intel widi will not be used.");
    }
#endif

    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

#ifdef INTEL_WIDI
    if(hlibintelwidi) {
        dlclose(hlibintelwidi);
    }
#endif
}
