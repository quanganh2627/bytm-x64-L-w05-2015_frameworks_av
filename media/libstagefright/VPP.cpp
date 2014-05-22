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

//#define LOG_NDEBUG 0
#define LOG_TAG "VPP"

#include "include/VPP.h"

#include <binder/MemoryDealer.h>

#include <media/stagefright/ACodec.h>
#include <media/stagefright/NativeWindowWrapper.h>
#include <media/stagefright/OMXClient.h>

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/OMXCodec.h>

#include <OMX_Component.h>

namespace android {

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

struct VppObserver : public BnOMXObserver {
    VppObserver() {}

    void setNotificationMessage(const sp<AMessage> &msg) {
        mNotify = msg;
    }

    // from IOMXObserver
    virtual void onMessage(const omx_message &omx_msg) {
        sp<AMessage> msg = mNotify->dup();

        msg->setInt32("type", omx_msg.type);
        msg->setPointer("node", omx_msg.node);

        switch (omx_msg.type) {
            case omx_message::EVENT:
            {
                msg->setInt32("event", omx_msg.u.event_data.event);
                msg->setInt32("data1", omx_msg.u.event_data.data1);
                msg->setInt32("data2", omx_msg.u.event_data.data2);
                break;
            }

            case omx_message::EMPTY_BUFFER_DONE:
            {
                msg->setPointer("buffer", omx_msg.u.buffer_data.buffer);
                break;
            }

            case omx_message::FILL_BUFFER_DONE:
            {
                msg->setPointer(
                        "buffer", omx_msg.u.extended_buffer_data.buffer);
                msg->setInt32(
                        "range_offset",
                        omx_msg.u.extended_buffer_data.range_offset);
                msg->setInt32(
                        "range_length",
                        omx_msg.u.extended_buffer_data.range_length);
                msg->setInt32(
                        "flags",
                        omx_msg.u.extended_buffer_data.flags);
                msg->setInt64(
                        "timestamp",
                        omx_msg.u.extended_buffer_data.timestamp);
                msg->setPointer(
                        "platform_private",
                        omx_msg.u.extended_buffer_data.platform_private);
                msg->setPointer(
                        "data_ptr",
                        omx_msg.u.extended_buffer_data.data_ptr);
                break;
            }

            default:
                TRESPASS();
                break;
        }

        msg->post();
    }

protected:
    virtual ~VppObserver() {}

private:
    sp<AMessage> mNotify;

    DISALLOW_EVIL_CONSTRUCTORS(VppObserver);
};

////////////////////////////////////////////////////////////////////////////////

//static
int32_t VPP::HasEncCtx = false;

VPP::VPP(const sp<ACodec> &codec) :
            mReplyId(0),
            mState(UNINIT),
            mPreState(UNINIT),
            mNotify(NULL),
            mNode(NULL),
            mOMX(NULL),
            mNativeWindow(NULL),
            mCodec(codec),
            mFilterTypes(0),
            mFrameRate(0),
            m3PConfiged(false)
{
}

VPP::~VPP() {
    HasEncCtx = false;
    if (mState != UNINIT) {
        CHECK_EQ(mOMX->freeNode(mNode), (status_t) OK);
        changeState(UNINIT);
    }
}

void VPP::setNotificationMessage(const sp<AMessage> &msg) {
    mNotify = msg;
}

void VPP::initiateAllocateComponent(const sp<AMessage> &msg) {
    msg->setWhat(kWhatAllocateComponent);
    msg->setTarget(id());
    msg->post();
}

void VPP::initiateConfigure(const sp<AMessage> &msg) {
    msg->setWhat(kWhatConfigure);
    msg->setTarget(id());
    msg->post();
}

void VPP::initiateStart(const sp<AMessage> &msg) {
    msg->setWhat(kWhatStart);
    msg->setTarget(id());
    msg->post();
}

void VPP::initiateDisablePorts(const sp<AMessage> &msg) {
    msg->setWhat(kWhatDisablePort);
    msg->setTarget(id());
    sp<AMessage> resp;
    msg->postAndAwaitResponse(&resp);
    LOGI("got onDisablePort reply");
}

void VPP::initiateEnablePorts(const sp<AMessage> &msg) {
    msg->setWhat(kWhatEnablePort);
    msg->setTarget(id());
    msg->post();
}

void VPP::emptyBuffer(const sp<AMessage> &msg) {
    msg->setWhat(kWhatEmptyBuffer);
    msg->setTarget(id());
    msg->post();
}

void VPP::initiateShutdown(const bool keepComponentAllocated) {
    sp<AMessage> msg = new AMessage(kWhatShutdown, id());
    msg->setInt32(kKeyKeepCompAlloc, keepComponentAllocated);
    msg->post();
}

void VPP::signalFlush() {
    (new AMessage(kWhatFlush, id()))->post();
}

void VPP::signalResume() {
    (new AMessage(kWhatResume, id()))->post();
}

void VPP::onMessageReceived(const sp<AMessage> &msg) {
    uint32_t what = msg->what();
    switch (what) {
        case kWhatAllocateComponent:
        {
            if (mState == UNINIT) {
                onAllocateComponent(msg);
            }
            break;
        }

        case kWhatConfigure:
        {
            if (mState == LOADED) {
                onConfigure(msg);
            }
            break;
        }

        case kWhatStart:
        {
            if (mState == LOADED || mState == PORTSETTINGCHANGE) {
                if (mState == LOADED) {
                    changeState(LOADEDTOIDLE);
                    changeOMXState();
                }
                onStart(msg);
            }
            break;
        }

        case kWhatDisablePort:
        {
            LOGI("kWhatDisablePort mState = %d", mState);
            if (mState == LOADEDTOIDLE || mState == IDLETOEXECUTE || mState == EXECUTE) {
                onDisablePorts(msg);
            }
            break;
        }

        case kWhatEnablePort:
        {
            if (mState == PORTSETTINGCHANGE) {
                onEnablePorts(msg);
            }
            break;
        }

        case kWhatEmptyBuffer:
        {
            onEmptyBuffer(msg);
            break;
        }

        case kWhatBufferDrained:
        {
            onBufferDrained(msg);
            break;
        }
        
        case kWhatShutdown:
        {
            int32_t keep;
            CHECK(msg->findInt32(kKeyKeepCompAlloc, &keep));
            mKeepComponentAllocated = keep;

            LOGI("kWhatShutdown mState = %d", mState);
            switch(mState)
            {
                case UNINIT:
                case LOADED:
                {
                    onShutdown();
                    break;
                }
                case LOADEDTOIDLE:
                case IDLETOEXECUTE:
                case FLUSH:
                case PORTSETTINGCHANGE:
                {
                    deferMessage(msg);
                    break;
                }
                case EXECUTE:
                {
                    changeState(EXECUTETOIDLE);
                    changeOMXState();
                    break;
                }
                default:
                    break;
            }
            break;
        }

        case kWhatFlush:
        {
            onFlush(msg);
            break;
        }

        //only use to resume from flush
        case kWhatResume:
        {
            enterExecuteState();
            break;
        }

        case kWhatOMXMessage:
        {
            onOMXMessage(msg);
            break;
        }

        default:
        {
            break;
        }
    }

}


void VPP::signalError(OMX_ERRORTYPE error, status_t internalError) {
    LOGE("signalError");
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatError);
    notify->setInt32("omx-error", error);
    notify->setInt32("err", internalError);
    notify->post();
}

