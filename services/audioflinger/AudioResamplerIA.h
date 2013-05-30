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

#ifndef ANDROID_AUDIO_RESAMPLER_IA_H
#define ANDROID_AUDIO_RESAMPLER_IA_H

#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>

#include "AudioResampler.h"

namespace android {

#define AUDRESAMPLEIA_SAMPLINGRATE_DEFAULT 44100
/* -------------------------------------------------------------------------*/

        class AudioResamplerIA : public AudioResampler {
public:
                AudioResamplerIA(int bitDepth, int inChannelCount,
                                 int32_t sampleRate);
                ~AudioResamplerIA();
                virtual void resample(int32_t *out, size_t outFrameCount,
                                      AudioBufferProvider *provider);
                virtual void setSampleRate(int32_t inSampleRate);
                static int sampleRateSupported(int inputRate, int outputRate);

private:
                void init();
                template < int CHANNELS >
                    void resample(int32_t *out, size_t outFrameCount,
                                  AudioBufferProvider *provider);
                void *mContext;

                static const int16_t mMaxInputBufferSize = 2048;
                float *mFloatInpUnaligned;
                float *mFloatInp;
                float *mFloatOut;

                /* remaining number of output frames that cannot
                 * fit in the current output buffer
                 */
                int mRemainingOutFrames;
                float mOutFrameBuffer[40] __attribute__ ((aligned(16)));
                int32_t mReinitNeeded;/* Flag for reiniting the SRC. */
        };

/* --------------------------------------------------------------------------*/

};/* namespace android */

#endif /*ANDROID_AUDIO_RESAMPLER_IA_H */
