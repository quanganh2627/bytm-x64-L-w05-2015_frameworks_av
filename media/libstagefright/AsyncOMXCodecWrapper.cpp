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
#define LOG_TAG "AsyncOMXCodecWrapper"
#include "include/AsyncOMXCodecWrapper.h"
#include <media/stagefright/MetaData.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/Log.h>

namespace android {

// time limitaion is set to 200ms, so that the upper call can acquire its lock in time.
const static int64_t kReadingBufferTimeOutNs = 200000000LL;

sp<MediaSource> AsyncOMXCodecWrapper::Create(const sp < IOMX > & omx,
        const sp < MetaData > & meta,
        bool createEncoder,
        const sp < MediaSource > & source,
        const char * matchComponentName,
        uint32_t flags,const sp < ANativeWindow > & nativeWindow) {
    ALOGV("AsyncOMXCodecWrapper::Create");
    sp<AsyncOMXCodecWrapper> asyncOMXCodecWrapper = new AsyncOMXCodecWrapper(omx, meta, createEncoder, source, matchComponentName, flags, nativeWindow);

    if (asyncOMXCodecWrapper->getOMXCodec() != NULL) {
       return asyncOMXCodecWrapper;
    }
    asyncOMXCodecWrapper.clear();
    ALOGV("Create OMXCodec failure!");
    return NULL;
}

AsyncOMXCodecWrapper::AsyncOMXCodecWrapper(const sp<IOMX> &omx,
        const sp<MetaData> &meta, bool createEncoder,
        const sp<MediaSource> &source,
        const char *matchComponentName,
        uint32_t flags,
        const sp<ANativeWindow> &nativeWindow): mLooper(new ALooper),
        mOMXCodec(OMXCodec::Create(omx, meta, createEncoder, source, matchComponentName, flags, nativeWindow)),
        mReadPending(false) {

    if (mOMXCodec != NULL) {
        mLooper->setName("reading_thread");
        mLooper->start();
        mReflector = new AHandlerReflector<AsyncOMXCodecWrapper>(this);
        mLooper->registerHandler(mReflector);
    }
}

sp<MediaSource> AsyncOMXCodecWrapper::getOMXCodec() {
    return mOMXCodec;
}

status_t AsyncOMXCodecWrapper::start(MetaData *meta) {
    ALOGV("functin = %s line = %d, in ",__FUNCTION__, __LINE__);

    sp<AMessage> msg = new AMessage(kWhatStart, mReflector->id());
    sp<AMessage> response;
    status_t err = msg->postAndAwaitResponse(&response);
    if (err != OK) {
        return err;
    }
    if (!response->findInt32("err", &err)) {
        err = OK;
    }
    ALOGV("functin = %s line = %d, out!!! ",__FUNCTION__, __LINE__);
    return err;
}

status_t AsyncOMXCodecWrapper::read(
        MediaBuffer **buffer, const ReadOptions *options) {

    bool seeking = false;
    int64_t seekTimeUs = -1;
    *buffer = NULL;
    if (!mReadPending) {
        mOptions = *options;
        sp<AMessage> msg = new AMessage(kWhatRead, mReflector->id());
        msg->setPointer("options", (void*)&mOptions);
        msg->post();
        mReadPending  = true;
    } else {
        // repeat reading, response the seeking first.
        ReadOptions::SeekMode seekMode;
        if (options && options->getSeekTo(&seekTimeUs, &seekMode)) {
            seeking = true;
        }
    }
    Mutex::Autolock autoLock(mLock);
    while(mAsyncResult == NULL) {
        status_t err = mCondition.waitRelative(mLock, kReadingBufferTimeOutNs);
        if (err != OK) {
            ALOGW("reading buffer timeout! reschedule the event");
            return -EWOULDBLOCK;
        }
    }
    CHECK(mAsyncResult != NULL);

    status_t err;
    CHECK(mAsyncResult->findInt32("result", &err));
    MediaBuffer *mediaBuffer = NULL;
    CHECK(mAsyncResult->findPointer("data", (void**)&mediaBuffer));

    if (seeking && err == OK) {
        int64_t currentSeekTime = -1;
        if (!mAsyncResult->findInt64("seek", &currentSeekTime) || currentSeekTime != seekTimeUs) {
            // another seek is comming, drop the current msg
            ALOGV("drop obsolete seeking msg old seek time=%lld, comming is = %lld", currentSeekTime, seekTimeUs);
            mAsyncResult.clear();
            mReadPending = false;
            mediaBuffer->release();
            mOptions.clearSeekTo();
            return -EWOULDBLOCK;
        }
        ALOGV("old seek time=%lld, comming is = %lld", currentSeekTime, seekTimeUs);
    }
    *buffer = mediaBuffer;
    mAsyncResult.clear();
    mOptions.clearSeekTo();
    mReadPending = false;
    return err;
}

status_t AsyncOMXCodecWrapper::stop() {
    ALOGV("functin = %s line = %d, in ",__FUNCTION__, __LINE__);

    sp<AMessage> msg = new AMessage(kWhatStop, mReflector->id());
    sp<AMessage> response;
    status_t err = msg->postAndAwaitResponse(&response);
    if (err != OK) {
        return err;
    }
    if (!response->findInt32("err", &err)) {
        err = OK;
    }
    ALOGV("functin = %s line = %d, out ",__FUNCTION__, __LINE__);
    return err;
}

sp<MetaData> AsyncOMXCodecWrapper::getFormat() {

    return mOMXCodec->getFormat();

}

status_t AsyncOMXCodecWrapper::pause() {
    ALOGV("functin = %s line = %d, in ",__FUNCTION__, __LINE__);

    sp<AMessage> msg = new AMessage(kWhatPause, mReflector->id());
    msg->post();
    ALOGV("functin = %s line = %d, out ",__FUNCTION__, __LINE__);
    // always OK
    return OK;
}

AsyncOMXCodecWrapper::~AsyncOMXCodecWrapper() {
    mLooper.clear();
    mOMXCodec.clear();
    mReflector.clear();
}


void AsyncOMXCodecWrapper::onMessageReceived(const sp < AMessage > & msg) {
    switch (msg->what()) {
        case kWhatRead:
        {
            ALOGV("reading in");
            CHECK(mAsyncResult == NULL);
            int64_t seekTimeUs = -1;
            bool seeking = false;
            ReadOptions *options = NULL;
            CHECK(msg->findPointer("options", (void**)&options));
            ReadOptions::SeekMode seekMode;
            if (options && options->getSeekTo(&seekTimeUs, &seekMode)) {
                seeking = true;
            }
            MediaBuffer *mediaBuffer = NULL;
            status_t err = mOMXCodec->read(&mediaBuffer, options);
            Mutex::Autolock autoLock(mLock);
            mAsyncResult = new AMessage;
            mAsyncResult->setInt32("result", err);
            mAsyncResult->setPointer("data", (void*)mediaBuffer);
            if (seeking) {
                mAsyncResult->setInt64("seek", seekTimeUs);
            }
            ALOGV("reading out");
            mCondition.signal();
            break;
        }
        case kWhatStart:
        {
            status_t err = mOMXCodec->start();
            uint32_t replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<AMessage> response = new AMessage;
            response->setInt32("err", err);
            response->postReply(replyID);
            break;
        }
        case kWhatPause:
        {
            mOMXCodec->pause();
            break;
        }
        case kWhatStop:
        {
            {
                Mutex::Autolock autoLock(mLock);
                if (mAsyncResult != NULL) {
                    MediaBuffer *mediaBuffer = NULL;
                    CHECK(mAsyncResult->findPointer("data", (void**)&mediaBuffer));
                    if (mediaBuffer != NULL) {
                        mediaBuffer->release();
                    }
                    mAsyncResult.clear();
                }
            }
            status_t err = mOMXCodec->stop();
            uint32_t replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            sp<AMessage> response = new AMessage;
            response->setInt32("err", err);
            response->postReply(replyID);
            break;
        }
        default:
            TRESPASS();
            break;
    }
}

}  // namespace android