bool VPP::onOMXMessage(const sp<AMessage> &msg) {
    int32_t type;
    CHECK(msg->findInt32("type", &type));

    IOMX::node_id nodeID;
    CHECK(msg->findPointer("node", &nodeID));
    CHECK_EQ(nodeID, mNode);
    LOGI("onOMXMessage type = %d", type);

    switch (type) {
        case omx_message::EVENT:
        {
            int32_t event, data1, data2;
            CHECK(msg->findInt32("event", &event));
            CHECK(msg->findInt32("data1", &data1));
            CHECK(msg->findInt32("data2", &data2));

            return onOMXEvent(
                    static_cast<OMX_EVENTTYPE>(event),
                    static_cast<OMX_U32>(data1),
                    static_cast<OMX_U32>(data2));
        }


        case omx_message::EMPTY_BUFFER_DONE:
        {
            IOMX::buffer_id bufferID;
            CHECK(msg->findPointer("buffer", &bufferID));

            return onOMXEmptyBufferDone(bufferID);
        } 

        case omx_message::FILL_BUFFER_DONE:
        {
            IOMX::buffer_id bufferID;
            CHECK(msg->findPointer("buffer", &bufferID));

            int32_t rangeOffset, rangeLength, flags;
            int64_t timeUs;
            void *platformPrivate;
            void *dataPtr;
            
            CHECK(msg->findInt32("range_offset", &rangeOffset));
            CHECK(msg->findInt32("range_length", &rangeLength));
            CHECK(msg->findInt32("flags", &flags));
            CHECK(msg->findInt64("timestamp", &timeUs));
            CHECK(msg->findPointer("platform_private", &platformPrivate));
            CHECK(msg->findPointer("data_ptr", &dataPtr));

            LOGI("========= flags = 0x%x, timestamp = %lld", flags, timeUs);
            return onOMXFillBufferDone(
                    bufferID,
                    (size_t)rangeOffset, (size_t)rangeLength,
                    (OMX_U32)flags,
                    timeUs,
                    platformPrivate,
                    dataPtr);
        }

        default:
            break;
    }
    return true;
}

