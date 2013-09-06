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

#ifndef UTILS_H_

#define UTILS_H_

#include <media/stagefright/foundation/AString.h>
#include <stdint.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <media/MediaPlayerInterface.h>

namespace android {

#define FOURCC(c1, c2, c3, c4) \
    (c1 << 24 | c2 << 16 | c3 << 8 | c4)

uint16_t U16_AT(const uint8_t *ptr);
uint32_t U32_AT(const uint8_t *ptr);
uint64_t U64_AT(const uint8_t *ptr);

uint16_t U16LE_AT(const uint8_t *ptr);
uint32_t U32LE_AT(const uint8_t *ptr);
uint64_t U64LE_AT(const uint8_t *ptr);

uint64_t ntoh64(uint64_t x);
uint64_t hton64(uint64_t x);

struct MetaData;
struct AMessage;
status_t convertMetaDataToMessage(
        const sp<MetaData> &meta, sp<AMessage> *format);
void convertMessageToMetaData(
        const sp<AMessage> &format, sp<MetaData> &meta);

AString MakeUserAgent();
// Send information from MetaData to the HAL via AudioSink
status_t sendMetaDataToHal(sp<MediaPlayerBase::AudioSink>& sink,
                            const sp<MetaData>& meta);
bool isInCall();

// Check whether the stream defined by meta can be offloaded to hardware
bool canOffloadStream( const sp<MetaData>& meta, bool hasVideo, bool isStreaming , uint32_t sessionId);

// used to set the AAC parameters for offloaded AAC files
status_t setAACParameters(sp<MetaData> meta, audio_format_t *aFormat, uint32_t *avgBitRate);
status_t mapMimeToAudioFormat(audio_format_t *audioFormat, const char *mime);
}  // namespace android

#endif  // UTILS_H_
