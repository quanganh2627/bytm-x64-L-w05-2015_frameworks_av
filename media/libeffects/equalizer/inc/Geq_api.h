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

#define         MAX_NO_BANDS                5
typedef struct {
    /* N-Band Equaliser parameters */
    int32_t                 gain[MAX_NO_BANDS];    /* Gain for the  bands */
    int32_t                 samplingRate;
    int32_t                 channelCount;
    int32_t                 frameSize;
} Geq_param_t;




int32_t geq_init(void **mod_instance);
int32_t geq_destroy(void **mod_instance);
int32_t geq_setparam(void *mod_instance, Geq_param_t *param);
int32_t geq_process_data(void *mod_instance, short *data, short *result);


