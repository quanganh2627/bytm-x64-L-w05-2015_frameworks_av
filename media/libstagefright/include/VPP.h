/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef _VPP_H_
#define _VPP_H_

#include <media/IOMX.h>
#include <media/stagefright/ACodec.h>
#include <media/stagefright/foundation/AHandler.h>

#include <OMX_VPP.h>

namespace android {


static const char* kKeyVppInBufNum          = "vibn";
static const char* kKeyVppOutBufNum         = "vobn";
static const char* kKeyDecodeMsg            = "dcdm";
static const char* kKeyKeepCompAlloc        = "kpcc";
static const char* kKeyDecodeBufID          = "ddbd";

struct ABuffer;
struct ACodec;

class VPP : public AHandler {
public:
    enum {
        kWhatComponentAllocated             = 'coal',
        kWhatConfigured                     = 'cfgd',
        kWhatStarted                        = 'sttd',
        kWhatDrainBuffer                    = 'drnb',
        kWhatShutdownCompleted              = 'sdcd',
        kWhatFlushCompleted                 = 'flcd',
    };

    VPP(const sp<ACodec> &codec);

    void setNotificationMessage(const sp<AMessage> &msg);
    void initiateAllocateComponent(const sp<AMessage> &msg);
    void initiateConfigure(const sp<AMessage> &msg);
    void initiateStart(const sp<AMessage> &msg);
    void initiateDisablePorts(const sp<AMessage> &msg);
    void initiateEnablePorts(const sp<AMessage> &msg);
    void emptyBuffer(const sp<AMessage> &msg);
    void initiateShutdown(const bool keepComponentAllocated);
    void signalFlush();
    void signalResume();

    static int32_t HasEncCtx;

protected:
    virtual ~VPP();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum {
        kPortIndexInput                     = 0,
        kPortIndexOutput                    = 1,
    };

    enum {
        UNINIT,
        LOADED,
        LOADEDTOIDLE,
        IDLETOEXECUTE,
        EXECUTE,
        EXECUTETOIDLE,
        IDLETOLOADED,
        FLUSH,
        PORTSETTINGCHANGE,
    };

    enum {
        kWhatOMXMessage                     = 'omme',
        kWhatError                          = 'errr',
        kWhatAllocateComponent              = 'alco',
        kWhatConfigure                      = 'cnfg',
        kWhatStart                          = 'stat',
        kWhatDisablePort                    = 'dsbp',
        kWhatEnablePort                     = 'enpo',
        kWhatEmptyBuffer                    = 'mptb',
        kWhatBufferDrained                  = 'bfrd',
        kWhatShutdown                       = 'shtd',
        kWhatFlush                          = 'flsh',
        kWhatResume                         = 'rsme',
    };

    class VPPBufferInfo {
    public:
        enum Status {
            OWNED_BY_US,
            OWNED_BY_NATIVE_WINDOW,
            OWNED_BY_VPP,
            OWNED_BY_DOWNSTREAM,
        };

        IOMX::buffer_id mBufferID;
        IOMX::buffer_id mDecBufferID;
        Status mStatus;
        sp<ABuffer> mData;
        sp<GraphicBuffer> mGraphicBuffer;
        sp<AMessage> mNotify;
    };

    uint32_t mReplyId;
    uint32_t mState;
    uint32_t mPreState;
    bool mKeepComponentAllocated;
    sp<ACodec> mCodec;
    sp<AMessage> mNotify;
    AString mComponentName;
    sp<IOMX> mOMX;
    IOMX::node_id mNode;
    sp<ANativeWindow> mNativeWindow;
    Vector<VPPBufferInfo> mBuffers[2];
    List<sp<AMessage> > mDeferred;
    bool mFlushed[2];
    uint32_t mFilterTypes;
    int32_t mFrameRate;
    bool m3PConfiged;

    void signalError(OMX_ERRORTYPE error, status_t internalError = UNKNOWN_ERROR);

    bool onOMXMessage(const sp<AMessage> &msg);
    bool onOMXEvent(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2);
    bool onOMXEmptyBufferDone(IOMX::buffer_id bufferID);
    bool onOMXFillBufferDone(
            IOMX::buffer_id bufferID,
            size_t rangeOffset, size_t rangeLength,
            OMX_U32 flags,
            int64_t timeUs,
            void *platformPrivate,
            void *dataPtr);

    bool onAllocateComponent(const sp<AMessage> &msg);
    bool onConfigure(const sp<AMessage> &msg);
    bool onStart(const sp<AMessage> &msg);
    bool onDisablePorts(const sp<AMessage> &msg);
    bool onEnablePorts(const sp<AMessage> &msg);
    bool onEmptyBuffer(const sp<AMessage> &msg);
    bool onBufferDrained(const sp<AMessage> &msg);
    bool onFlush(const sp<AMessage> &msg);
    bool onFlushed();
    bool onShutdown();

    void deferMessage(const sp<AMessage> &msg);
    void processDeferredMessage();
    void changeState(uint32_t state);
    void changeOMXState();
    VPPBufferInfo *dequeueBufferFromNativeWindow();
    void submitOutputBuffers();
    status_t freeBuffersNotOwnedByVPP(uint32_t portIndex);
    status_t freeBuffer(uint32_t portIndex, size_t i);
    status_t cancelBufferToNativeWindow(VPPBufferInfo *info);
    VPPBufferInfo * findBufferByID(uint32_t portIndex, 
            IOMX::buffer_id bufferID, ssize_t *index);
    VPPBufferInfo * findBufferByDecID(uint32_t portIndex, 
            IOMX::buffer_id bufferID, ssize_t *index);
    status_t configComponent(int32_t *inBufNum, int32_t *outBufNum,
            int32_t *reserveBufNum, int32_t *allNum);
    status_t configFilters();
    status_t config3PFilter(int32_t hasEncoder, int32_t frameRate);
    status_t setInputBuffersToOMX(const uint32_t inBufNum, const uint32_t totalBufNum);
    status_t setOutputBuffersToOMX(const uint32_t bufferNum, const uint32_t cancelNum);
    void enterExecuteState();

    DISALLOW_EVIL_CONSTRUCTORS(VPP);
};

}  // namespace android

#endif  // _VPP_H_
