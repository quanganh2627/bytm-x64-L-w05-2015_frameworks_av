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

#define LOG_TAG "Core_FX"
//#define LOG_NDEBUG 0
#include "EffectEQ.h"
#define CHECK_ARG(cond) {                     \
    if (!(cond)) {                            \
        ALOGV("\tERROR : Invalid argument: "#cond);      \
        return -EINVAL;                       \
    }                                         \
}
extern "C" const struct effect_interface_s gCoreEffectInterface;
namespace android {
const effect_descriptor_t gEqualizerDescriptor = {
        {0x0bed4300, 0xddd6, 0x11db, 0x8f34, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // type
        {0xd7a247c1, 0x1a7b, 0x11d0, 0xbc0d, {0x2a, 0x30, 0xdf, 0xd7, 0x20, 0x45}}, // uuid
        EFFECT_CONTROL_API_VERSION,
        (EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_LAST),
        0,
        1,
        "Equalizer",
        "Intel Corporation",
};

int CoreEQSetConfig(CoreEffect *pContext, effect_config_t *pConfig);
void EffectGetConfig(CoreEffect *pContext, effect_config_t *pConfig);
int Effect_setState(CoreEffect *pContext, bool state);
int Equalizer_getParameter(CoreEffect     *pContext,
                           void              *pParam,
                           size_t            *pValueSize,
                           void              *pValue);
int32_t GetNumPresets();
int32_t EqualizerGetBandFreqRange(CoreEffect *pContext, int32_t band, uint32_t *pmin,
                                  uint32_t *pmax);
int32_t EqualizerGetPreset(CoreEffect *pContext);
const char * EqualizerGetPresetName(int32_t preset);
int32_t EqualizerGetBand(CoreEffect *pContext, uint32_t targetFreq);
int Equalizer_setParameter (CoreEffect *pContext, void *pParam, void *pValue);
int EqualizerSetPreset (CoreEffect *pContext , int preset);
int EqualizerSetBandLevel(CoreEffect  *pContext, int band, short Gain);

extern "C" int CoreEQCreate(const effect_uuid_t *uuid, int32_t sessionId,
                            int32_t ioId, effect_handle_t *pInterface) {
    int ret;
    size_t i, j;
    if ((pInterface == NULL) || (uuid == NULL)) return NULL;
    CoreEffect* pContext = new CoreEffect;
    if (pContext == NULL) {
        ALOGW("CoreEQCreate() failed");
        return -EINVAL;
    }
    pContext->state = FALSE;
    pContext->pLib = NULL;
    pContext->pParams = new(Geq_param_t);
    for (i = 0; i < NUM_BANDS; i++) {
        pContext->bandFreq[i] = bandCFrequencies[i];
        pContext->bandGain[i] = pContext->pParams->gain[i] = 0;
    }
    pContext->preset = 3;
    pContext->pItf = &gCoreEffectInterface;
    *pInterface = (effect_handle_t)pContext;
    return 0;
}


extern "C" int CoreEQRelease(effect_handle_t interface) {

    CoreEffect* pContext = (CoreEffect*)interface;
    if (pContext == NULL) {
        ALOGV("CoreEQRelease called with NULL pointer");
        return -EINVAL;
    }
    pContext->state = FALSE;
    if (geq_destroy(&pContext->pLib)) {
        ALOGV("CoreEQRelease lib destroy failed");
        return -EINVAL;
    }
    delete pContext;
    pContext = NULL;
    return 0;
}

extern "C" int CoreEQGetDescriptor(const effect_uuid_t *uuid,
                                   effect_descriptor_t *pDescriptor) {
    const effect_descriptor_t *desc = NULL;

    ALOGV("CoreEQGetDescriptor() start");
    if (pDescriptor == NULL || uuid == NULL) {
        ALOGV("CoreEQGetDescriptor() called with NULL pointer");
        return -EINVAL;
    }

    if (memcmp(uuid, &gEqualizerDescriptor.uuid, sizeof(effect_uuid_t)) == 0) {
        desc = &gEqualizerDescriptor;
    }

    if (desc == NULL) {
        return  -EINVAL;
    }

    *pDescriptor = *desc;

    ALOGV("CoreEQGetDescriptor() end");
    return 0;
}
// Local functions
int CoreEQSetConfig(CoreEffect *pContext, effect_config_t *pConfig) {

    CHECK_ARG(pContext != NULL);
    CHECK_ARG(pConfig != NULL);

    CHECK_ARG(pConfig->inputCfg.samplingRate == pConfig->outputCfg.samplingRate);
    CHECK_ARG(pConfig->inputCfg.channels == pConfig->outputCfg.channels);
    CHECK_ARG(pConfig->inputCfg.format == pConfig->outputCfg.format);
    CHECK_ARG(pConfig->inputCfg.channels == AUDIO_CHANNEL_OUT_STEREO);
    CHECK_ARG(pConfig->outputCfg.accessMode == EFFECT_BUFFER_ACCESS_WRITE
              || pConfig->outputCfg.accessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE);
    CHECK_ARG(pConfig->inputCfg.format == AUDIO_FORMAT_PCM_16_BIT);

    memcpy(&pContext->config, pConfig, sizeof(effect_config_t));
    pContext->pParams->samplingRate =  pConfig->outputCfg.samplingRate;
    pContext->pParams->channelCount = 2;
    pContext->pParams->frameSize = pConfig->inputCfg.buffer.frameCount;
    if (geq_setparam(pContext->pLib, pContext->pParams)) {
        ALOGV("CoreEQSetConfig set control parameters to lib failed");
        return -EINVAL;
    }
    return 0;
}

void EffectGetConfig(CoreEffect *pContext, effect_config_t *pConfig) {
    memcpy(pConfig, &pContext->config, sizeof(effect_config_t));
}

int Equalizer_getParameter(CoreEffect     *pContext,
                           void              *pParam,
                           size_t            *pValueSize,
                           void              *pValue){
    int status = 0;
    int bMute = 0;
    int32_t *pParamTemp = (int32_t *)pParam;
    int32_t param = *pParamTemp++;
    int32_t param2;
    char *name;

    ALOGV("Equalizer_getParameter start: param: %d", param);

    switch (param) {
    case EQ_PARAM_NUM_BANDS:
    case EQ_PARAM_CUR_PRESET:
    case EQ_PARAM_GET_NUM_OF_PRESETS:
    case EQ_PARAM_BAND_LEVEL:
    case EQ_PARAM_GET_BAND:
        if (*pValueSize < sizeof(int16_t)) {
            ALOGV("Equalizer_getParameter() invalid pValueSize %d", *pValueSize);
            return -EINVAL;
        }
        *pValueSize = sizeof(int16_t);
        break;

    case EQ_PARAM_LEVEL_RANGE:
        if (*pValueSize < 2 * sizeof(int16_t)) {
            ALOGV("Equalizer_getParameter() invalid pValueSize %d", *pValueSize);
            return -EINVAL;
        }
        *pValueSize = 2 * sizeof(int16_t);
        break;
    case EQ_PARAM_BAND_FREQ_RANGE:
        if (*pValueSize < 2 * sizeof(int32_t)) {
            ALOGV("Equalizer_getParameter() invalid pValueSize %d", *pValueSize);
            return -EINVAL;
        }
        *pValueSize = 2 * sizeof(int32_t);
        break;

    case EQ_PARAM_CENTER_FREQ:
        if (*pValueSize < sizeof(int32_t)) {
            ALOGV("Equalizer_getParameter() invalid pValueSize %d", *pValueSize);
            return -EINVAL;
        }
        *pValueSize = sizeof(int32_t);
        break;

    case EQ_PARAM_GET_PRESET_NAME:
        break;

    case EQ_PARAM_PROPERTIES:
        if (*pValueSize < (2 + NUM_BANDS) * sizeof(uint16_t)) {
            ALOGV("Equalizer_getParameter() invalid pValueSize %d", *pValueSize);
            return -EINVAL;
        }
        *pValueSize = (2 + NUM_BANDS) * sizeof(uint16_t);
        break;

    default:
        ALOGV("Equalizer_getParameter unknown param %d", param);
        return -EINVAL;
    }

    switch (param) {
    case EQ_PARAM_NUM_BANDS:
        *(uint16_t *)pValue = (uint16_t)NUM_BANDS;
        break;

    case EQ_PARAM_LEVEL_RANGE:
        *(int16_t *)pValue = -1500;
        *((int16_t *)pValue + 1) = 1500;
        break;

    case EQ_PARAM_BAND_LEVEL:
        param2 = *pParamTemp;
        if (param2 >= NUM_BANDS) {
            status = -EINVAL;
            break;
        }
        *(int16_t *)pValue = (int16_t)(pContext->pParams->gain[param2] * 100);
        break;

    case EQ_PARAM_CENTER_FREQ:
        param2 = *pParamTemp;
        if (param2 >= NUM_BANDS) {
            status = -EINVAL;
            break;
        }
        *(int32_t *)pValue = pContext->bandFreq[param2];
        break;

    case EQ_PARAM_BAND_FREQ_RANGE:
        param2 = *pParamTemp;
        if (param2 >= NUM_BANDS) {
            status = -EINVAL;
            break;
        }
        EqualizerGetBandFreqRange(pContext, param2, (uint32_t *)pValue, ((uint32_t *)pValue + 1));
        break;

    case EQ_PARAM_GET_BAND:
        param2 = *pParamTemp;
        *(uint16_t *)pValue = (uint16_t)EqualizerGetBand(pContext, param2);
        //ALOGV("\tEqualizer_getParameter() EQ_PARAM_GET_BAND frequency %d, band %d",
        //      param2, *(uint16_t *)pValue);
        break;

    case EQ_PARAM_CUR_PRESET:
        *(uint16_t *)pValue = (uint16_t)pContext->preset;
        //ALOGV("\tEqualizer_getParameter() EQ_PARAM_CUR_PRESET %d", *(int32_t *)pValue);
        break;

    case EQ_PARAM_GET_NUM_OF_PRESETS:
        *(uint16_t *)pValue = (uint16_t)GetNumPresets();
        //ALOGV("\tEqualizer_getParameter() EQ_PARAM_GET_NUM_OF_PRESETS %d", *(int16_t *)pValue);
        break;

    case EQ_PARAM_GET_PRESET_NAME:
        param2 = *pParamTemp;
        if (param2 >= GetNumPresets()) {
            status = -EINVAL;
            break;
        }
        name = (char *)pValue;
        strncpy(name, EqualizerGetPresetName(param2), *pValueSize - 1);
        name[*pValueSize - 1] = 0;
        *pValueSize = strlen(name) + 1;
        break;

    case EQ_PARAM_PROPERTIES: {
        int16_t *p = (int16_t *)pValue;
        ALOGV("Equalizer_getParameter() EQ_PARAM_PROPERTIES");
        p[0] = (int16_t)pContext->preset;
        p[1] = (int16_t)NUM_BANDS;
        for (int i = 0; i < NUM_BANDS; i++) {
            p[2 + i] = (int16_t)(pContext->pParams->gain[i] * 100);
        }
    } break;

    default:
        ALOGV("ERROR : Equalizer_getParameter() invalid param %d", param);
        status = -EINVAL;
        break;
    }

    ALOGV("Equalizer_getParameter end");
    return status;
} /* end Equalizer_getParameter */

int GetNumPresets() {
    return sizeof(gPresets) / sizeof(char*);
}

int32_t EqualizerGetBandFreqRange(CoreEffect *pContext, int32_t band, uint32_t *pmin,
                                  uint32_t *pmax){
    *pmin = bandFreqRange[band][0];
    *pmax  = bandFreqRange[band][1];
    return 0;
}

const char * EqualizerGetPresetName(int32_t preset){
    ALOGV("EqualizerGetPresetName %d, %s", preset, gPresets[preset]);
    return gPresets[preset];
}

int32_t EqualizerGetBand(CoreEffect *pContext, uint32_t targetFreq){
    int band = 0;

    if(targetFreq < bandFreqRange[0][0]){
        return -EINVAL;
    }else if(targetFreq == bandFreqRange[0][0]){
        return 0;
    }
    for(int i=0; i<NUM_BANDS;i++){
        if((targetFreq > bandFreqRange[i][0])&&(targetFreq <= bandFreqRange[i][1])){
            band = i;
        }
    }
    return band;
}

int Equalizer_setParameter (CoreEffect *pContext, void *pParam, void *pValue){
    int status = 0;
    int32_t preset;
    int32_t band;
    int32_t level;
    int32_t *pParamTemp = (int32_t *)pParam;
    int32_t param = *pParamTemp++;

    ALOGV("Equalizer_setParameter start: param: %d",param);
    switch (param) {
    case EQ_PARAM_CUR_PRESET:
        preset = (int32_t)(*(uint16_t *)pValue);
        if ((preset >= GetNumPresets()) || (preset < 0)) {
            status = -EINVAL;
            break;
        }
        EqualizerSetPreset(pContext, preset);
        break;
    case EQ_PARAM_BAND_LEVEL:
        band =  *pParamTemp;
        level = (int32_t)(*(int16_t *)pValue);
        if (band >= NUM_BANDS) {
            status = -EINVAL;
            break;
        }
        EqualizerSetBandLevel(pContext, band, level);
        break;
    case EQ_PARAM_PROPERTIES: {
        int16_t *p = (int16_t *)pValue;
        if ((int)p[0] >= GetNumPresets()) {
            status = -EINVAL;
            break;
        }
        if (p[0] >= 0) {
            EqualizerSetPreset(pContext, (int)p[0]);
        } else {
            if ((int)p[1] != NUM_BANDS) {
                status = -EINVAL;
                break;
            }
            for (int i = 0; i < NUM_BANDS; i++) {
                EqualizerSetBandLevel(pContext, i, (int)p[2 + i]);
            }
        }
    } break;
    default:
        ALOGV("Equalizer_setParameter() invalid param %d", param);
        status = -EINVAL;
        break;
    }
    return status;
}

int EqualizerSetPreset(CoreEffect* pContext, int preset) {

    ALOGV("EqualizerSetPreset: preset: %d", preset);
    if (pContext == NULL)
        return -EINVAL;
    int gain, gainRounded;
    for (int i=0; i < NUM_BANDS; i++) {
        pContext->bandGain[i] = bandGains[preset * NUM_BANDS + i];
        pContext->pParams->gain[i] = pContext->bandGain[i];
         if (geq_setparam(pContext->pLib, pContext->pParams)) {
             ALOGV("EqualizerSetPreset set to lib failed");
             return -EINVAL;
         }
    }
    pContext->preset = preset;
    return 0;
}
int EqualizerSetBandLevel(CoreEffect* pContext, int band, short gain) {

    ALOGV("EqualizerSetBandLevel: band: %d, gain: %d", band, gain);
    if (pContext == NULL)
        return -EINVAL;
    int gainRounded;
    if (gain > 0)
       gainRounded = (gain + 50) / 100;
    else
       gainRounded = (gain - 50) / 100;
    pContext->bandGain[band] = gainRounded;
    pContext->pParams->gain[band] = gainRounded;
    pContext->preset = PRESET_CUSTOM;
    if (geq_setparam(pContext->pLib, pContext->pParams)) {
        ALOGV("EqualizerSetBandLevel set in lib failed");
        return -EINVAL;
    }
    return 0;
}

int Effect_setState(CoreEffect *pContext, bool state) {

     if (pContext == NULL) {
         return -EINVAL;
     }
     pContext->state = state;
     return 0;
}
// End Local functions
} //namespace android

extern "C" {
int CoreEQ_command(effect_handle_t  self,
                              uint32_t            cmdCode,
                              uint32_t            cmdSize,
                              void                *pCmdData,
                              uint32_t            *replySize,
                              void                *pReplyData) {
    CoreEffect * pContext = (CoreEffect *) self;
    ALOGV("CoreEQ_command: cmdCode: %d", cmdCode);
    switch (cmdCode) {
        case EFFECT_CMD_INIT:
            if (pReplyData == NULL || *replySize != sizeof(int)) {
                ALOGV("EFFECT_CMD_INIT: ERROR");
                return -EINVAL;
            }
            *(int *) pReplyData = 0;
            if (geq_init(&pContext->pLib)) {
                ALOGV("CoreEQ_command : cmdCase INIT, Lib init failed");
                return -EINVAL;
            }
            break;
        case EFFECT_CMD_SET_CONFIG:
            if (pCmdData    == NULL||
                cmdSize     != sizeof(effect_config_t)||
                pReplyData  == NULL||
                *replySize  != sizeof(int)) {
                ALOGV("CoreEQ_command cmdCode Case: "
                        "EFFECT_CMD_SET_CONFIG: ERROR");
                return -EINVAL;
            }
            *(int *) pReplyData = android::CoreEQSetConfig(pContext,
                                                   (effect_config_t *)pCmdData);
            break;
        case EFFECT_CMD_GET_CONFIG:
            if (pReplyData == NULL ||
                *replySize != sizeof(effect_config_t)) {
                ALOGV("CoreEQ_command cmdCode Case: "
                        "EFFECT_CMD_GET_CONFIG: ERROR");
                return -EINVAL;
            }
            android::EffectGetConfig(pContext, (effect_config_t *)pReplyData);
            break;

        case EFFECT_CMD_RESET:
            android::CoreEQSetConfig(pContext, &pContext->config);
            break;

        case EFFECT_CMD_GET_PARAM: {
            ALOGV("EFFECT_CMD_GET_PARAM");
            if (pCmdData == NULL ||
                    cmdSize < (int)(sizeof(effect_param_t) + sizeof(int32_t)) ||
                    pReplyData == NULL ||
                    *replySize < (int) (sizeof(effect_param_t) + sizeof(int32_t))) {
                    ALOGV("CoreEQ_command cmdCode Case: "
                            "EFFECT_CMD_GET_PARAM");
                    return -EINVAL;
                }
                effect_param_t *p = (effect_param_t *)pCmdData;

                memcpy(pReplyData, pCmdData, sizeof(effect_param_t) + p->psize);

                p = (effect_param_t *)pReplyData;

                int voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

                p->status = android::Equalizer_getParameter(pContext,
                                                            p->data,
                                                            &p->vsize,
                                                            p->data + voffset);

                *replySize = sizeof(effect_param_t) + voffset + p->vsize;


        } break;
        case EFFECT_CMD_SET_PARAM:{
                int i;
                ALOGV("EFFECT_CMD_SET_PARAM");
                if (pCmdData == NULL || cmdSize < (int)(sizeof(effect_param_t) + sizeof(int32_t)) ||
                    pReplyData == NULL || *replySize != sizeof(int32_t)) {
                    ALOGV("CoreEQ_command cmdCode Case: "
                            "EFFECT_CMD_SET_PARAM: ERROR");
                    return -EINVAL;
                }
                effect_param_t *p = (effect_param_t *) pCmdData;
                *(int *)pReplyData = android::Equalizer_setParameter(pContext,
                                                                    (void *)p->data,
                                                                     p->data + p->psize);
        } break;

        case EFFECT_CMD_ENABLE:
            ALOGV("CoreEQ_command cmdCode Case: EFFECT_CMD_ENABLE start");
            if (pReplyData == NULL || *replySize != sizeof(int)){
                ALOGV("CoreEQ_command cmdCode Case: EFFECT_CMD_ENABLE: ERROR");
                return -EINVAL;
            }

            *(int *)pReplyData = android::Effect_setState(pContext, TRUE);
            break;

        case EFFECT_CMD_DISABLE:
            ALOGV("CoreEQ_command cmdCode Case: EFFECT_CMD_DISABLE start");
            if (pReplyData == NULL || *replySize != sizeof(int)){
                ALOGV("CoreEQ_command cmdCode Case: EFFECT_CMD_DISABLE: ERROR");
                return -EINVAL;
            }
            *(int *)pReplyData = android::Effect_setState(pContext, FALSE);
            break;

        case EFFECT_CMD_SET_DEVICE:
        case EFFECT_CMD_SET_VOLUME:
        case EFFECT_CMD_SET_AUDIO_MODE:
            break;
        default:
            ALOGV("CoreEQ_command command %d invalid", cmdCode);
            return -EINVAL;
    }
    return 0;
}
int CoreEQ_process(effect_handle_t     self,
                              audio_buffer_t         *inBuffer,
                              audio_buffer_t         *outBuffer) {

    CoreEffect * pContext = (CoreEffect *) self;
    int i;
    if (pContext == NULL) {
        ALOGV("CoreEQ_process() pContext == NULL");
        return -EINVAL;
    }
    if (inBuffer == NULL  || inBuffer->raw == NULL  ||
            outBuffer == NULL || outBuffer->raw == NULL ||
            inBuffer->frameCount != outBuffer->frameCount){
        ALOGV("CoreEQ_process() invalid param");
        return -EINVAL;
    }
    if (pContext->pLib == NULL) {
        if (geq_init(&pContext->pLib)) {
            ALOGV("CoreEQ_process lib init failed");
            return -EINVAL;
        }
    }
    geq_process_data(pContext->pLib,inBuffer->s16,outBuffer->s16);
    return 0;
}

int CoreEQ_getDescriptor(effect_handle_t   self,
                                    effect_descriptor_t *pDescriptor) {
    CoreEffect * pContext = (CoreEffect *) self;
    const effect_descriptor_t *desc;

    if (pContext == NULL || pDescriptor == NULL) {
        ALOGV("CoreEQ_getDescriptor() NULL pointer");
        return -EINVAL;
    }
    desc = &android::gEqualizerDescriptor;
    *pDescriptor = *desc;

    return 0;
}

const struct effect_interface_s gCoreEffectInterface = {
    CoreEQ_process,
    CoreEQ_command,
    CoreEQ_getDescriptor,
    NULL
};
__attribute__ ((visibility ("default")))
audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    tag : AUDIO_EFFECT_LIBRARY_TAG,
    version : EFFECT_LIBRARY_API_VERSION,
    name : "Intel EQ Library",
    implementor : "Intel",
    create_effect : android::CoreEQCreate,
    release_effect : android::CoreEQRelease,
    get_descriptor : android::CoreEQGetDescriptor,
};
} //extern C