bool VPP::onOMXEvent(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2) {
    LOGI("event = %d, data1 = %d, data2 = %d", event, data1, data2);
    switch (event) {
        case OMX_EventCmdComplete:
        {
            if (data1 == OMX_CommandStateSet) {
                if (mState == PORTSETTINGCHANGE) {
                    break;
                }

                if (data2 == OMX_StateLoaded) {
                    changeState(LOADED);
                    onShutdown();

                } else if (data2 == OMX_StateIdle) {
                    CHECK((mState == LOADEDTOIDLE) || (mState == EXECUTETOIDLE));
                    if (mState == LOADEDTOIDLE)
                        changeState(IDLETOEXECUTE);
                    else if (mState == EXECUTETOIDLE) {
                        changeState(IDLETOLOADED);
                        freeBuffersNotOwnedByVPP(kPortIndexInput);
                        freeBuffersNotOwnedByVPP(kPortIndexOutput);
                    }
                    changeOMXState();

                } else if (data2 == OMX_StateExecuting) {
                    CHECK_EQ(mState, (uint32_t)IDLETOEXECUTE);
                    enterExecuteState();
                }

            } else if (data1 == OMX_CommandPortDisable) {
                LOGI("port disabled data2 = %d", data2);
                if (data2 == kPortIndexOutput) {
                    sp<AMessage> response = new AMessage;
                    response->postReply(mReplyId);
                }
                // because we have to wait for ACodec to do allocate buffer again,
                // so here we don't enable port by ourselves right after port disable,
                // instead, we are waiting caller(MediaCodec) to enable port again.

            } else if (data1 == OMX_CommandPortEnable) {
                LOGI("port enabled data2 = %d, mState = %d, mPreState = %d", data2, mState, mPreState);
                if (data2 == kPortIndexOutput) {
                    if (mPreState == LOADEDTOIDLE) {
                        changeState(IDLETOEXECUTE);
                        changeOMXState();
                    } else if (mPreState == IDLETOEXECUTE || mPreState == EXECUTE) {
                        enterExecuteState();
                    }
                }

            } else if (data1 == OMX_CommandFlush) {
                CHECK_EQ(mState, (uint32_t)FLUSH);
                if (data2 == kPortIndexInput || data2 == kPortIndexOutput) {
                    CHECK(!mFlushed[data2]);
                    mFlushed[data2] = true;
                    if (mFlushed[kPortIndexInput]
                            && mFlushed[kPortIndexOutput]) {
                        onFlushed();
                    }
                } else {
                    CHECK_EQ(data2, OMX_ALL);
                    CHECK(mFlushed[kPortIndexInput]);
                    CHECK(mFlushed[kPortIndexOutput]);
                    onFlushed();
                }
            }

            break;
        }
        default:
            break;
    }
    return true;
}

bool VPP::onOMXEmptyBufferDone(IOMX::buffer_id bufferID) {

    ssize_t index;
    VPPBufferInfo *info = findBufferByID(kPortIndexInput, bufferID, &index);
    ALOGI("[%s] onOMXEmptyBufferDone %p, decID = %p, mState = %d", mComponentName.c_str(), bufferID, info->mDecBufferID, mState);

    CHECK_EQ((int)info->mStatus, (int)VPPBufferInfo::OWNED_BY_VPP);
    info->mStatus = VPPBufferInfo::OWNED_BY_US;

    switch (mState) {
        case EXECUTE:
        case EXECUTETOIDLE:
        case IDLETOLOADED:
        {
            info->mNotify->post();
            info->mNotify = NULL;
            break;
        }

        //case FLUSH:
        default:
        {
            break;
        }
    }

    return true;
}

bool VPP::onOMXFillBufferDone(IOMX::buffer_id bufferID,
            size_t rangeOffset, size_t rangeLength, OMX_U32 flags,
            int64_t timeUs, void *platformPrivate, void *dataPtr) {
    ALOGI("[%s] onOMXFillBufferDone %p time %lld us, flags = 0x%08lx, mState = %d",
         mComponentName.c_str(), bufferID, timeUs, flags, mState);

    ssize_t index;
    VPPBufferInfo *info = findBufferByID(kPortIndexOutput, bufferID, &index);

    CHECK_EQ((int)info->mStatus, (int)VPPBufferInfo::OWNED_BY_VPP);
    info->mStatus = VPPBufferInfo::OWNED_BY_US;

    switch (mState) {
        case PORTSETTINGCHANGE:
        {
            CHECK_EQ((status_t)OK, freeBuffer(kPortIndexOutput, index));
            break;
        }

        case EXECUTE:
        {
            if (rangeLength == 0 && !(flags & OMX_BUFFERFLAG_EOS)) {
                ALOGI("[%s] calling fillBuffer %p when rangeLength = 0", mComponentName.c_str(), info->mBufferID);

                CHECK_EQ(mOMX->fillBuffer(mNode, info->mBufferID), (status_t)OK);
                info->mStatus = VPPBufferInfo::OWNED_BY_VPP;
                break;
            }

            sp<AMessage> reply =
                new AMessage(kWhatBufferDrained, id());

            info->mData->setRange(rangeOffset, rangeLength);
            info->mData->meta()->setInt64("timeUs", timeUs);

            LOGI("post message to drain buffer %p, vpp buf id = %p, time = %lld", 
                    info->mDecBufferID, info->mBufferID, timeUs);

            sp<AMessage> notify = mNotify->dup();
            notify->setInt32("what", kWhatDrainBuffer);
            notify->setPointer("buffer-id", info->mBufferID);
            notify->setBuffer("buffer", info->mData);
            notify->setInt32("flags", flags);

            reply->setPointer("buffer-id", info->mBufferID);

            notify->setMessage("reply", reply);

            notify->post();

            info->mStatus = VPPBufferInfo::OWNED_BY_DOWNSTREAM;

            if (flags & OMX_BUFFERFLAG_EOS) {
                ALOGV("[%s] saw output EOS", mComponentName.c_str());
                //TODO: since MediaCodec does not deal with this EOS, so no to send it now
                /*
                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", ACodec::kWhatEOS);
                notify->setInt32("err", mCodec->mInputEOSResult);
                notify->post();

                mCodec->mPortEOS[kPortIndexOutput] = true;
                */
            }

            break;
        }

        case FLUSH:
        default:
        {
            break;
        }
    }

    return true;
}


