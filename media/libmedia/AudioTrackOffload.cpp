
//#define LOG_NDEBUG 0
#define LOG_TAG "AudioTrackOffload"

#include <stdint.h>
#include <sys/types.h>
#include <limits.h>

#include <sched.h>
#include <sys/resource.h>

#include <private/media/AudioTrackShared.h>

#include <media/AudioSystem.h>
#include <media/AudioTrack.h>
#include <media/AudioTrackOffload.h>

#include <utils/Log.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <utils/Timers.h>
#include <utils/Atomic.h>

#include <cutils/bitops.h>
#include <cutils/compiler.h>

#include <system/audio.h>
#include <system/audio_policy.h>

#include <audio_utils/primitives.h>

#ifdef USE_INTEL_SRC
#include "iasrc_resampler.h"
#endif

namespace android {
// ---------------------------------------------------------------------------

AudioTrackOffload::AudioTrackOffload()
{
    mStatus = NO_INIT;
    mIsTimed = false;
    mPreviousPriority = ANDROID_PRIORITY_NORMAL;
    mPreviousSchedulingGroup = SP_DEFAULT;
    mOutput = 0;
    mBitRate = 0;
    mOffloadEOSReached = 0;
}

//Constructor to support Offload.
AudioTrackOffload::AudioTrackOffload(
        audio_stream_type_t streamType,
        int bitRate,
        uint32_t sampleRate,
        audio_format_t format,
        int channelMask,
        int frameCount,
        audio_output_flags_t flags,
        callback_t cbf,
        void* user,
        int notificationFrames,
        int sessionId)
{
    mStatus = NO_INIT;
    mIsTimed = false;
    mPreviousPriority = ANDROID_PRIORITY_NORMAL;
    mPreviousSchedulingGroup = SP_DEFAULT;
    mOutput = 0;
    mBitRate = bitRate;
    mStatus = set(streamType, sampleRate, format, channelMask,
        frameCount, flags, cbf, user, notificationFrames,
        0, false, sessionId);
    mAudioTrack = AudioTrack::mAudioTrack;
}

status_t AudioTrackOffload::set(
        audio_stream_type_t streamType,
        uint32_t sampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        int frameCount,
        audio_output_flags_t flags,
        callback_t cbf,
        void* user,
        int notificationFrames,
        const sp<IMemory>& sharedBuffer,
        bool threadCanCallJava,
        int sessionId)
{
    ALOGV_IF(sharedBuffer != 0, "sharedBuffer: %p, size: %d", sharedBuffer->pointer(), sharedBuffer->size());

    ALOGV("set() streamType %d frameCount %d flags %04x", streamType, frameCount, flags);

    AutoMutex lock(mLock);
    if (mAudioTrack != 0) {
        ALOGE("Track already in use");
        return INVALID_OPERATION;
    }

    // handle default values first.
    if (streamType == AUDIO_STREAM_DEFAULT) {
        streamType = AUDIO_STREAM_MUSIC;
    }

    if (sampleRate == 0) {
        int afSampleRate;
        if (AudioSystem::getOutputSamplingRate(&afSampleRate, streamType) != NO_ERROR) {
            return NO_INIT;
        }
        sampleRate = afSampleRate;
    }

    // these below should probably come from the audioFlinger too...
    if (format == AUDIO_FORMAT_DEFAULT) {
        format = AUDIO_FORMAT_PCM_16_BIT;
    }
    if (channelMask == 0) {
        channelMask = AUDIO_CHANNEL_OUT_STEREO;
    }

    // validate parameters
    if (!audio_is_valid_format(format)) {
        ALOGE("Invalid format");
        return BAD_VALUE;
    }

    // AudioFlinger does not currently support 8-bit data in shared memory
    if (format == AUDIO_FORMAT_PCM_8_BIT && sharedBuffer != 0) {
        ALOGE("8-bit data in shared memory is not supported");
        return BAD_VALUE;
    }

    // force direct flag if format is not linear PCM
    if (!audio_is_linear_pcm(format)) {
        flags = (audio_output_flags_t)
                // FIXME why can't we allow direct AND fast?
                ((flags | AUDIO_OUTPUT_FLAG_DIRECT) & ~AUDIO_OUTPUT_FLAG_FAST);
    }
    // only allow deep buffering for music stream type
    if (streamType != AUDIO_STREAM_MUSIC) {
        flags = (audio_output_flags_t)(flags &~AUDIO_OUTPUT_FLAG_DEEP_BUFFER);
    }

    if (!audio_is_output_channel(channelMask)) {
        ALOGE("Invalid channel mask %#x", channelMask);
        return BAD_VALUE;
    }
    uint32_t channelCount = popcount(channelMask);

    if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        // Get offload buffer size and also sets bitrate for output stream in HAL
        int bufferSize = 0;
        if(mBitRate < 0) {
            mBitRate = 0;
            ALOGV("[AudioTrackOffload] set: mBitRate reset to %d", mBitRate);
        }

        bufferSize = getOffloadBufferSize(mBitRate, sampleRate, channelMask, NULL);
        if (bufferSize == 0) {
            // cannot offload with zero buffer size return track not initailized
            ALOGE("[AudioTrackOffload] Could not get offload buffer size for stream type %d", streamType);
            return NO_INIT;
        }
        ALOGV("[AudioTrackOffload] The offload buffer size is %d", bufferSize);
        frameCount = bufferSize; // For offload, buffer size is used as frame count.
    }

