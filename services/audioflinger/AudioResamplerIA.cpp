/*
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_TAG "AudioResamplerIA"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include "cutils/log.h"
#include "AudioResamplerIA.h"
#include "iasrc_resampler.h"

namespace android {
// ----------------------------------------------------------------------------


AudioResamplerIA::AudioResamplerIA(int bitDepth, int inChannelCount, int32_t sampleRate)
    : AudioResampler(bitDepth, inChannelCount, sampleRate, DEFAULT_QUALITY)
{
    /* If samplerate combination is not supported.. behave like passthough */
    if (!sampleRateSupported(mInSampleRate, mSampleRate)) {
        mInSampleRate = AUDRESAMPLEIA_SAMPLINGRATE_DEFAULT;
        mSampleRate = AUDRESAMPLEIA_SAMPLINGRATE_DEFAULT;
    }
    iaresamplib_new(&mContext, mChannelCount, mInSampleRate, mSampleRate);

    mFloatInpUnaligned = new float[(mMaxInputBufferSize+64+16)];
    mFloatInp = (float*) ((int) ((unsigned char*) (mFloatInpUnaligned)+16) & (~0xF));
    mFloatOut = new float[(mMaxInputBufferSize + 64)*mSampleRate/mInSampleRate + 1];
    mRemainingOutFrames = 0;
    memset(&mOutFrameBuffer, 0, sizeof(mOutFrameBuffer));
    mReinitNeeded = 1;
}

AudioResamplerIA::~AudioResamplerIA()
{
    iaresamplib_delete(&mContext);
    delete [] mFloatInpUnaligned;
    delete [] mFloatOut;
}

void AudioResamplerIA::setSampleRate(int32_t inSampleRate)
{
    if (mReinitNeeded == 1 || inSampleRate != mInSampleRate) {
        iaresamplib_delete(&mContext);
        /* If samplerate combination is not supported.. behave like passthough */
        if (sampleRateSupported(inSampleRate, mSampleRate)) {
            mInSampleRate = inSampleRate;
        }
        else {
            mInSampleRate = AUDRESAMPLEIA_SAMPLINGRATE_DEFAULT;
            mSampleRate = AUDRESAMPLEIA_SAMPLINGRATE_DEFAULT;
        }

        iaresamplib_new(&mContext, mChannelCount, mInSampleRate, mSampleRate);

        delete [] mFloatOut;
        mFloatOut = new float[(mMaxInputBufferSize+64)*mSampleRate/mInSampleRate];
        mReinitNeeded = 0;
        LOGD("Create AudioResamplerIA Resampler: Input rate %i, output rate %i",
             mInSampleRate, mSampleRate);
    }
}

int AudioResamplerIA::sampleRateSupported(int inputRate, int outputRate)
{
    return iaresamplib_supported_conversion(inputRate, outputRate);
}

void AudioResamplerIA::init()
{
}

void AudioResamplerIA::resample(int32_t* out, size_t outFrameCount,
                                AudioBufferProvider* provider)
{
    // select the appropriate resampler
    switch (mChannelCount) {
    case 1:
        resample<1>(out, outFrameCount, provider);
        break;
    case 2:
        resample<2>(out, outFrameCount, provider);
        break;
    }
}

