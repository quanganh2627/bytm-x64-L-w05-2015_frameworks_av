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

#ifndef ASYNC_OMXCODEC_WRAPPER_H_

#define ASYNC_OMXCODEC_WRAPPER_H_

#include <sys/types.h>

#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <media/IOMX.h>
#include <android/native_window.h>
#include <media/stagefright/OMXCodec.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AHandlerReflector.h>

namespace android {

struct AsyncOMXCodecWrapper : public MediaSource {

    static sp<MediaSource> Create(
             const sp<IOMX> &omx,
             const sp<MetaData> &meta, bool createEncoder,
             const sp<MediaSource> &source,
             const char *matchComponentName = NULL,
             uint32_t flags = 0,
             const sp<ANativeWindow> &nativeWindow = NULL);


    // To be called before any other methods on this object, except
    // getFormat().
    virtual status_t start(MetaData *params = NULL);

    // Any blocking read call returns immediately with a result of NO_INIT.
    // It is an error to call any methods other than start after this call
    // returns. Any buffers the object may be holding onto at the time of
    // the stop() call are released.
    // Also, it is imperative that any buffers output by this object and
    // held onto by callers be released before a call to stop() !!!
    virtual status_t stop();

    // Returns the format of the data output by this media source.
    virtual sp<MetaData> getFormat();

    // Returns a new buffer of data. Call blocks until a
    // buffer is available, an error is encountered of the end of the stream
    // is reached.
    // End of stream is signalled by a result of ERROR_END_OF_STREAM.
    // A result of INFO_FORMAT_CHANGED indicates that the format of this
    // MediaSource has changed mid-stream, the client can continue reading
    // but should be prepared for buffers of the new configuration.
    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);


    // Causes this source to suspend pulling data from its upstream source
    // until a subsequent read-with-seek. Currently only supported by
    // OMXCodec.
    virtual status_t pause();

    virtual void onMessageReceived(const sp<AMessage> &msg);

    sp<MediaSource> getOMXCodec();

protected:
    virtual ~AsyncOMXCodecWrapper();

private:
    enum {
         kWhatStart      = 'start',
         kWhatRead       = 'read',
         kWhatPause      = 'pause',
         kWhatStop       = 'stop'
    };
    AsyncOMXCodecWrapper(const sp<IOMX> &omx,
             const sp<MetaData> &meta, bool createEncoder,
             const sp<MediaSource> &source,
             const char *matchComponentName = NULL,
             uint32_t flags = 0,
             const sp<ANativeWindow> &nativeWindow = NULL);

    AsyncOMXCodecWrapper(const AsyncOMXCodecWrapper &);
    AsyncOMXCodecWrapper &operator=(const AsyncOMXCodecWrapper &);
    sp<MediaSource> mOMXCodec;
    sp<MetaData> mOutputFormat;
    sp<ALooper> mLooper;
    sp<AHandlerReflector<AsyncOMXCodecWrapper> > mReflector;
    Mutex mLock;
    Condition mCondition;
    sp<AMessage> mAsyncResult;
    MediaSource::ReadOptions mOptions;
    bool mReadPending;
};

}  // namespace android

#endif  // ASYNC_OMXCODEC_WRAPPER_H_
