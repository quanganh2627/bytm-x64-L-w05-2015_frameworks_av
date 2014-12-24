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
#define LOG_TAG "WifiDisplayStats"

#include <cutils/properties.h>
#include <utils/Log.h>
#include <time.h>
#include "WifiDisplayStats.h"

#include <media/stagefright/foundation/ADebug.h>

namespace android {

////////////////////////////////////////////////////////////////////////////////
// class: WifiDisplayStats
////////////////////////////////////////////////////////////////////////////////

// our singleton instance
WifiDisplayStats WifiDisplayStats::mInstance;

// convert nanoseconds to milliseconds (floating point)
static inline float ns2msf(nsecs_t nanos)
{
    return nanos / 1000000.0;
}

// convert nanoseconds to seconds (floating point)
static inline float ns2sf(nsecs_t nanos)
{
    return nanos / 1000000000.0;
}

// convert bits to kilobits
static float bitsToKilobits(int64_t bits)
{
    return bits / float(1 << 10);
}

WifiDisplayStats::WifiDisplayStats() :
    mNumSeconds(2),
    mVideoEncodeTime("Video Enc Time", DATA_TYPE_TIME),
    mInputFPS("Input FPS", DATA_TYPE_FPS),
    mOutputFPS("Output FPS", DATA_TYPE_FPS),
    mEncodeBits("Enc Bits", DATA_TYPE_BITS),
    mVideoPackTime("Video Pack Time", DATA_TYPE_TIME),
    mAudioInputSampleRate("Audio Input SR", DATA_TYPE_SR),
    mAudioOutputSampleRate("Audio Output SR", DATA_TYPE_SR),
    mFrameNew(0),
    mFrameDuplicate(0),
    mThread(new StatsThread(*this)),
    mStatsEnabled(false),
    mVerboseStatsEnabled(false),
    mLastFrame(NULL),
    mLastInputTime(0),
    mLastOutputTime(0),
    mAudioLastInputTime(0),
    mAudioLastOutputTime(0)
{
}

WifiDisplayStats::~WifiDisplayStats()
{
    mThread = NULL;
}

WifiDisplayStats& WifiDisplayStats::getInstance()
{
    return mInstance;
}

void WifiDisplayStats::start()
{
    // check android properties
    checkStatsEnabled();
    checkVerboseStatsEnabled();

    if (mStatsEnabled) {
        ALOGD("starting...");
        reset();
        mThread->run();
    }
}

void WifiDisplayStats::stop()
{
    if (mStatsEnabled) {
        ALOGD("stopping...");
        {
            Mutex::Autolock _l(mMutex);
            mCondition.signal();
        }
        mThread->requestExitAndWait();

        // reset the flags until next time...
        mStatsEnabled = false;
        mVerboseStatsEnabled = false;

        mVideoEncodeStartTimes.clear();
        mVideoPackStartTimes.clear();
    }
}

void WifiDisplayStats::updateInputFPS(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Input Frame Delta: %.2f millis", ns2msf(time));
        }
        mInputFPS.addSample(time - mLastInputTime);
        mLastInputTime = time;
    }
}

void WifiDisplayStats::updateOutputFPS(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Output Frame Delta: %.2f millis", ns2msf(time));
        }
        mOutputFPS.addSample(time - mLastOutputTime);
        mLastOutputTime = time;
    }
}

void WifiDisplayStats::updateVideoEncodeStartTime(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Video Enc Start Time: %.2f millis", ns2msf(time));
        }

        mVideoEncodeStartTimes.push_back(time);
    }
}

void WifiDisplayStats::updateVideoEncodeEndTime(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Video Enc End Time: %.2f millis", ns2msf(time));
        }

        if(!mVideoEncodeStartTimes.empty())
        {
            List<int64_t>::iterator i = mVideoEncodeStartTimes.begin();
            int64_t starttime = *i;
            mVideoEncodeStartTimes.erase(i);
            mVideoEncodeTime.addSample(time - starttime);
        }
    }
}

