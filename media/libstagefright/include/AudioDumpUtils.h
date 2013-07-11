/*
Portions Copyright (c) 2011 Intel Corporation.
*/

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

namespace android {

class AudioDump {

    FILE          *mFilePtr;
    char          *mFileName;
    const char    *mComponentName;
    uint16_t      mComponent;

    char* getAudioDumpFileName();
    void  writeToFile(uint8_t* data, size_t offset, size_t range_length);
public:
    bool           isOffloadTrack;

    enum {
        AUDIO_PARSER = 0,
        AUDIO_DECODER,
        AUDIOFLINGER_PLAYBACK,
        AUDIOFLINGER_RECORD,
    };

    // array of four filedump counts. One for each component.
    static int16_t fileDumpCount[4];

    AudioDump();
    AudioDump(int num);
    virtual ~AudioDump();

    void dumpData(uint8_t* data, size_t offset, size_t range_length);
};

} //namespace android