bool VPP::onAllocateComponent(const sp<AMessage> &msg) {
    LOGI("VPP::onAllocateComponent");
    CHECK(mNode == NULL);

    OMXClient client;
    CHECK_EQ(client.connect(), (status_t)OK);

    sp<IOMX> omx = client.interface();

    sp<VppObserver> observer = new VppObserver();
    IOMX::node_id node = NULL;

    pid_t tid = androidGetTid();
    int prevPriority = androidGetThreadPriority(tid);
    androidSetThreadPriority(tid, ANDROID_PRIORITY_FOREGROUND);
    status_t err = omx->allocateNode("OMX.Intel.Video.PostProcess", observer, &node);
    androidSetThreadPriority(tid, prevPriority);

    if (node == NULL) {
        ALOGE("Unable to instantiate OMX.Intel.Video.PostProcess component");

        signalError(OMX_ErrorComponentNotFound);
        return false;
    }

    sp<AMessage> omxNotify = new AMessage(kWhatOMXMessage, id());
    observer->setNotificationMessage(omxNotify);

    mOMX = omx;
    mNode = node;
    mComponentName = AString("OMX.Intel.Video.PostProcess");

    //get buffer count from VPP component
    OMX_PARAM_PORTDEFINITIONTYPE vppInDef;
    InitOMXParams(&vppInDef);
    vppInDef.nPortIndex = kPortIndexInput;
    err = mOMX->getParameter(
            mNode, OMX_IndexParamPortDefinition, &vppInDef, sizeof(vppInDef));

    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }

    OMX_PARAM_PORTDEFINITIONTYPE vppOutDef;
    InitOMXParams(&vppOutDef);
    vppOutDef.nPortIndex = kPortIndexOutput;
    err = mOMX->getParameter(
            mNode, OMX_IndexParamPortDefinition, &vppOutDef, sizeof(vppOutDef));

    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatComponentAllocated);
    notify->setInt32(kKeyVppInBufNum, vppInDef.nBufferCountMin);
    notify->setInt32(kKeyVppOutBufNum, vppOutDef.nBufferCountMin);
    notify->post();
    LOGI("vpp %d, %d", vppInDef.nBufferCountMin, vppOutDef.nBufferCountMin);

    changeState(LOADED);

    return true;
}

bool VPP::onConfigure(const sp<AMessage> &msg) {
    LOGI("VPP::onConfigure");
    CHECK(mNode != NULL);    

    sp<RefBase> obj;
    if (msg->findObject("native-window", &obj)) { 
        LOGE("vpp find native window");
        sp<NativeWindowWrapper> nativeWindow(
                static_cast<NativeWindowWrapper *>(obj.get()));
        CHECK(nativeWindow != NULL);
        mNativeWindow = nativeWindow->getNativeWindow();
    }

    if (msg->findInt32("frame-rate", &mFrameRate)) {
        LOGI("vpp find frame-rate = %d", mFrameRate);
    }

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatConfigured);
    notify->post();
    return true;

}

bool VPP::onStart(const sp<AMessage> &msg) {
    LOGI("VPP::onStart");
    CHECK(mNode != NULL);

    int32_t in = 0, out = 0, reserve = 0, all = 0;
    if (configComponent(&in, &out, &reserve, &all) != 0)
        return false;
    LOGI("in = %d, out = %d, reserve = %d, all = %d", in, out, reserve, all);

    if (setInputBuffersToOMX(in, all) != 0)
        return false;

    if (setOutputBuffersToOMX(out, reserve) != 0)
        return false;

    if (configFilters() != 0)
        return false;

    sp<ACodec::PortDescription> desc = new ACodec::PortDescription;
    for (uint32_t i = 0; i < mBuffers[kPortIndexOutput].size(); i++) {
        VPPBufferInfo * info = &mBuffers[kPortIndexOutput].editItemAt(i);
        info->mData->meta()->setPointer(kKeyDecodeBufID, info->mDecBufferID);
        desc->addBuffer(info->mBufferID, info->mData);
    }
    
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatStarted);
    notify->setObject("portDesc", desc);
    notify->post();
    return true;
}

bool VPP::onDisablePorts(const sp<AMessage> &msg) {

    LOGI("going to disable vpp input port");
    CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandPortDisable, kPortIndexInput),
            (status_t)OK);
    LOGI("going to disable vpp output port ");
    CHECK_EQ(mOMX->sendCommand(mNode,OMX_CommandPortDisable, kPortIndexOutput),
            (status_t)OK);

    freeBuffersNotOwnedByVPP(kPortIndexInput);
    freeBuffersNotOwnedByVPP(kPortIndexOutput);
    
    changeState(PORTSETTINGCHANGE);

    uint32_t replyId;
    CHECK(msg->senderAwaitsResponse(&replyId));
    mReplyId = replyId;
    // going to return message when output port disabled

    LOGI("onDisablePort returns");
    return true;
}

bool VPP::onEnablePorts(const sp<AMessage> &msg) {

    LOGI("going to enable vpp input port");
    CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandPortEnable, kPortIndexInput),
            (status_t)OK);
    LOGI("going to enable vpp output port ");
    CHECK_EQ(mOMX->sendCommand(mNode,OMX_CommandPortEnable, kPortIndexOutput),
            (status_t)OK);

    return true;
}