template<int CHANNELS>
void AudioResamplerIA::resample(int32_t* out, size_t outFrameCount,
                                AudioBufferProvider* provider)
{
    size_t lcou;
    size_t inputIndex = mInputIndex;
    size_t outputIndex = 0;
    size_t outputSampleCount = outFrameCount * 2;
    size_t inFrameCount;
    int16_t *pInp;
    int32_t *pOut = out;
    int32_t *pStOut;
    size_t framesInBuffer;
    size_t framesToProcess;
    size_t framesProcessed = 0;
    size_t framesLeft;
    size_t curFrameSize;
    size_t outFrameSize;
    size_t frameCount;
    size_t outFramesLeft = outFrameCount;
    size_t outputChannels = 2;

    inFrameCount = (((outFrameCount-mRemainingOutFrames)*mInSampleRate)/mSampleRate) + 1;
    // Get input buffer. Could already be partrially used before.
    AudioBufferProvider::Buffer& buffer(mBuffer);

    size_t framesToCpy = mRemainingOutFrames < outFrameCount ?
                         mRemainingOutFrames : outFrameCount;
    if (framesToCpy > 0) {
        iaresamplib_convert_2_output_format(&mOutFrameBuffer[0], pOut, framesToCpy, CHANNELS, mVolume);
        pOut += framesToCpy*outputChannels;
    }

    if (mRemainingOutFrames>framesToCpy) {
        memcpy((void*) &mOutFrameBuffer[0], (void*) &mOutFrameBuffer[framesToCpy*outputChannels],
               (mRemainingOutFrames-framesToCpy)*outputChannels*sizeof(float));
    }
    mRemainingOutFrames -= framesToCpy;
    outputIndex += framesToCpy;

    while (outputIndex < outFrameCount) {
        // buffer is empty, fetch a new one
        while (buffer.frameCount == 0) {
            buffer.frameCount = inFrameCount;
            provider->getNextBuffer(&buffer);
            if (buffer.raw == NULL) {
                goto resample_exit;
            }
        }

        frameCount = buffer.frameCount;

        // get to the point where we need to start processing.
        // ASSUME that input is of int16_t
        // mInputIndex points to the place where we want to read next.
        pInp = buffer.i16 + inputIndex * CHANNELS;

        framesInBuffer = frameCount - inputIndex;

        // The number of samples that can be processed in the current buffer.
        framesToProcess = (inFrameCount-framesProcessed) > framesInBuffer ?
                          framesInBuffer : (inFrameCount-framesProcessed);
        framesLeft = framesToProcess;

        while (framesLeft > 0) {
            // What can be processed in the current loop based on input buffer size.
            curFrameSize = (mMaxInputBufferSize/CHANNELS) < framesLeft ?
                           mMaxInputBufferSize/CHANNELS : framesLeft;

            // Convert the int16 to float
            iaresamplib_convert_short_2_float(pInp, mFloatInp, curFrameSize*CHANNELS);
            // resample
            iaresamplib_process_float(mContext, mFloatInp, curFrameSize, mFloatOut, &outFrameSize);
            outputIndex += outFrameSize;

            // convert back to int32. Also converts mono to stereo and applies volume
            // ASSUMPTION : Volume is in 4.12 format.
            if (outputIndex>outFrameCount) {
                iaresamplib_convert_2_output_format(mFloatOut, pOut, outFrameSize-(outputIndex-outFrameCount),
                                                    CHANNELS, mVolume);
                mRemainingOutFrames = outputIndex - outFrameCount;
	        memcpy(&mOutFrameBuffer[0], mFloatOut+(outFrameSize-mRemainingOutFrames)*CHANNELS,
                       mRemainingOutFrames*CHANNELS*sizeof(float));
            }
            else {
                iaresamplib_convert_2_output_format(mFloatOut, pOut, outFrameSize, CHANNELS, mVolume);
            }

            // iaresamplib_convert_float_2_short_stereo(mFloatOut, pout, out_n_frames*CHANNELS, CHANNELS, );
            framesLeft -= curFrameSize;
            pOut += outFrameSize*outputChannels; // Assuming that output is always stereo.
            pInp += curFrameSize*CHANNELS;
        }
        inputIndex += framesToProcess;
        framesProcessed += framesToProcess;

        // if done with buffer, save samples
        if (inputIndex >= frameCount) {
            inputIndex -= frameCount;
            provider->releaseBuffer(&buffer);
        }
    }

resample_exit:
    mInputIndex = inputIndex;
}


// ----------------------------------------------------------------------------
}; // namespace android

