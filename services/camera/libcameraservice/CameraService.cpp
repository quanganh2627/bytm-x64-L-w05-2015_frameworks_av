/*
**
** Copyright (C) 2008, The Android Open Source Project
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

#define LOG_TAG "CameraService"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
#include <gui/SurfaceTextureClient.h>
#include <gui/Surface.h>
#include <hardware/hardware.h>
#include <media/AudioSystem.h>
#include <media/mediaplayer.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/String16.h>

#include "CameraService.h"
#include "CameraClient.h"
#include "Camera2Client.h"

namespace android {

// ----------------------------------------------------------------------------
// Logging support -- this is for debugging only
// Use "adb shell dumpsys media.camera -v 1" to change it.
volatile int32_t gLogLevel = 0;

#define LOG1(...) ALOGD_IF(gLogLevel >= 1, __VA_ARGS__);
#define LOG2(...) ALOGD_IF(gLogLevel >= 2, __VA_ARGS__);

static void setLogLevel(int level) {
    android_atomic_write(level, &gLogLevel);
}

// ----------------------------------------------------------------------------

static int getCallingPid() {
    return IPCThreadState::self()->getCallingPid();
}

static int getCallingUid() {
    return IPCThreadState::self()->getCallingUid();
}

// ----------------------------------------------------------------------------

// This is ugly and only safe if we never re-create the CameraService, but
// should be ok for now.
static CameraService *gCameraService;

CameraService::CameraService()
:mSoundRef(0), mAudioTrackBurst(NULL), mBufferBurst(NULL), mBufferBurstSize(0), mModule(0)
{
    ALOGI("CameraService started (pid=%d)", getpid());
    gCameraService = this;
}

void CameraService::onFirstRef()
{
    BnCameraService::onFirstRef();

    if (hw_get_module(CAMERA_HARDWARE_MODULE_ID,
                (const hw_module_t **)&mModule) < 0) {
        ALOGE("Could not load camera HAL module");
        mNumberOfCameras = 0;
    }
    else {
        mNumberOfCameras = mModule->get_number_of_cameras();
        if (mNumberOfCameras > MAX_CAMERAS) {
            ALOGE("Number of cameras(%d) > MAX_CAMERAS(%d).",
                    mNumberOfCameras, MAX_CAMERAS);
            mNumberOfCameras = MAX_CAMERAS;
        }
        for (int i = 0; i < mNumberOfCameras; i++) {
            setCameraFree(i);
        }
    }
}

CameraService::~CameraService() {
    for (int i = 0; i < mNumberOfCameras; i++) {
        if (mBusy[i]) {
            ALOGE("camera %d is still in use in destructor!", i);
        }
    }

    gCameraService = NULL;
    if (mAudioTrackBurst) {
        mAudioTrackBurst->stop();
        delete mAudioTrackBurst;
        mAudioTrackBurst = NULL;
    }
    if (mBufferBurst) {
        delete [] mBufferBurst;
        mBufferBurst = NULL;
        mBufferBurstSize = 0;
    }
}

int32_t CameraService::getNumberOfCameras() {
    return mNumberOfCameras;
}

status_t CameraService::getCameraInfo(int cameraId,
                                      struct CameraInfo* cameraInfo) {
    if (!mModule) {
        return NO_INIT;
    }

    if (cameraId < 0 || cameraId >= mNumberOfCameras) {
        return BAD_VALUE;
    }

    struct camera_info info;
    status_t rc = mModule->get_camera_info(cameraId, &info);
    cameraInfo->facing = info.facing;
    cameraInfo->orientation = info.orientation;
    return rc;
}

sp<ICamera> CameraService::connect(
        const sp<ICameraClient>& cameraClient, int cameraId) {
    int callingPid = getCallingPid();

    LOG1("CameraService::connect E (pid %d, id %d)", callingPid, cameraId);

    if (!mModule) {
        ALOGE("Camera HAL module not loaded");
        return NULL;
    }

    sp<Client> client;
    if (cameraId < 0 || cameraId >= mNumberOfCameras) {
        ALOGE("CameraService::connect X (pid %d) rejected (invalid cameraId %d).",
            callingPid, cameraId);
        return NULL;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("sys.secpolicy.camera.disabled", value, "0");
    if (strcmp(value, "1") == 0) {
        // Camera is disabled by DevicePolicyManager.
        ALOGI("Camera is disabled. connect X (pid %d) rejected", callingPid);
        return NULL;
    }

    Mutex::Autolock lock(mServiceLock);
    if (mClient[cameraId] != 0) {
        client = mClient[cameraId].promote();
        if (client != 0) {
            if (cameraClient->asBinder() == client->getCameraClient()->asBinder()) {
                LOG1("CameraService::connect X (pid %d) (the same client)",
                     callingPid);
                return client;
            } else {
                ALOGW("CameraService::connect X (pid %d) rejected (existing client).",
                      callingPid);
                return NULL;
            }
        }
        mClient[cameraId].clear();
    }

    if (mBusy[cameraId]) {
        ALOGW("CameraService::connect X (pid %d) rejected"
                " (camera %d is still busy).", callingPid, cameraId);
        return NULL;
    }

    struct camera_info info;
    if (mModule->get_camera_info(cameraId, &info) != OK) {
        ALOGE("Invalid camera id %d", cameraId);
        return NULL;
    }

    int deviceVersion;
    if (mModule->common.module_api_version == CAMERA_MODULE_API_VERSION_2_0) {
        deviceVersion = info.device_version;
    } else {
        deviceVersion = CAMERA_DEVICE_API_VERSION_1_0;
    }

    switch(deviceVersion) {
      case CAMERA_DEVICE_API_VERSION_1_0:
        client = new CameraClient(this, cameraClient, cameraId,
                info.facing, callingPid, getpid());
        break;
      case CAMERA_DEVICE_API_VERSION_2_0:
        client = new Camera2Client(this, cameraClient, cameraId,
                info.facing, callingPid, getpid());
        break;
      default:
        ALOGE("Unknown camera device HAL version: %d", deviceVersion);
        return NULL;
    }

    if (client->initialize(mModule) != OK) {
        return NULL;
    }

    cameraClient->asBinder()->linkToDeath(this);

    mClient[cameraId] = client;
    LOG1("CameraService::connect X (id %d, this pid is %d)", cameraId, getpid());
    return client;
}

void CameraService::removeClient(const sp<ICameraClient>& cameraClient) {
    int callingPid = getCallingPid();
    LOG1("CameraService::removeClient E (pid %d)", callingPid);

    // Declare this before the lock to make absolutely sure the
    // destructor won't be called with the lock held.
    Mutex::Autolock lock(mServiceLock);

    int outIndex;
    sp<Client> client = findClientUnsafe(cameraClient->asBinder(), outIndex);

    if (client != 0) {
        // Found our camera, clear and leave.
        LOG1("removeClient: clear camera %d", outIndex);
        mClient[outIndex].clear();

        client->unlinkToDeath(this);
    }

    LOG1("CameraService::removeClient X (pid %d)", callingPid);
}

sp<CameraService::Client> CameraService::findClientUnsafe(
                        const wp<IBinder>& cameraClient, int& outIndex) {
    sp<Client> client;

    for (int i = 0; i < mNumberOfCameras; i++) {

        // This happens when we have already disconnected (or this is
        // just another unused camera).
        if (mClient[i] == 0) continue;

        // Promote mClient. It can fail if we are called from this path:
        // Client::~Client() -> disconnect() -> removeClient().
        client = mClient[i].promote();

        // Clean up stale client entry
        if (client == NULL) {
            mClient[i].clear();
            continue;
        }

        if (cameraClient == client->getCameraClient()->asBinder()) {
            // Found our camera
            outIndex = i;
            return client;
        }
    }

    outIndex = -1;
    return NULL;
}

CameraService::Client* CameraService::getClientByIdUnsafe(int cameraId) {
    if (cameraId < 0 || cameraId >= mNumberOfCameras) return NULL;
    return mClient[cameraId].unsafe_get();
}

Mutex* CameraService::getClientLockById(int cameraId) {
    if (cameraId < 0 || cameraId >= mNumberOfCameras) return NULL;
    return &mClientLock[cameraId];
}

sp<CameraService::Client> CameraService::getClientByRemote(
                                const wp<IBinder>& cameraClient) {

    // Declare this before the lock to make absolutely sure the
    // destructor won't be called with the lock held.
    sp<Client> client;

    Mutex::Autolock lock(mServiceLock);

    int outIndex;
    client = findClientUnsafe(cameraClient, outIndex);

    return client;
}

status_t CameraService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    // Permission checks
    switch (code) {
        case BnCameraService::CONNECT:
            const int pid = getCallingPid();
            const int self_pid = getpid();
            if (pid != self_pid) {
                // we're called from a different process, do the real check
                if (!checkCallingPermission(
                        String16("android.permission.CAMERA"))) {
                    const int uid = getCallingUid();
                    ALOGE("Permission Denial: "
                         "can't use the camera pid=%d, uid=%d", pid, uid);
                    return PERMISSION_DENIED;
                }
            }
            break;
    }

    return BnCameraService::onTransact(code, data, reply, flags);
}

// The reason we need this busy bit is a new CameraService::connect() request
// may come in while the previous Client's destructor has not been run or is
// still running. If the last strong reference of the previous Client is gone
// but the destructor has not been finished, we should not allow the new Client
// to be created because we need to wait for the previous Client to tear down
// the hardware first.
void CameraService::setCameraBusy(int cameraId) {
    android_atomic_write(1, &mBusy[cameraId]);

    ALOGV("setCameraBusy cameraId=%d", cameraId);
}

void CameraService::setCameraFree(int cameraId) {
    android_atomic_write(0, &mBusy[cameraId]);

    ALOGV("setCameraFree cameraId=%d", cameraId);
}

// We share the media players for shutter and recording sound for all clients.
// A reference count is kept to determine when we will actually release the
// media players.

MediaPlayer* CameraService::newMediaPlayer(const char *file) {
    MediaPlayer* mp = new MediaPlayer();
    if (mp->setDataSource(file, NULL) == NO_ERROR) {
        mp->setAudioStreamType(AUDIO_STREAM_ENFORCED_AUDIBLE);
        mp->prepare();
    } else {
        ALOGE("Failed to load CameraService sounds: %s", file);
        delete mp;
        return NULL;
    }
    return mp;
}

// We use AudioTrack player play short burst sound

void CameraService::loadSoundBurst() {
    // Create AudioTrack when SOUND_BURST has to be played
    int framecount(0);
    FILE* fp(NULL);

    if (AudioSystem::getOutputFrameCount(&framecount, AUDIO_STREAM_ENFORCED_AUDIBLE) != (NO_ERROR)) {
        ALOGE("Failed to get the framecount for audio track in CameraService");
        return;
    }

    mAudioTrackBurst = new AudioTrack();
    if (mAudioTrackBurst == NULL) {
        ALOGE("Failed alloc CameraService AudioTrack");
        return;
    }
    mAudioTrackBurst->set(AUDIO_STREAM_ENFORCED_AUDIBLE,
                     0,//AudioTrack calculated this value to defaut 44100 in it's set function
                     AUDIO_FORMAT_PCM_16_BIT,
                     AUDIO_CHANNEL_OUT_STEREO,
                     framecount
    );

    status_t err = mAudioTrackBurst->initCheck();
    if (err != NO_ERROR) {
        ALOGE("CameraService AudioTrack init check failed");
        goto error1;
    }

    // Open the fast_click.pcm file and copy the contents into the buffer for later use
    fp = fopen("/system/media/audio/ui/fast_click.pcm", "r");
    if (fp == NULL) {
        ALOGE("Failed to load CameraService sound file /system/media/audio/ui/fast_click.pcm");
        goto error1;
    }

    fseek(fp, 0, SEEK_END);
    mBufferBurstSize = ftell(fp); // filesize
    rewind(fp);

    mBufferBurst = new uint8_t[mBufferBurstSize];
    if (mBufferBurst == NULL) {
        ALOGE("Failed alloc CameraService mBufferBurst");
        goto error2;
    }
    fread(mBufferBurst, 1, mBufferBurstSize, fp);
    fclose(fp);

    return;

    // error situation handling
error2:
    fclose(fp);
    mBufferBurstSize = 0;

error1:
    delete mAudioTrackBurst;
    mAudioTrackBurst = NULL;
}

void CameraService::loadSound() {
    Mutex::Autolock lock(mSoundLock);
    LOG1("CameraService::loadSound ref=%d", mSoundRef);
    if (mSoundRef++) return;

    int64_t token = IPCThreadState::self()->clearCallingIdentity();

    mSoundPlayer[SOUND_SHUTTER] = newMediaPlayer("/system/media/audio/ui/camera_click.ogg");
    mSoundPlayer[SOUND_RECORDING] = newMediaPlayer("/system/media/audio/ui/VideoRecord.ogg");
    mSoundPlayer[SOUND_BURST].clear(); //we use AudioTrack instead of MediaPlayer
    loadSoundBurst();

    IPCThreadState::self()->restoreCallingIdentity(token);
}

void CameraService::releaseSound() {
    Mutex::Autolock lock(mSoundLock);
    LOG1("CameraService::releaseSound ref=%d", mSoundRef);
    if (--mSoundRef) return;

    int64_t token = IPCThreadState::self()->clearCallingIdentity();

    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (mSoundPlayer[i] != 0) {
            mSoundPlayer[i]->disconnect();
            mSoundPlayer[i].clear();
        }
    }

    if (mAudioTrackBurst) {
        mAudioTrackBurst->stop();
        delete mAudioTrackBurst;
        mAudioTrackBurst = NULL;
    }
    if (mBufferBurst) {
        delete [] mBufferBurst;
        mBufferBurst = NULL;
        mBufferBurstSize = 0;
    }

    IPCThreadState::self()->restoreCallingIdentity(token);
}

void CameraService::playSound(sound_kind kind) {
    LOG1("playSound(%d)", kind);
    Mutex::Autolock lock(mSoundLock);

    int64_t token = IPCThreadState::self()->clearCallingIdentity();

    //Only for SOUND_BURST play the sound from fast_click.pcm
    if (kind == SOUND_BURST) {
        if (mAudioTrackBurst == NULL)
            return;
        if (!mAudioTrackBurst->stopped()) { // stop the track if already playing
            mAudioTrackBurst->stop();
            mAudioTrackBurst->flush();
        }
        mAudioTrackBurst->start();
        mAudioTrackBurst->write((const void*) mBufferBurst, (size_t) mBufferBurstSize);

        int atSize = mAudioTrackBurst->frameCount() * mAudioTrackBurst->frameSize();
        if (mBufferBurstSize < atSize) {
            // call stop() to make the audiotrack played immediately.
            mAudioTrackBurst->stop();
        }

        IPCThreadState::self()->restoreCallingIdentity(token);
        return;
    }

    sp<MediaPlayer> player = mSoundPlayer[kind];
    if (player != 0) {
        player->seekTo(0);
        player->start();
    }

    IPCThreadState::self()->restoreCallingIdentity(token);
}

// ----------------------------------------------------------------------------

CameraService::Client::Client(const sp<CameraService>& cameraService,
        const sp<ICameraClient>& cameraClient,
        int cameraId, int cameraFacing, int clientPid, int servicePid) {
    int callingPid = getCallingPid();
    LOG1("Client::Client E (pid %d, id %d)", callingPid, cameraId);

    mCameraService = cameraService;
    mCameraClient = cameraClient;
    mCameraId = cameraId;
    mCameraFacing = cameraFacing;
    mClientPid = clientPid;
    mServicePid = servicePid;
    mDestructionStarted = false;

    cameraService->setCameraBusy(cameraId);
    cameraService->loadSound();
    LOG1("Client::Client X (pid %d, id %d)", callingPid, cameraId);
}

// tear down the client
CameraService::Client::~Client() {
    mCameraService->releaseSound();

    // unconditionally disconnect. function is idempotent
    Client::disconnect();
}

// ----------------------------------------------------------------------------

Mutex* CameraService::Client::getClientLockFromCookie(void* user) {
    return gCameraService->getClientLockById((int) user);
}

// Provide client pointer for callbacks. Client lock returned from getClientLockFromCookie should
// be acquired for this to be safe
CameraService::Client* CameraService::Client::getClientFromCookie(void* user) {
    Client* client = gCameraService->getClientByIdUnsafe((int) user);

    // This could happen if the Client is in the process of shutting down (the
    // last strong reference is gone, but the destructor hasn't finished
    // stopping the hardware).
    if (client == NULL) return NULL;

    // destruction already started, so should not be accessed
    if (client->mDestructionStarted) return NULL;

    return client;
}

// NOTE: function is idempotent
void CameraService::Client::disconnect() {
    mCameraService->removeClient(mCameraClient);
    mCameraService->setCameraFree(mCameraId);
}

// ----------------------------------------------------------------------------

static const int kDumpLockRetries = 50;
static const int kDumpLockSleep = 60000;

static bool tryLock(Mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.tryLock() == NO_ERROR) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

status_t CameraService::dump(int fd, const Vector<String16>& args) {
    String8 result;
    if (checkCallingPermission(String16("android.permission.DUMP")) == false) {
        result.appendFormat("Permission Denial: "
                "can't dump CameraService from pid=%d, uid=%d\n",
                getCallingPid(),
                getCallingUid());
        write(fd, result.string(), result.size());
    } else {
        bool locked = tryLock(mServiceLock);
        // failed to lock - CameraService is probably deadlocked
        if (!locked) {
            result.append("CameraService may be deadlocked\n");
            write(fd, result.string(), result.size());
        }

        bool hasClient = false;
        if (!mModule) {
            result = String8::format("No camera module available!\n");
            write(fd, result.string(), result.size());
            return NO_ERROR;
        }

        result = String8::format("Camera module HAL API version: 0x%x\n",
                mModule->common.hal_api_version);
        result.appendFormat("Camera module API version: 0x%x\n",
                mModule->common.module_api_version);
        result.appendFormat("Camera module name: %s\n",
                mModule->common.name);
        result.appendFormat("Camera module author: %s\n",
                mModule->common.author);
        result.appendFormat("Number of camera devices: %d\n\n", mNumberOfCameras);
        write(fd, result.string(), result.size());
        for (int i = 0; i < mNumberOfCameras; i++) {
            result = String8::format("Camera %d static information:\n", i);
            camera_info info;

            status_t rc = mModule->get_camera_info(i, &info);
            if (rc != OK) {
                result.appendFormat("  Error reading static information!\n");
                write(fd, result.string(), result.size());
            } else {
                result.appendFormat("  Facing: %s\n",
                        info.facing == CAMERA_FACING_BACK ? "BACK" : "FRONT");
                result.appendFormat("  Orientation: %d\n", info.orientation);
                int deviceVersion;
                if (mModule->common.module_api_version <
                        CAMERA_MODULE_API_VERSION_2_0) {
                    deviceVersion = CAMERA_DEVICE_API_VERSION_1_0;
                } else {
                    deviceVersion = info.device_version;
                }
                result.appendFormat("  Device version: 0x%x\n", deviceVersion);
                if (deviceVersion >= CAMERA_DEVICE_API_VERSION_2_0) {
                    result.appendFormat("  Device static metadata:\n");
                    write(fd, result.string(), result.size());
                    dump_indented_camera_metadata(info.static_camera_characteristics,
                            fd, 2, 4);
                } else {
                    write(fd, result.string(), result.size());
                }
            }

            sp<Client> client = mClient[i].promote();
            if (client == 0) {
                result = String8::format("  Device is closed, no client instance\n");
                write(fd, result.string(), result.size());
                continue;
            }
            hasClient = true;
            result = String8::format("  Device is open. Client instance dump:\n");
            write(fd, result.string(), result.size());
            client->dump(fd, args);
        }
        if (!hasClient) {
            result = String8::format("\nNo active camera clients yet.\n");
            write(fd, result.string(), result.size());
        }

        if (locked) mServiceLock.unlock();

        // change logging level
        int n = args.size();
        for (int i = 0; i + 1 < n; i++) {
            String16 verboseOption("-v");
            if (args[i] == verboseOption) {
                String8 levelStr(args[i+1]);
                int level = atoi(levelStr.string());
                result = String8::format("\nSetting log level to %d.\n", level);
                setLogLevel(level);
                write(fd, result.string(), result.size());
            }
        }

    }
    return NO_ERROR;
}

/*virtual*/void CameraService::binderDied(
    const wp<IBinder> &who) {

    /**
      * While tempting to promote the wp<IBinder> into a sp,
      * it's actually not supported by the binder driver
      */

    ALOGV("java clients' binder died");

    sp<Client> cameraClient = getClientByRemote(who);

    if (cameraClient == 0) {
        ALOGV("java clients' binder death already cleaned up (normal case)");
        return;
    }

    ALOGW("Disconnecting camera client %p since the binder for it "
          "died (this pid %d)", cameraClient.get(), getCallingPid());

    cameraClient->disconnect();

}

}; // namespace android