bool VPP::onEmptyBuffer(const sp<AMessage> &msg) {
    sp<AMessage> decoder;
    CHECK(msg->findMessage(kKeyDecodeMsg, &decoder));

    sp<ABuffer> buffer;
    CHECK(decoder->findBuffer("buffer", &buffer));
    int64_t timeUs;
    CHECK(buffer->meta()->findInt64("timeUs", &timeUs));

    IOMX::buffer_id decID;
    CHECK(decoder->findPointer("buffer-id", &decID));

    ssize_t index;
    VPPBufferInfo *info = findBufferByDecID(kPortIndexInput, decID, &index);
    CHECK_EQ((int)info->mStatus, (int)VPPBufferInfo::OWNED_BY_US);

    CHECK(decoder->findMessage("reply", &info->mNotify));

    int32_t flags;
    CHECK(decoder->findInt32("flags", &flags));

    int32_t fps = mFrameRate;
    if (!m3PConfiged) {
        config3PFilter(HasEncCtx, fps);
        m3PConfiged = true;
    }

    CHECK_EQ(mOMX->emptyBuffer(mNode, info->mBufferID, 0, info->mData->size(), flags, timeUs),
            (status_t)OK);
    info->mStatus = VPPBufferInfo::OWNED_BY_VPP;

    LOGI("onEmptyBuffer with time = %lld", timeUs);
    return true;
}

bool VPP::onBufferDrained(const sp<AMessage> &msg) {
    IOMX::buffer_id bufferID;
    CHECK(msg->findPointer("buffer-id", &bufferID));
    LOGI("buffer %p is drained!", bufferID);

    ssize_t index;
    VPPBufferInfo *info = findBufferByID(kPortIndexOutput, bufferID, &index);
    CHECK_EQ((int)info->mStatus, (int)VPPBufferInfo::OWNED_BY_DOWNSTREAM);

/*    android_native_rect_t crop;
    if (msg->findRect("crop",
            &crop.left, &crop.top, &crop.right, &crop.bottom)) {
        CHECK_EQ(0, native_window_set_crop(
                mCodec->mNativeWindow.get(), &crop));
    }*/

    int32_t render = 0;
    if (mNativeWindow != NULL
            && msg->findInt32("render", &render) && (render == 1)
            && (info->mData == NULL || info->mData->size() != 0)) {
        // The client wants this buffer to be rendered.

        status_t err;
        if ((err = mNativeWindow->queueBuffer(
                    mNativeWindow.get(),
                    info->mGraphicBuffer.get(), -1)) == OK) {
            info->mStatus = VPPBufferInfo::OWNED_BY_NATIVE_WINDOW;
        } else {
            signalError(OMX_ErrorUndefined, err);
            info->mStatus = VPPBufferInfo::OWNED_BY_US;
        }
    } else {
        info->mStatus = VPPBufferInfo::OWNED_BY_US;
    }
    LOGI("render = %d, info->mData->size() = %d", render, info->mData->size());

    switch (mState) {
        case FLUSH:
        {
            //do nothing
            break;
        }
        case EXECUTE:
        {
            if (info->mStatus == VPPBufferInfo::OWNED_BY_NATIVE_WINDOW) {
                // We cannot resubmit the buffer we just rendered, dequeue
                // the spare instead.

                info = dequeueBufferFromNativeWindow();
            }

            if (info != NULL) {
                ALOGI("[%s] calling fillBuffer %p", mComponentName.c_str(), info->mBufferID);

                CHECK_EQ(mOMX->fillBuffer(mNode, info->mBufferID), (status_t)OK);

                info->mStatus = VPPBufferInfo::OWNED_BY_VPP;
            }
            break;
        }

        case EXECUTETOIDLE:
        default:
        {
            CHECK_EQ((status_t)OK, freeBuffer(kPortIndexOutput, index));
            break;
        }
    }
    return true;
}

bool VPP::onFlush(const sp<AMessage> &msg) {
    switch(mState) {
        case UNINIT:
        case LOADED:
        case LOADEDTOIDLE:
        case IDLETOEXECUTE:
        {
            onFlushed();
            break;
        }
        
        case EXECUTE:
        {
            changeState(FLUSH);
            changeOMXState();
            mFlushed[kPortIndexInput] = false;
            mFlushed[kPortIndexOutput] = false;
            break;
        }
        
        case EXECUTETOIDLE:
        case IDLETOLOADED:
        {
            LOGE("should not be here");
            return false;
        }

        case PORTSETTINGCHANGE:
        {
            deferMessage(msg);
            break;
        }

        default:
        {
            break;
        }
    }
    return true;
}

bool VPP::onFlushed() {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatFlushCompleted);
    notify->post();
    return true;
}

bool VPP::onShutdown() {
    if (!mKeepComponentAllocated) {
        LOGI("going to free node");
        CHECK_EQ(mOMX->freeNode(mNode), (status_t) OK);

        changeState(UNINIT);
    }

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kWhatShutdownCompleted);
    notify->post();
    return true;
}

void VPP::deferMessage(const sp<AMessage> &msg) {
    LOGI("deferMessage");
    mDeferred.push_back(msg);
}

void VPP::processDeferredMessage() {
    List<sp<AMessage> > queue = mDeferred;
    mDeferred.clear();

    List<sp<AMessage> >::iterator it = queue.begin();
    while (it != queue.end()) {
        onMessageReceived(*it++);
    }
}

VPP::VPPBufferInfo *VPP::dequeueBufferFromNativeWindow() {
    ANativeWindowBuffer *buf;
    CHECK(mNativeWindow.get() != NULL);

    if (native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf) != 0) {
        ALOGE("dequeueBuffer failed.");
        return NULL;
    }

    for (size_t i = mBuffers[kPortIndexOutput].size(); i-- > 0;) {
        VPPBufferInfo *info =
            &mBuffers[kPortIndexOutput].editItemAt(i);

        if (info->mGraphicBuffer != NULL &&
            info->mGraphicBuffer->handle == buf->handle) {
            CHECK_EQ((int)info->mStatus,
                     (int)VPPBufferInfo::OWNED_BY_NATIVE_WINDOW);

            info->mStatus = VPPBufferInfo::OWNED_BY_US;
            LOGI("dequeueBufferFromNativeWindow %p", info->mBufferID);
            return info;
        }
    }

    LOGE("should not be here");
    return NULL;
}