void WifiDisplayStats::updateVideoEncodeBits(int32_t bits)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        mEncodeBits.addSample(bits);
    }
}

void WifiDisplayStats::updateFrameType(MediaBuffer * frame)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);

        if(frame == mLastFrame)
            mFrameDuplicate++;
        else
            mFrameNew++;

        mLastFrame = frame;
    }
}

void WifiDisplayStats::updateVideoPackStartTime(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Video Pack Start Time: %.2f millis", ns2msf(time));
        }

        mVideoPackStartTimes.push_back(time);
    }
}

void WifiDisplayStats::updateVideoPackEndTime(nsecs_t time)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Video Pack End Time: %.2f millis", ns2msf(time));
        }

        if(!mVideoPackStartTimes.empty())
        {
            List<int64_t>::iterator i = mVideoPackStartTimes.begin();
            int64_t starttime = *i;
            mVideoPackStartTimes.erase(i);
            mVideoPackTime.addSample(time - starttime);
        }
    }
}

void WifiDisplayStats::updateInputSampleRate(nsecs_t time, int samples)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Audio Input Delta: %.2f millis  %d samples", ns2msf(time), samples);
        }
        mAudioInputSampleRate.addSample(time - mAudioLastInputTime, samples);
        mAudioLastInputTime = time;
    }
}

void WifiDisplayStats::updateOutputSampleRate(nsecs_t time, int samples)
{
    if (mStatsEnabled) {
        Mutex::Autolock _l(mMutex);
        if (mVerboseStatsEnabled) {
            ALOGD("Audio Output Delta: %.2f millis  %d samples", ns2msf(time), samples);
        }
        mAudioOutputSampleRate.addSample(time - mAudioLastOutputTime,samples);
        mAudioLastOutputTime = time;
    }
}

void WifiDisplayStats::reset()
{
    mVideoEncodeTime.reset();
    mInputFPS.reset();
    mOutputFPS.reset();
    mEncodeBits.reset();
    mVideoPackTime.reset();
    mFrameNew = 0;
    mFrameDuplicate = 0;
    mVideoEncodeStartTimes.clear();
    mVideoPackStartTimes.clear();
    mAudioOutputSampleRate.reset();
    mAudioInputSampleRate.reset();
    mLastFrame = NULL;
}

void WifiDisplayStats::checkStatsEnabled()
{
    const char* propName = "media.wfd.stats";
    char propVal[PROPERTY_VALUE_MAX];
    mStatsEnabled = property_get(propName, propVal, "0") &&
        strncmp(propVal, "1", 1) == 0;
}

// this is for the super-chatty messages that print every frame
void WifiDisplayStats::checkVerboseStatsEnabled()
{
    const char* propName = "media.wfd.stats.verbose";
    char propVal[PROPERTY_VALUE_MAX];
    mVerboseStatsEnabled = property_get(propName, propVal, "0") &&
        strncmp(propVal, "1", 1) == 0;
}

void WifiDisplayStats::checkInterval()
{
    const char* propName = "media.wfd.stats.interval";
    char propVal[PROPERTY_VALUE_MAX];
    if(property_get(propName, propVal, NULL) > 0)
    {
        mNumSeconds = strtoul(propVal, NULL, 10);
    }
}

////////////////////////////////////////////////////////////////////////////////
// class: StatsThread
////////////////////////////////////////////////////////////////////////////////

WifiDisplayStats::StatsThread::StatsThread(WifiDisplayStats &stats) :
    Thread(true)
    ,mStats(stats)
{
}

WifiDisplayStats::StatsThread::~StatsThread()
{
    // noop
}