    audio_io_handle_t output = AudioSystem::getOutput(
                                    streamType,
                                    sampleRate, format, channelMask,
                                    flags);

    if (output == 0) {
        ALOGE("Could not get audio output for stream type %d", streamType);
        return BAD_VALUE;
    }

    mVolume[LEFT] = 1.0f;
    mVolume[RIGHT] = 1.0f;
    mSendLevel = 0.0f;
    mFrameCount = frameCount;
    mNotificationFramesReq = notificationFrames;
    mSessionId = sessionId;
    mAuxEffectId = 0;
    mFlags = flags;
    mCbf = cbf;

    // create the IAudioTrack
    status_t status = createTrack_l(streamType,
                                  sampleRate,
                                  format,
                                  channelMask,
                                  frameCount,
                                  flags,
                                  sharedBuffer,
                                  output);

    if (status != NO_ERROR) {
        if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
            AudioSystem::releaseOutput(output);
        }

        if (mAudioTrackThread != 0) {
            mAudioTrackThread->requestExit();
            mAudioTrackThread.clear();
        }
        return status;
    }

    if (cbf != NULL) {
        mAudioTrackThread = new AudioTrackThread(*this, threadCanCallJava);
        mAudioTrackThread->run("AudioTrack", ANDROID_PRIORITY_AUDIO, 0 /*stack*/);
    }

    mStatus = NO_ERROR;

    mStreamType = streamType;
    mFormat = format;
    mChannelMask = channelMask;
    mChannelCount = channelCount;
    mSharedBuffer = sharedBuffer;
    mMuted = false;
    mActive = false;
    mUserData = user;
    mLoopCount = 0;
    mMarkerPosition = 0;
    mMarkerReached = false;
    mNewPosition = 0;
    mUpdatePeriod = 0;
    mFlushed = false;
    AudioSystem::acquireAudioSessionId(mSessionId);
    mOutput = output;
    mOffloadEOSReached = false;
    mRestoreStatus = NO_ERROR;
    return NO_ERROR;

}

size_t AudioTrackOffload::getOffloadBufferSize(
        uint32_t bitRate,
        uint32_t sampleRate,
        uint32_t channel,
        audio_io_handle_t output)
{
    ALOGV("AudioTrackOffload::getOffloadBufferSize");
    const sp<IAudioFlinger>& audioFlinger = AudioSystem::get_audio_flinger();
    if (audioFlinger == 0) {
        ALOGE("[AudioTrackOffload] Could not get audioflinger");
        return 0;
    }
    return audioFlinger->getOffloadBufferSize(bitRate, sampleRate, channel, output);
}

status_t AudioTrackOffload::setParameters( const String8& keyValuePairs )
{
    if ((mAudioTrack != 0) && (mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)) {
        return mAudioTrack->setParameters( keyValuePairs );
    } else {
        return NO_INIT;
    }
}