void VPP::changeState(uint32_t state) {
    mPreState = mState;
    mState = state;
    return;
}

void VPP::changeOMXState() {
    switch (mState) {
        //case LOADED:
        case LOADEDTOIDLE:
            CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandStateSet, OMX_StateIdle), (status_t)OK);
            break;
        case IDLETOEXECUTE:
            CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandStateSet, OMX_StateExecuting), (status_t)OK);
            break;
        case EXECUTE:
            //do nothing
            break;
        case EXECUTETOIDLE:
            CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandStateSet, OMX_StateIdle), (status_t)OK);
            break;
        case IDLETOLOADED:
            CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandStateSet, OMX_StateLoaded), (status_t)OK);
            break;
        case FLUSH:
            CHECK_EQ(mOMX->sendCommand(mNode, OMX_CommandFlush, OMX_ALL), (status_t)OK);
            break;

        default:
            break;
    }
    return;
}

void VPP::submitOutputBuffers() {
    for (size_t i = 0; i < mBuffers[kPortIndexOutput].size(); ++i) {
        VPPBufferInfo *info = &mBuffers[kPortIndexOutput].editItemAt(i);

        if (mNativeWindow != NULL) {
            CHECK(info->mStatus == VPPBufferInfo::OWNED_BY_US
                    || info->mStatus == VPPBufferInfo::OWNED_BY_NATIVE_WINDOW);

            if (info->mStatus == VPPBufferInfo::OWNED_BY_NATIVE_WINDOW) {
                continue;
            }
        } 

        ALOGI("[%s] calling fillBuffer %p", mComponentName.c_str(), info->mBufferID);

        CHECK_EQ(mOMX->fillBuffer(mNode, info->mBufferID), (status_t)OK);

        info->mStatus = VPPBufferInfo::OWNED_BY_VPP;
    }
}

status_t VPP::freeBuffersNotOwnedByVPP(uint32_t portIndex) {
    LOGI("freeBuffersNotOwnedByVPP for port index = %d", portIndex);
    for (size_t i = mBuffers[portIndex].size(); i-- > 0;) {
        VPPBufferInfo *info =
            &mBuffers[portIndex].editItemAt(i);

        // At this time some buffers may still be with the component
        // or being drained.
        if (info->mStatus != VPPBufferInfo::OWNED_BY_VPP
                && info->mStatus != VPPBufferInfo::OWNED_BY_DOWNSTREAM) {
            CHECK_EQ((status_t)OK, freeBuffer(portIndex, i));
        }
    }

    return OK;
}

status_t VPP::freeBuffer(uint32_t portIndex, size_t i) {
    VPPBufferInfo *info = &mBuffers[portIndex].editItemAt(i);
    LOGI("[%s]: freeBuffer with bufferID = %p, status = %d, i = %d", 
            mComponentName.c_str(), info->mBufferID, info->mStatus, i);

    CHECK(info->mStatus == VPPBufferInfo::OWNED_BY_US
            || info->mStatus == VPPBufferInfo::OWNED_BY_NATIVE_WINDOW);

    if (portIndex == kPortIndexOutput && mNativeWindow != NULL
            && info->mStatus == VPPBufferInfo::OWNED_BY_US) {
        CHECK_EQ((status_t)OK, cancelBufferToNativeWindow(info));
    }

    CHECK_EQ(mOMX->freeBuffer(mNode, portIndex, info->mBufferID),
            (status_t)OK);

    mBuffers[portIndex].removeAt(i);

    return OK;
}

status_t VPP::cancelBufferToNativeWindow(VPPBufferInfo *info) {
    CHECK_EQ((int)info->mStatus, (int)VPPBufferInfo::OWNED_BY_US);

    ALOGI("[%s] Calling cancelBuffer on buffer %p",
         mComponentName.c_str(), info->mBufferID);

    int err = mNativeWindow->cancelBuffer(
        mNativeWindow.get(), info->mGraphicBuffer.get(), -1);

    CHECK_EQ(err, 0);

    info->mStatus = VPPBufferInfo::OWNED_BY_NATIVE_WINDOW;
    return OK;
}

VPP::VPPBufferInfo * VPP::findBufferByID(uint32_t portIndex, 
            IOMX::buffer_id bufferID, ssize_t *index) {
    for (size_t i = 0; i < mBuffers[portIndex].size(); ++i) {
        VPPBufferInfo *info = &mBuffers[portIndex].editItemAt(i);

        if (info->mBufferID == bufferID) {
            if (index != NULL) {
                *index = i;
            }
            return info;
        }
    }

    return NULL;
}

VPP::VPPBufferInfo * VPP::findBufferByDecID(uint32_t portIndex, 
            IOMX::buffer_id bufferID, ssize_t *index) {
    for (size_t i = 0; i < mBuffers[portIndex].size(); ++i) {
        VPPBufferInfo *info = &mBuffers[portIndex].editItemAt(i);

        if (info->mDecBufferID == bufferID) {
            if (index != NULL) {
                *index = i;
            }
            return info;
        }
    }

    return NULL;
}

