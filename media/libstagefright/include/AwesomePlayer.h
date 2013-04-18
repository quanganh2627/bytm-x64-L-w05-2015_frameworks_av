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

#ifndef AWESOME_PLAYER_H_

#define AWESOME_PLAYER_H_

#include "HTTPBase.h"
#include "TimedEventQueue.h"

#ifdef TARGET_HAS_VPP
#include "VPPProcessor.h"
#include <media/stagefright/OMXCodec.h>
#endif

#ifdef LVSE
#include "LVAudioSource.h"
#endif

#include <media/MediaPlayerInterface.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/TimeSource.h>
#include <utils/threads.h>
#include <drm/DrmManagerClient.h>
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
#include <display/MultiDisplayClient.h>
#endif

#include <media/stagefright/MetaData.h> // for AAC aparams
extern "C" void timerCallback(union sigval);
extern "C" void timerCallbackEOS(union sigval);

namespace android {
#define OFFLOAD_PAUSED_TIMEOUT_DURATION  10000000  //in micro seconds
#define OFFLOAD_STANDBY_TIMEOUT_DURATION  3000000  //in micro seconds
struct AudioPlayer;
struct DataSource;
struct MediaBuffer;
struct MediaExtractor;
struct MediaSource;
struct NuCachedSource2;
struct ISurfaceTexture;

class DrmManagerClinet;
class DecryptHandle;

class TimedTextDriver;
struct WVMExtractor;

struct AwesomeRenderer : public RefBase {
    AwesomeRenderer() {}

    virtual void render(MediaBuffer *buffer, void *platformPrivate = NULL) = 0;

private:
    AwesomeRenderer(const AwesomeRenderer &);
    AwesomeRenderer &operator=(const AwesomeRenderer &);
};

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
struct IntelPlatformPrivate {
    int usage;
};
#endif

struct AwesomePlayer {
    AwesomePlayer();
    ~AwesomePlayer();

    void setListener(const wp<MediaPlayerBase> &listener);
    void setUID(uid_t uid);

    status_t setDataSource(
            const char *uri,
            const KeyedVector<String8, String8> *headers = NULL);

    status_t setDataSource(int fd, int64_t offset, int64_t length);

    status_t setDataSource(const sp<IStreamSource> &source);

    void reset();

    status_t prepare();
    status_t prepare_l();
    status_t prepareAsync();
    status_t prepareAsync_l();

    status_t play();
    status_t pause();

    bool isPlaying() const;

    status_t setSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture);
    void setAudioSink(const sp<MediaPlayerBase::AudioSink> &audioSink);
    status_t setLooping(bool shouldLoop);

    status_t getDuration(int64_t *durationUs);
    status_t getPosition(int64_t *positionUs);

    status_t setParameter(int key, const Parcel &request);
    status_t getParameter(int key, Parcel *reply);
    status_t invoke(const Parcel &request, Parcel *reply);
    status_t setCacheStatCollectFreq(const Parcel &request);

    status_t seekTo(int64_t timeUs);

    // This is a mask of MediaExtractor::Flags.
    uint32_t flags() const;

    void postAudioEOS(int64_t delayUs = 0ll);
    void postAudioSeekComplete();

    status_t dump(int fd, const Vector<String16> &args) const;
    void offloadPauseStartTimer(int64_t time, bool at_pause = false);
    status_t offloadSuspend();
    status_t offloadResume();
#ifdef BGM_ENABLED
    status_t remoteBGMSuspend();
    status_t remoteBGMResume();
#endif
    bool mOffloadCalAudioEOS;
    bool mOffloadPostAudioEOS;
    void postAudioOffloadTearDown();
    status_t tearDownToNonDeepBufferAudio();
private:
    friend struct AwesomeEvent;
    friend struct PreviewPlayer;

    enum {
        PLAYING             = 0x01,
        LOOPING             = 0x02,
        FIRST_FRAME         = 0x04,
        PREPARING           = 0x08,
        PREPARED            = 0x10,
        AT_EOS              = 0x20,
        PREPARE_CANCELLED   = 0x40,
        CACHE_UNDERRUN      = 0x80,
        AUDIO_AT_EOS        = 0x0100,
        VIDEO_AT_EOS        = 0x0200,
        AUTO_LOOPING        = 0x0400,

        // We are basically done preparing but are currently buffering
        // sufficient data to begin playback and finish the preparation phase
        // for good.
        PREPARING_CONNECTED = 0x0800,

