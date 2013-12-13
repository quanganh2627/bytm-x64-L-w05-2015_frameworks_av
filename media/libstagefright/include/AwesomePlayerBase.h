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

#ifndef AWESOME_PLAYER_BASE_H_
#define AWESOME_PLAYER_BASE_H_

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaExtractor.h>

namespace android {

struct AwesomePlayerBase {
public:
    AwesomePlayerBase() {}
    virtual ~AwesomePlayerBase() {}

protected:
    virtual status_t setDataSource_l(const char *uri, const KeyedVector<String8, String8> *headers = NULL) = 0;
    virtual status_t setDataSource_l(const sp<DataSource> &dataSource) = 0;
    virtual status_t setDataSource_l(const sp<MediaExtractor> &extractor) = 0;
    virtual void addTextSource_l(size_t trackIndex, const sp<MediaSource>& source) = 0;

    virtual status_t initVideoDecoder(uint32_t flags = 0) = 0;
    virtual void initRenderer_l() = 0;

    virtual status_t play_l() = 0;
    virtual status_t seekTo_l(int64_t timeUs) = 0;
    virtual status_t pause_l(bool at_eos = false) = 0;
    virtual void reset_l() = 0;

    virtual void shutdownVideoDecoder_l() = 0;
    virtual void onVideoEvent() = 0;
    virtual void onStreamDone() = 0;

private:
    AwesomePlayerBase(const AwesomePlayerBase &);
    AwesomePlayerBase &operator=(const AwesomePlayerBase &);
};

}  // namespace android

#endif  // AWESOME_PLAYER_BASE_H_
