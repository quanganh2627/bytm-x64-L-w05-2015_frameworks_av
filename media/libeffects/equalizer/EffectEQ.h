/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <cutils/log.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <utils/String8.h>
#include <media/AudioSystem.h>
#include <audio_effects/effect_equalizer.h>
#include <hardware/audio_effect.h>
#include <hardware/audio.h>
#include <eq.h>
#if __cplusplus
extern "C" {
#endif
#include "Geq_api.h"

#define TRUE 1
#define FALSE 0

struct CoreEffect {
    const struct effect_interface_s*    pItf; //Holds the itfe of the effect
    effect_config_t         config;  //Config of the buffer
    bool                    state;   // effect enabled state
    int32_t                 preset;  // Current preset
    int16_t                 bandGain[NUM_BANDS]; // band gains
    uint16_t                bandFreq[NUM_BANDS]; // centre freq of the bands
    Geq_param_t*            pParams; //Pointer to the library param structure
    void*                   pLib; // Effect lib handle
};
#if __cplusplus
} // extern C
#endif