        // We're triggering a single video event to display the first frame
        // after the seekpoint.
        SEEK_PREVIEW        = 0x1000,

        AUDIO_RUNNING       = 0x2000,
        AUDIOPLAYER_STARTED = 0x4000,

        INCOGNITO           = 0x8000,

        TEXT_RUNNING        = 0x10000,
        TEXTPLAYER_INITIALIZED  = 0x20000,

        SLOW_DECODER_HACK   = 0x40000,
    };

    mutable Mutex mLock;
    Mutex mMiscStateLock;
    mutable Mutex mStatsLock;
    Mutex mAudioLock;

    OMXClient mClient;
    TimedEventQueue mQueue;
    bool mQueueStarted;
    wp<MediaPlayerBase> mListener;
    bool mUIDValid;
    uid_t mUID;

    sp<ANativeWindow> mNativeWindow;
    sp<MediaPlayerBase::AudioSink> mAudioSink;

    SystemTimeSource mSystemTimeSource;
    TimeSource *mTimeSource;

    String8 mUri;
    KeyedVector<String8, String8> mUriHeaders;

    sp<DataSource> mFileSource;

    sp<MediaSource> mVideoTrack;
    sp<MediaSource> mVideoSource;
    sp<AwesomeRenderer> mVideoRenderer;
    bool mVideoRenderingStarted;
    bool mVideoRendererIsPreview;

    ssize_t mActiveAudioTrackIndex;
    sp<MediaSource> mAudioTrack;
    sp<MediaSource> mAudioSource;
    AudioPlayer *mAudioPlayer;
    int64_t mDurationUs;

    int32_t mDisplayWidth;
    int32_t mDisplayHeight;
    int32_t mVideoScalingMode;

    uint32_t mFlags;
    uint32_t mExtractorFlags;
    uint32_t mSinceLastDropped;

    int64_t mTimeSourceDeltaUs;
    int64_t mVideoTimeUs;

    enum SeekType {
        NO_SEEK,
        SEEK,
        SEEK_VIDEO_ONLY
    };
    SeekType mSeeking;

    bool mSeekNotificationSent;
    int64_t mSeekTimeUs;

    int64_t mBitrate;  // total bitrate of the file (in bps) or -1 if unknown.

    bool mWatchForAudioSeekComplete;
    bool mWatchForAudioEOS;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    MultiDisplayClient* mMDClient;
    sp<ANativeWindow> mDefaultNativeWindow;
    int mFramesToDirty;
    uint32_t mRenderedFrames;
#endif

    sp<TimedEventQueue::Event> mVideoEvent;
    bool mVideoEventPending;
    sp<TimedEventQueue::Event> mStreamDoneEvent;
    bool mStreamDoneEventPending;
    sp<TimedEventQueue::Event> mBufferingEvent;
    bool mBufferingEventPending;
    sp<TimedEventQueue::Event> mCheckAudioStatusEvent;
    bool mAudioStatusEventPending;
    sp<TimedEventQueue::Event> mVideoLagEvent;
    bool mVideoLagEventPending;

    sp<TimedEventQueue::Event> mAsyncPrepareEvent;
    Condition mPreparedCondition;
    bool mIsAsyncPrepare;
    status_t mPrepareResult;
    status_t mStreamDoneStatus;

    void postVideoEvent_l(int64_t delayUs = -1);
    void postBufferingEvent_l();
    void postStreamDoneEvent_l(status_t status);
    void postCheckAudioStatusEvent(int64_t delayUs);
    void postVideoLagEvent_l();
    status_t play_l();

    MediaBuffer *mVideoBuffer;

    sp<HTTPBase> mConnectingDataSource;
    sp<NuCachedSource2> mCachedSource;

    DrmManagerClient *mDrmManagerClient;
    sp<DecryptHandle> mDecryptHandle;

#ifdef TARGET_HAS_VPP
    VPPProcessor *mVPPProcessor;
#endif

    int64_t mLastVideoTimeUs;
    TimedTextDriver *mTextDriver;

    sp<WVMExtractor> mWVMExtractor;
    sp<MediaExtractor> mExtractor;

    status_t setDataSource_l(
            const char *uri,
            const KeyedVector<String8, String8> *headers = NULL);

    status_t setDataSource_l(const sp<DataSource> &dataSource);
    status_t setDataSource_l(const sp<MediaExtractor> &extractor);
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    void notifyMDSPlayerStatus_l(int status);
    void setDisplaySource_l(bool isplaying);
#endif
    void reset_l();
    status_t seekTo_l(int64_t timeUs);
    status_t pause_l(bool at_eos = false);
    void initRenderer_l();
    void notifyVideoSize_l();
    void seekAudioIfNecessary_l();