bool WifiDisplayStats::StatsThread::threadLoop()
{
    nsecs_t startTime = systemTime();
    Mutex::Autolock _l(mStats.mMutex);
    while (true) {

        mStats.checkInterval();

        // wait for stats interval
        status_t status = mStats.mCondition.waitRelative(mStats.mMutex,
                s2ns(mStats.mNumSeconds));

        nsecs_t endTime = systemTime();

        // how long have we been sleeping exactly?
        nsecs_t deltaNsec = endTime - startTime;

        // save time for next iteration
        startTime = endTime;

        // see if chatty messages are enabled. we do this
        // every two seconds rather than from frame to
        // frame to conserve system resources
        mStats.checkVerboseStatsEnabled();

        // display statistics
        ALOGD("--- Elapsed Time: %.2f seconds", ns2sf(deltaNsec));
        mStats.mVideoEncodeTime.showStats(deltaNsec);
        mStats.mInputFPS.showStats(deltaNsec);
        mStats.mOutputFPS.showStats(deltaNsec);
        mStats.mEncodeBits.showStats(deltaNsec);
        mStats.mVideoPackTime.showStats(deltaNsec);
        mStats.mAudioInputSampleRate.showStats(deltaNsec);
        mStats.mAudioOutputSampleRate.showStats(deltaNsec);
        ALOGD("Frame Type Count: new=%d, duplicate=%d, (total delivered=%d)",
                mStats.mFrameNew, mStats.mFrameDuplicate,
                mStats.mFrameNew + mStats.mFrameDuplicate);
        ALOGD("---");

        // clear all data
        mStats.reset();

        // someone called stop()
        if (status == 0) {
            ALOGD("Exiting...\n");
            return false;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
// class: Data
////////////////////////////////////////////////////////////////////////////////

WifiDisplayStats::Data::Data(const char *label, DataType dataType)
{
    name = label;
    type = dataType;
    reset();
}

WifiDisplayStats::Data::~Data()
{
}

void WifiDisplayStats::Data::addSample(int64_t sample, int numcount)
{
    // update min/max
    if (count == 0) {
        min = max = sample;
    } else {
        if (sample < min)
            min = sample;
        if (sample > max)
            max = sample;
    }

    // update sum
    sum += sample;
    count += numcount;
}

void WifiDisplayStats::Data::showStats(nsecs_t time)
{
    if (count == 0)
        return;

    float timeSec = ns2sf(time);
    float avg = count > 0 ? sum / (float) count : 0.0; // no div by zero

    switch (type) {
        case DATA_TYPE_TIME:
            ALOGD("%s (millis): samples=%d, min=%.2f, max=%.2f, avg=%.2f, total=%.2f, utilization=%.2f%%",
                    name, count,
                    ns2msf(min), ns2msf(max),
                    ns2msf(avg),
                    ns2msf(sum), (sum / (float) time) * 100.0f);
            break;
        case DATA_TYPE_FPS:
            ALOGD("%s (millis): samples=%d, min=%.2f, max=%.2f, avg=%.2f, framerate=%.2f fps",
                    name, count,
                    ns2msf(min), ns2msf(max),
                    ns2msf(avg), count / ns2sf(sum));
            break;
        case DATA_TYPE_BITS:
            ALOGD("%s (kilobits): samples=%d, min=%.2f, max=%.2f, "
                    "avg=%.2f, bitrate=%.2f kbps",
                    name, count,
                    bitsToKilobits(min),
                    bitsToKilobits(max),
                    bitsToKilobits(int64_t(avg)),
                    bitsToKilobits(sum) / timeSec);
            break;
        case DATA_TYPE_SR:
            ALOGD("%s (millis): samples=%d, min=%.2f, max=%.2f, avg=%.2f, samplerate=%.2f ",
                    name, count,
                    ns2msf(min), ns2msf(max),
                    ns2msf(avg), count / ns2sf(sum));
            break;
        default:
            break;
    }
}

void WifiDisplayStats::Data::reset()
{
    count = 0;
    min = 0;
    max = 0;
    sum = 0;
}

} // namespace android