status_t VPP::setInputBuffersToOMX(const uint32_t inBufNum, const uint32_t totalBufferNum) {
    status_t err = 0;
    ACodec::BufferInfo * info = NULL;
    for (uint32_t i = 0; i < totalBufferNum; i++) {
        info = &mCodec->mBuffers[kPortIndexOutput].editItemAt(i);
        LOGI("index = %d owned by %d", i, info->mStatus);
        if (info->mStatus != ACodec::BufferInfo::OWNED_BY_NATIVE_WINDOW) {
            //it is decoder buffer, should set to input port

            IOMX::buffer_id bufferId;
            err = mOMX->useGraphicBuffer(mNode, kPortIndexInput, info->mGraphicBuffer,
                &bufferId);
            LOGI("useGraphicBuffer vpp input %d/%d: buffer_id = %p, decid = %p", i, totalBufferNum, bufferId, info->mBufferID);
            if (err != 0) {
                ALOGI("registering GraphicBuffer %lu with OMX IL VPP component failed: "
                        "%d", i, err);
                signalError(OMX_ErrorUndefined);
                return err;
            }

            VPPBufferInfo vppIn;
            vppIn.mBufferID = bufferId;
            vppIn.mDecBufferID = info->mBufferID;
            vppIn.mStatus = VPPBufferInfo::OWNED_BY_US;
            vppIn.mData = info->mData;
            vppIn.mGraphicBuffer = info->mGraphicBuffer;
            mBuffers[kPortIndexInput].push(vppIn);

            //The other buffers shoule be vpp output buffer
        } else {
            VPPBufferInfo vppOut;
            vppOut.mDecBufferID = info->mBufferID;
            vppOut.mStatus = VPPBufferInfo::OWNED_BY_US;
            vppOut.mData = info->mData;
            //vppOut.mData->meta()->setPointer(kKeyDecodeBufID, vppOut.mDecBufferID);
            mBuffers[kPortIndexOutput].push(vppOut); 
        }
    }
    CHECK_EQ(inBufNum, mBuffers[kPortIndexInput].size());
    LOGI("-------------------------------------------------------------------------------");
    LOGI("input buffer num = %d, output buffer num = %d", 
            mBuffers[kPortIndexInput].size(), mBuffers[kPortIndexOutput].size());
    return 0;
}

status_t VPP::setOutputBuffersToOMX(const uint32_t bufferNum, const uint32_t cancelNum) {
    CHECK_EQ(bufferNum, mBuffers[kPortIndexOutput].size());
    status_t err = 0;
    
    for (uint32_t i = 0; i < bufferNum; i++) {
        ANativeWindowBuffer *buf;
        if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf)) != 0) {
            ALOGE("dequeueBuffer failed.");
            signalError(OMX_ErrorUndefined);
            return err;
        }

        sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
        //it could be vpp output buffer, only dequeue num = vppOutDef.nBufferCountActual
        IOMX::buffer_id bufferId;
        err = mOMX->useGraphicBuffer(mNode, kPortIndexOutput, graphicBuffer, &bufferId);
        LOGI("VPP output %d: buffer_id = %p", i, bufferId);
        if (err != 0) {
            ALOGE("registering GraphicBuffer with OMX IL component failed: %d", err);
            signalError(OMX_ErrorUndefined);
            return err;
        }
        VPPBufferInfo * vppInfo = &mBuffers[kPortIndexOutput].editItemAt(i);
        vppInfo->mBufferID = bufferId;
        vppInfo->mGraphicBuffer = graphicBuffer;
    }

    for (uint32_t i = 0; i < cancelNum; i ++) {
        VPPBufferInfo * info = &mBuffers[kPortIndexOutput].editItemAt(i);
        cancelBufferToNativeWindow(info);
    }
    return 0;
}