    void cancelPlayerEvents(bool keepNotifications = false);

    void setAudioSource(sp<MediaSource> source);
    status_t initAudioDecoder();

    void setVideoSource(sp<MediaSource> source);
#ifdef TARGET_HAS_VPP
    VPPProcessor* createVppProcessor_l();
#endif
    status_t initVideoDecoder(uint32_t flags = 0);

    void addTextSource_l(size_t trackIndex, const sp<MediaSource>& source);

    void onStreamDone();

    void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);

    void onVideoEvent();
    void onBufferingUpdate();
    void onCheckAudioStatus();
    void onPrepareAsyncEvent();
    void abortPrepare(status_t err);
    void finishAsyncPrepare_l();
    void onVideoLagUpdate();

    bool getCachedDuration_l(int64_t *durationUs, bool *eos);

    status_t finishSetDataSource_l();

    static bool ContinuePreparation(void *cookie);

    bool getBitrate(int64_t *bitrate);

    void finishSeekIfNecessary(int64_t videoTimeUs);
    void ensureCacheIsFetching_l();

    status_t startAudioPlayer_l(bool sendErrorNotification = true);

    void shutdownVideoDecoder_l();
    status_t setNativeWindow_l(const sp<ANativeWindow> &native);

    bool isStreamingHTTP() const;
    void sendCacheStats();
    void checkDrmStatus(const sp<DataSource>& dataSource);

    enum FlagMode {
        SET,
        CLEAR,
        ASSIGN
    };
    void modifyFlags(unsigned value, FlagMode mode);

    struct TrackStat {
        String8 mMIME;
        String8 mDecoderName;
    };

    // protected by mStatsLock
    struct Stats {
        int mFd;
        String8 mURI;
        int64_t mBitrate;

        KeyedVector<String8, String8> mUriHeaders;
        sp<DataSource> mFileSource;
        int64_t mPositionUs;
        int64_t mDurationUs; /* store the file duration */
        // FIXME:
        // These two indices are just 0 or 1 for now
        // They are not representing the actual track
        // indices in the stream.
        ssize_t mAudioTrackIndex;
        ssize_t mVideoTrackIndex;

        int64_t mNumVideoFramesDecoded;
        int64_t mNumVideoFramesDropped;
        int32_t mVideoWidth;
        int32_t mVideoHeight;
        int32_t mFrameRate;
        uint32_t mFlags;
        Vector<TrackStat> mTracks;
        bool mOffloadSinkCreationError;
    } mStats;

    status_t setVideoScalingMode(int32_t mode);
    status_t setVideoScalingMode_l(int32_t mode);
    status_t getTrackInfo(Parcel* reply) const;

    status_t selectAudioTrack_l(const sp<MediaSource>& source, size_t trackIndex);

    // when select is true, the given track is selected.
    // otherwise, the given track is unselected.
    status_t selectTrack(size_t trackIndex, bool select);

    size_t countTracks() const;

    AwesomePlayer(const AwesomePlayer &);
    AwesomePlayer &operator=(const AwesomePlayer &);
    void postAudioOffloadTearDownEvent_l();
    void onAudioOffloadTearDownEvent();
    bool isAudioEffectEnabled();
    bool isInCall();
    status_t createAudioPlayer(audio_format_t audioFormat,
                               int sampleRate,
                               int channelsCount,
                               int bitRate);
    status_t mapMimeToAudioFormat(audio_format_t *audioFormat, const char *mime);
    status_t setAACParameters(sp<MetaData> meta, audio_format_t *aFormat,
                              uint32_t *avgBitRate);

    audio_format_t mAudioFormat;
    bool mOffload;
    timer_t mPausedTimerId;
    bool mOffloadTearDown;
    bool mOffloadTearDownForPause;
    int64_t mOffloadPauseUs;
    sp<TimedEventQueue::Event> mAudioOffloadTearDownEvent;
    bool mAudioOffloadTearDownEventPending;
    bool mOffloadSinkCreationError;

    bool mDeepBufferAudio;
    bool mDeepBufferTearDown;

#ifdef BGM_ENABLED
    bool mRemoteBGMsuspend;
    bool mBGMEnabled;
#endif

#ifdef LVSE
    sp<LVAudioSource> mLVAudioSource;
#endif

};

}  // namespace android

#endif  // AWESOME_PLAYER_H_