status_t AudioTrackOffload::setVolume(float left, float right)
{
    if (left < 0.0f || left > 1.0f || right < 0.0f || right > 1.0f) {
        return BAD_VALUE;
    }

    AutoMutex lock(mLock);
    mVolume[LEFT] = left;
    mVolume[RIGHT] = right;

    mCblk->setVolumeLR((uint32_t(uint16_t(right * 0x1000)) << 16) | uint16_t(left * 0x1000));
    mAudioTrack->setVolume(left, right);

    return NO_ERROR;
}

void AudioTrackOffload::flush_l()
{
    ALOGV("flush_l");

    // clear playback marker and periodic update counter
    mMarkerPosition = 0;
    mMarkerReached = false;
    mUpdatePeriod = 0;

    if (!mActive || (mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)) {
        mFlushed = true;
        AudioTrack::mAudioTrack->flush();
        // Release AudioTrack callback thread in case it was waiting for new buffers
        // in AudioTrack::obtainBuffer()

        mOffloadEOSReached = false;
        mCblk->cv.signal();
    }
}

status_t AudioTrackOffload::getPosition(uint32_t *position)
{
    if (position == NULL) return BAD_VALUE;
    AutoMutex lock(mLock);
    if (!(mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)) {
        *position = mFlushed ? 0 : mCblk->server;
    } else {
        uint32_t dspFrames = 0;
        if (mOutput != 0) {
            uint32_t halFrames = 0;
            AudioSystem::getRenderPosition(mOutput, &halFrames, &dspFrames);
        }
        *position = dspFrames;
    }
    return NO_ERROR;
}

status_t AudioTrackOffload::setOffloadEOSReached(bool value)
{
    if (mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        mOffloadEOSReached = value;
        ALOGV("[AudioTrackOffload] mOffloadEOSReached: %x", mOffloadEOSReached);
        if (mAudioTrack != 0) {
            ALOGV("AudioTrackOffload::setOffloadEOSReached: calling setOffloadEOSReached");
            return mAudioTrack->setOffloadEOSReached(mOffloadEOSReached);
        }
    }
    return NO_INIT;
}

