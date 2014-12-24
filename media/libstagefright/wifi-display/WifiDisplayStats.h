/*
 * INTEL CONFIDENTIAL
 * Copyright 2013 Intel Corporation All Rights Reserved.
 *
 * The source code, information and material ("Material") contained herein is owned
 * by Intel Corporation or its suppliers or licensors, and title to such Material
 * remains with Intel Corporation or its suppliers or licensors. The Material contains
 * proprietary information of Intel or its suppliers and licensors. The Material is
 * protected by worldwide copyright laws and treaty provisions. No part of the Material
 * may be used, copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed or disclosed in any way without Intel's prior express written permission.
 * No license under any patent, copyright or other intellectual property rights in the
 * Material is granted to or conferred upon you, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 *
 * Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any
 * other notice embedded in Materials by Intel or Intel's suppliers or licensors in any way.
 */

#ifndef WifiDisplay_STATS_H
#define WifiDisplay_STATS_H

#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <utils/Condition.h>
#include <utils/List.h>

#include <media/stagefright/MediaBuffer.h>

namespace android {

class WifiDisplayStats {

public:

    enum FrameType {
        FRAME_NEW,
        FRAME_DUPLICATE
    };

    // singleton creation method
    static WifiDisplayStats& getInstance();

    // start/stop statistics thread
    void start();
    void stop();

    // update the statistics
    void updateInputFPS(nsecs_t time);
    void updateOutputFPS(nsecs_t time);
    void updateVideoEncodeStartTime(nsecs_t time);
    void updateVideoEncodeEndTime(nsecs_t time);
    void updateVideoEncodeBits(int32_t bits);
    void updateFrameType(MediaBuffer * frame);
    void updateVideoPackStartTime(nsecs_t time);
    void updateVideoPackEndTime(nsecs_t time);
    //Audio input sample rate
    void updateInputSampleRate(nsecs_t time, int samples);
    //Audio sample rate at the converter output
    void updateOutputSampleRate(nsecs_t time, int samples);

private:

    // singleton hides
    WifiDisplayStats();
    WifiDisplayStats(const WifiDisplayStats &copy);
    WifiDisplayStats& operator=(const WifiDisplayStats &rhs);
    ~WifiDisplayStats();

    void reset();

    void checkStatsEnabled();
    void checkVerboseStatsEnabled();
    void checkInterval();

private:

    // thread for periodic statistics
    class StatsThread : public android::Thread {
    public:
        StatsThread(WifiDisplayStats &stats);
        virtual ~StatsThread();
    private:
        virtual bool threadLoop();
    private:
        WifiDisplayStats &mStats;
    };

    enum DataType {
        DATA_TYPE_BITS,
        DATA_TYPE_TIME,
        DATA_TYPE_FPS,
        DATA_TYPE_SR, //sample rate
    };

    // statistics data
    struct Data {
        Data(const char *label, DataType dataType);
        ~Data();
        void addSample(int64_t sample, int numcount = 1); // need 64 bits for timestamps
        void showStats(nsecs_t time);
        void reset();

        DataType type;
        const char *name;

        // stats
        int32_t count;
        int64_t min; // need 64 bits for timestamps
        int64_t max;
        int64_t sum;
    };

    friend class StatsThread;

private:

    int32_t mNumSeconds;

    // data
    Data mVideoEncodeTime;
    Data mInputFPS;
    Data mOutputFPS;
    Data mEncodeBits;
    Data mVideoPackTime;
    Data mAudioInputSampleRate;
    Data mAudioOutputSampleRate;
    int32_t mFrameNew;
    int32_t mFrameDuplicate;

    // thread stuff
    Condition mCondition;
    Mutex mMutex;
    sp<StatsThread> mThread;

    // singleton instance data
    static WifiDisplayStats mInstance;

    bool mStatsEnabled;
    bool mVerboseStatsEnabled;

    MediaBuffer *mLastFrame;
    List<int64_t> mVideoEncodeStartTimes;
    List<int64_t> mVideoPackStartTimes;
    int64_t mLastInputTime;
    int64_t mLastOutputTime;
    int64_t mAudioLastInputTime;
    int64_t mAudioLastOutputTime;


}; // class WifiDisplayStats

} // namespace android
#endif // WifiDisplay_STATS_H
