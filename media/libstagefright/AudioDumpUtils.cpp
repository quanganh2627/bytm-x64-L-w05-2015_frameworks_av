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

#include <utils/Log.h>
#include <cutils/properties.h>

#include "AudioDumpUtils.h"

#define MAX_NUM_FILES 99

namespace android {

// initializing each component's file dump count to 1
int16_t AudioDump::fileDumpCount[4] = {1,1,1,1};

AudioDump::AudioDump()
    : mFilePtr(NULL),
      mFileName(NULL),
      isOffloadTrack(false) {
    ALOGV(" AudioDump :: Constructor");
}

AudioDump::AudioDump(int num)
     : mFilePtr(NULL),
       mFileName(NULL),
       mComponent(num),
       isOffloadTrack(false) {
    ALOGV(" AudioDump :: Constructor");
    switch (num) {
        case AUDIO_DECODER:
            mComponentName = "dec_out_audio_";
            break;
        case AUDIO_PARSER:
            mComponentName = "parser_out_audio_";
            break;
        case AUDIOFLINGER_PLAYBACK:
            mComponentName = "flinger_pb_out_audio_";
            break;
        case AUDIOFLINGER_RECORD:
            mComponentName = "flinger_rc_in_audio_";
            break;
        default:
            mComponentName = "unknown_out_";
    }
}

AudioDump::~AudioDump() {
    ALOGV(" AudioDump :: Destructor");
    if (mFileName) {
        free(mFileName);
        mFileName = NULL;
    }
    if (mFilePtr) {
        fclose(mFilePtr);
        mFilePtr = NULL;
    }
}

void AudioDump::dumpData(uint8_t *data, size_t offset, size_t range_length) {
    // Taking file dump
    writeToFile(data, offset, range_length);
}

void AudioDump::writeToFile(uint8_t *data, size_t offset, size_t range_length) {

    if (!mFilePtr) {
        if (!mFileName) {
            mFileName = getAudioDumpFileName();
        }
        if (mFileName) {
            mFilePtr = fopen(mFileName, "wb+");
            if (mFilePtr) {
                // take a file dump
                size_t bytesWritten = fwrite(data + offset, 1, range_length, mFilePtr);
                fflush(mFilePtr);
            }
        }
    } else {
        // take a file dump
        size_t bytesWritten = fwrite(data + offset, 1, range_length, mFilePtr);
        fflush(mFilePtr);
    }
}

char* AudioDump::getAudioDumpFileName() {

    char *filename = (char *)calloc(40, sizeof(char));
    if (filename) {
        if (fileDumpCount[mComponent] > MAX_NUM_FILES) {
            ALOGW("Too many file dumps taken, rewriting from first file");
            fileDumpCount[mComponent] = 1;
        }
        strcpy(filename, "/data/");
        strcat(filename, mComponentName);

        char *ptr = filename + strlen(filename);
        // Converting the filecount to char values and catenating to filename
        *ptr++ = fileDumpCount[mComponent]/10 + 48; // int to ascii conversion
        *ptr++ = fileDumpCount[mComponent]%10 + 48; // int to ascii conversion

        if (mComponent == AUDIO_PARSER)
            strcat(filename, ".enc");
        else if ((mComponent == AUDIO_DECODER && !isOffloadTrack) ||
                 (mComponent == AUDIOFLINGER_PLAYBACK && !isOffloadTrack) ||
                 mComponent == AUDIOFLINGER_RECORD)
            strcat(filename, ".pcm");
        else
            strcat(filename, ".dat");

        fileDumpCount[mComponent]++;

        return filename;
    }

    return NULL;
}

} //namespace android