status_t AudioTrackOffload::obtainBuffer(Buffer* audioBuffer, int32_t waitCount)
{
    AutoMutex lock(mLock);
    bool active;
    status_t result = NO_ERROR;
    audio_track_cblk_t* cblk = mCblk;
    uint32_t framesReq = audioBuffer->frameCount;
    uint32_t waitTimeMs = (waitCount < 0) ? cblk->bufferTimeoutMs : WAIT_PERIOD_MS;

    audioBuffer->frameCount  = 0;
    audioBuffer->size = 0;

    uint32_t framesAvail = cblk->framesAvailable();

    cblk->lock.lock();
    if (cblk->flags & CBLK_INVALID_MSK) {
        // no need to clear the invalid flag as this cblk will not be used anymore
        if (cblk->flags & CBLK_OFFLOAD_TEAR_DOWN_MSK) {
            ALOGW("AudioTrackOffload::obtainBuffer() before loop, track %p invalidated. Tear down stream", this);
            cblk->lock.unlock();
            return TEAR_DOWN;
        }
        goto create_new_track;
    }
    cblk->lock.unlock();

    if (framesAvail == 0) {
        cblk->lock.lock();
        goto start_loop_here;
        while (framesAvail == 0) {
            active = mActive;
            if (CC_UNLIKELY(!active)) {
                ALOGV("Not active and NO_MORE_BUFFERS");
                cblk->lock.unlock();
                return NO_MORE_BUFFERS;
            }
            if (CC_UNLIKELY(!waitCount)) {
                cblk->lock.unlock();
                return WOULD_BLOCK;
            }
            if (!(cblk->flags & CBLK_INVALID_MSK)) {
                mLock.unlock();
                result = cblk->cv.waitRelative(cblk->lock, milliseconds(waitTimeMs));
                cblk->lock.unlock();
                mLock.lock();
                if (!mActive) {
                    return status_t(STOPPED);
                }
                cblk->lock.lock();
            }

            if (cblk->flags & CBLK_INVALID_MSK) {
                cblk->lock.unlock();
                // no need to clear the invalid flag as this cblk will not be used anymore
                if (cblk->flags & CBLK_OFFLOAD_TEAR_DOWN_MSK) {
                    ALOGW("AudioTrackOffload::obtainBuffer() track %p invalidated. Tear down stream", this);
                    return TEAR_DOWN;
                }
                goto create_new_track;
            }
            if (CC_UNLIKELY(result != NO_ERROR)) {
                cblk->waitTimeMs += waitTimeMs;
                if (!(mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) &&
                    cblk->waitTimeMs >= cblk->bufferTimeoutMs) {
                    // timing out when a loop has been set and we have already written upto loop end
                    // is a normal condition: no need to wake AudioFlinger up.
                    if (cblk->user < cblk->loopEnd) {
                        ALOGW(   "obtainBuffer timed out (is the CPU pegged?) %p name=%#x"
                                 "user=%08x, server=%08x", this, cblk->mName, cblk->user, cblk->server);
                        //unlock cblk mutex before calling mAudioTrack->start() (see issue #1617140)
                        cblk->lock.unlock();
                        result = mAudioTrack->start();
                        cblk->lock.lock();
                        if (result == DEAD_OBJECT) {
                            android_atomic_or(CBLK_INVALID_ON, &cblk->flags);
create_new_track:
                            result = restoreTrack_l(cblk, false);
                        }
                        if (result != NO_ERROR) {
                            ALOGW("obtainBuffer create Track error %d", result);
                            cblk->lock.unlock();
                            return result;
                        }
                    }
                    cblk->waitTimeMs = 0;
                }

                if (--waitCount == 0) {
                    cblk->lock.unlock();
                    return TIMED_OUT;
                }
            }
            // read the server count again
        start_loop_here:
            framesAvail = cblk->framesAvailable_l();
        }
        cblk->lock.unlock();
    }

    cblk->waitTimeMs = 0;

    if (framesReq > framesAvail) {
        framesReq = framesAvail;
    }

    uint32_t u = cblk->user;
    uint32_t bufferEnd = cblk->userBase + cblk->frameCount;

    if (framesReq > bufferEnd - u) {
        framesReq = bufferEnd - u;
    }

    audioBuffer->flags = mMuted ? Buffer::MUTE : 0;
    audioBuffer->channelCount = mChannelCount;
    audioBuffer->frameCount = framesReq;
    audioBuffer->size = framesReq * cblk->frameSize;
    if (audio_is_linear_pcm(mFormat)) {
        audioBuffer->format = AUDIO_FORMAT_PCM_16_BIT;
    } else {
        audioBuffer->format = mFormat;
    }
    audioBuffer->raw = (int8_t *)cblk->buffer(u);
    active = mActive;
    return active ? status_t(NO_ERROR) : status_t(STOPPED);
}