status_t VPP::configComponent(int32_t *inBufNum, int32_t *outBufNum,
        int32_t *reserveBufNum, int32_t *allNum) {
    int32_t bufNumAll = 0;
    int32_t bufSize = 0;
    int32_t reserve = 0;

    //get unqueued buffer number
    status_t err = mNativeWindow->query(mNativeWindow.get(),
            NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &reserve);
    if (err != OK) {
        ALOGE("NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)",
                strerror(-err), -err);
        signalError(OMX_ErrorUndefined);
        return err;
    }
    if (mCodec->mQuirks & OMXCodec::kRequiresHoldExtraBuffers) {
        reserve += 2;
    }

    //get decoder param and set to VPP
    OMX_PARAM_PORTDEFINITIONTYPE decDef;
    InitOMXParams(&decDef);
    decDef.nPortIndex = kPortIndexOutput;
    err = mOMX->getParameter(
            mCodec->mNode, OMX_IndexParamPortDefinition, &decDef, sizeof(decDef));
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }
    LOGI("width = %d, height = %d, colorformat = 0x%x, buffercount = %d, buffersize = %d",
            decDef.format.video.nFrameWidth,
            decDef.format.video.nFrameHeight,
            decDef.format.video.eColorFormat,
            decDef.nBufferCountActual,
            decDef.nBufferSize);

    bufNumAll = decDef.nBufferCountActual;
    bufSize = decDef.nBufferSize;

    //set buffer geometry for VPP output
    err = native_window_set_buffers_geometry(mNativeWindow.get(),
                        decDef.format.video.nFrameWidth,
                        decDef.format.video.nFrameHeight,
                        decDef.format.video.eColorFormat);
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }

    //configure VPP input/output port
    OMX_PARAM_PORTDEFINITIONTYPE vppInDef;
    InitOMXParams(&vppInDef);
    vppInDef.nPortIndex = kPortIndexInput;
    err = mOMX->getParameter(
            mNode, OMX_IndexParamPortDefinition, &vppInDef, sizeof(vppInDef));
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }
    vppInDef.format.video.nFrameWidth = decDef.format.video.nFrameWidth;
    vppInDef.format.video.nFrameHeight = decDef.format.video.nFrameHeight;
    vppInDef.format.video.eColorFormat = decDef.format.video.eColorFormat;

    OMX_PARAM_PORTDEFINITIONTYPE vppOutDef;
    InitOMXParams(&vppOutDef);
    vppOutDef.nPortIndex = kPortIndexOutput;
    err = mOMX->getParameter(
            mNode, OMX_IndexParamPortDefinition, &vppOutDef, sizeof(vppOutDef));

    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        return err;
    }

    vppInDef.nBufferCountActual = bufNumAll - vppOutDef.nBufferCountMin - reserve;

    err = mOMX->setParameter(
            mNode, OMX_IndexParamPortDefinition, &vppInDef, sizeof(vppInDef));
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        ALOGE("setting nBufferCountActual to %lu failed: %d",
                vppInDef.nBufferCountActual, err);
        return err;
    }
    vppOutDef.format.video.nFrameWidth = decDef.format.video.nFrameWidth;
    vppOutDef.format.video.nFrameHeight = decDef.format.video.nFrameHeight;
    vppOutDef.format.video.eColorFormat = decDef.format.video.eColorFormat;


    vppOutDef.nBufferCountActual = vppOutDef.nBufferCountMin + reserve;
    LOGI("vpp output count = %d, min = %d", vppOutDef.nBufferCountActual, vppOutDef.nBufferCountMin);
    err = mOMX->setParameter(
            mNode, OMX_IndexParamPortDefinition, &vppOutDef, sizeof(vppOutDef));
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        ALOGE("setting nBufferCountActual to %lu failed: %d",
                vppOutDef.nBufferCountActual, err);
        return err;
    }

    *inBufNum = vppInDef.nBufferCountActual;
    *outBufNum = vppOutDef.nBufferCountActual;
    *reserveBufNum = reserve;
    *allNum = bufNumAll;

    return err;
}

status_t VPP::configFilters() {
    status_t err = 0;
    OMX_INTEL_CONFIG_FILTERTYPE filterTypes;
    InitOMXParams(&filterTypes);
    err = mOMX->getConfig(
            mNode, (OMX_INDEXTYPE)OMX_INTEL_IndexConfigFilterType, (void*) &filterTypes, sizeof(OMX_INTEL_CONFIG_FILTERTYPE));
    if (err != OK) {
        signalError(OMX_ErrorUndefined);
        ALOGE("get filterTypes failed: %d", err);
        return err;
    }
    LOGI("supported Filter types = 0x%x", filterTypes.nFilterType);

    //Denoise
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilterDenoise) {
        OMX_INTEL_CONFIG_DENOISETYPE denoise;
        InitOMXParams(&denoise);
        denoise.nLevel = 32;
        err = mOMX->setConfig(mNode, (OMX_INDEXTYPE)OMX_INTEL_IndexConfigDenoiseLevel,
                (void*) &denoise, sizeof(OMX_INTEL_CONFIG_DENOISETYPE));
        if (err != OK) {
            signalError(OMX_ErrorUndefined);
            ALOGE("setConfig OMX_IndexConfigDenoiseLevel failed: %d", err);
            return err;
        }
    }

    //Deinterlace
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilterDeinterlace) {

    }

    //Sharpness
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilterSharpness) {

    }

    //Scale
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilterScale) {

    }

    //ColorBalance
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilterColorBalance) {

    }

    //Intel3P should be config while we got first decoder frame
    //we need video source info there
    if (filterTypes.nFilterType & OMX_INTEL_ImageFilter3P) {

    }

    mFilterTypes = filterTypes.nFilterType;

    return err;
}

void VPP::enterExecuteState() {
    mFlushed[kPortIndexInput] = false;
    mFlushed[kPortIndexOutput] = false;
    changeState(EXECUTE);
    submitOutputBuffers();
    processDeferredMessage();
    return;
}

status_t VPP::config3PFilter(int32_t hasEncoder, int32_t frameRate) {
    status_t err = 0;

    if (mFilterTypes & OMX_INTEL_ImageFilter3P) {
        LOGI("going to config3PFilter hasEncoder = %d", hasEncoder);
        //config 3P
        OMX_INTEL_CONFIG_INTEL3PTYPE intel3P;
        InitOMXParams(&intel3P);
        intel3P.nHasEncoder = hasEncoder;
        intel3P.nFrameRate = mFrameRate;

        err = mOMX->setConfig(mNode, (OMX_INDEXTYPE)OMX_INTEL_IndexConfigIntel3PLevel,
                (void*) &intel3P, sizeof(OMX_INTEL_CONFIG_INTEL3PTYPE));
        if (err != OK) {
            signalError(OMX_ErrorUndefined);
            ALOGE("setConfig OMX_IndexConfigIntel3PLevel failed: %d", err);
            return err;
        }
    }

    return err;
}

} //namespace android