bool AudioTrackOffload::processAudioBuffer(const sp<AudioTrackThread>& thread)
{
    Buffer audioBuffer;
    uint32_t frames;
    size_t writtenSize = 0;
    mLock.lock();
    // acquire a strong reference on the IMemory and IAudioTrack so that they cannot be destroyed
    // while we are accessing the cblk
    sp<IAudioTrack> audioTrack = mAudioTrack;
    sp<IMemory> iMem = mCblkMemory;
    audio_track_cblk_t* cblk = mCblk;
    bool active = mActive;
    mLock.unlock();

    if ((mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) &&
        (mOffloadEOSReached) && (mCblk->flags & CBLK_OFFLOAD_STREAM_END_DONE)) {
        android_atomic_and(~CBLK_OFFLOAD_STREAM_END_DONE, &cblk->flags);
        mOffloadEOSReached = false;
        ALOGV("[AudioTrackOffload] Posting play complete");
        mCbf(EVENT_STREAM_END, mUserData, 0);
    }

    // Manage underrun callback
    if (!mOffloadEOSReached &&
        active && (cblk->framesAvailable() == cblk->frameCount)) {
        ALOGV("Underrun user: %x, server: %x, flags %04x", cblk->user, cblk->server, cblk->flags);
        if (!(android_atomic_or(CBLK_UNDERRUN_ON, &cblk->flags) & CBLK_UNDERRUN_MSK)) {
            mCbf(EVENT_UNDERRUN, mUserData, 0);
            if (cblk->server == cblk->frameCount) {
                mCbf(EVENT_BUFFER_END, mUserData, 0);
            }
            if (mSharedBuffer != 0) return false;
        }
    }

    // Manage loop end callback
    while (mLoopCount > cblk->loopCount) {
           int loopCount = -1;
           mLoopCount--;
           if (mLoopCount >= 0) loopCount = mLoopCount;

           mCbf(EVENT_LOOP_END, mUserData, (void *)&loopCount);
    }

    // Manage marker callback
    if (!mMarkerReached && (mMarkerPosition > 0)) {
        if (cblk->server >= mMarkerPosition) {
            mCbf(EVENT_MARKER, mUserData, (void *)&mMarkerPosition);
            mMarkerReached = true;
        }
    }

    // Manage new position callback
    if (mUpdatePeriod > 0) {
        while (cblk->server >= mNewPosition) {
               mCbf(EVENT_NEW_POS, mUserData, (void *)&mNewPosition);
               mNewPosition += mUpdatePeriod;
        }
    }

    // If Shared buffer is used, no data is requested from client.
    if (mSharedBuffer != 0) {
        frames = 0;
    } else {
        frames = mRemainingFrames;
    }

    // See description of waitCount parameter at declaration of obtainBuffer().
    // The logic below prevents us from being stuck below at obtainBuffer()
    // not being able to handle timed events (position, markers, loops).
    int32_t waitCount = -1;
    if (mUpdatePeriod || (!mMarkerReached && mMarkerPosition) || mLoopCount) {
        waitCount = 1;
    }

    do {

        audioBuffer.frameCount = frames;

        status_t err = obtainBuffer(&audioBuffer, waitCount);
        if (err < NO_ERROR) {
            if (err != TIMED_OUT) {
                if (err == status_t(TEAR_DOWN)) {
                    ALOGV("AudioTrackOffload::processAudioBuffer: Tear down in progress");
                    mCbf(EVENT_TEAR_DOWN, mUserData, NULL);
                } else {
                    ALOGE_IF(err != status_t(NO_MORE_BUFFERS), "Error obtaining an audio buffer, giving up.");
                }
                return false;
            }
            break;
        }
        if (err == status_t(STOPPED)) return false;

        // Divide buffer size by 2 to take into account the expansion
        // due to 8 to 16 bit conversion: the callback must fill only half
        // of the destination buffer
        if (mFormat == AUDIO_FORMAT_PCM_8_BIT && !(mFlags & AUDIO_OUTPUT_FLAG_DIRECT)) {
            audioBuffer.size >>= 1;
        }

        size_t reqSize = audioBuffer.size;
        mCbf(EVENT_MORE_DATA, mUserData, &audioBuffer);
        writtenSize = audioBuffer.size;

        // Sanity check on returned size
        if (ssize_t(writtenSize) <= 0) {
            // The callback is done filling buffers
            // Keep this thread going to handle timed events and
            // still try to get more data in intervals of WAIT_PERIOD_MS
            // but don't just loop and block the CPU, so wait
            if ((mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) && (mOffloadEOSReached)) {
                ALOGV("AudioTrackOffload::processAudioBuffer: EOS reached, sleeping for 100 ms");
                usleep(OFFLOAD_WAIT_PERIOD_MS*1000);
            } else {
               usleep(WAIT_PERIOD_MS*1000);
            }
            break;
        }

        if (writtenSize > reqSize) writtenSize = reqSize;

        if (mFormat == AUDIO_FORMAT_PCM_8_BIT && !(mFlags & AUDIO_OUTPUT_FLAG_DIRECT)) {
            // 8 to 16 bit conversion, note that source and destination are the same address
            memcpy_to_i16_from_u8(audioBuffer.i16, (const uint8_t *) audioBuffer.i8, writtenSize);
            writtenSize <<= 1;
        }

        audioBuffer.size = writtenSize;
        // NOTE: mCblk->frameSize is not equal to AudioTrack::frameSize() for
        // 8 bit PCM data: in this case,  mCblk->frameSize is based on a sample size of
        // 16 bit.
        audioBuffer.frameCount = writtenSize/mCblk->frameSize;

        frames -= audioBuffer.frameCount;

        releaseBuffer(&audioBuffer);
    }
    while (frames);

    if (frames == 0) {
        mRemainingFrames = mNotificationFramesAct;
    } else {
        mRemainingFrames = frames;
    }
    return true;
}

}; // namespace android
