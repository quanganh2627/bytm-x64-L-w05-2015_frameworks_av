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

#define LOG_TAG "FileSource"
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/FileSource.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_READ_TIME_US 100000 // Arbitrary - to be able to detect long stalls of filesource

namespace android {

FileSourceBuffer::FileSourceBuffer() {
    for (int i = 0; i < FILE_SOURCE_META_BUFFER_NUMBER; i++) {
        mMetaBuffer[i].valid = 0;
        mMetaBuffer[i].count = 0;
        mMetaBuffer[i].len = 0;
    }
    mNextBuffer = 0;
}

ssize_t FileSourceBuffer::readFromBuffer(off64_t offset, void *data, size_t size, int file) {
    off64_t local_offset = offset & FILE_SOURCE_META_BUFFER_DATA_LOCAL_OFFSET;
    Mutex::Autolock autoLock(mLock);
    if (local_offset + size < FILE_SOURCE_META_BUFFER_DATA_SIZE) {
        off64_t block_offset = offset & FILE_SOURCE_META_BUFFER_DATA_OFFSET;
        for (int i = 0; i < FILE_SOURCE_META_BUFFER_NUMBER; i++) {
            if (mMetaBuffer[i].valid) {
                if (mMetaBuffer[i].count < FILE_SOURCE_META_BUFFER_LRU_COUNT_LIMIT) {
                    mMetaBuffer[i].count ++;
                }
            }
        }
        for (int i = 0; i < FILE_SOURCE_META_BUFFER_NUMBER; i++) {
            if ((block_offset == mMetaBuffer[i].offset) && mMetaBuffer[i].valid) {
                if (mMetaBuffer[i].len < FILE_SOURCE_META_BUFFER_DATA_SIZE) {
                    if (local_offset >= mMetaBuffer[i].len) {
                        return 0;
                    }
                    if ((local_offset + size) > mMetaBuffer[i].len) {
                        size = mMetaBuffer[i].len - local_offset;
                    }
                }
                memcpy(data, mMetaBuffer[i].data + local_offset, size);
                mMetaBuffer[i].count = 0;
                return size;
            }
        }

        off64_t err = lseek64(file, block_offset, SEEK_SET);

        if (err == -1) {
            LOGE("seek to %ld failed", block_offset);
            return UNKNOWN_ERROR;
        }

        // mNextBuffer calculation
        // 1) select the buffer with valid = 0;
        // 2) LRU calculation: select the buffer with the max count
        mNextBuffer = -1;
        for (int i = 0; i < FILE_SOURCE_META_BUFFER_NUMBER; i++) {
            if (mMetaBuffer[i].valid == 0) {
                mNextBuffer = i;
                break;
            }
        }
        if (mNextBuffer == -1) {
            int max_count = -1;
            for (int i = 0; i < FILE_SOURCE_META_BUFFER_NUMBER; i++) {
                if (mMetaBuffer[i].count > max_count) {
                    max_count = mMetaBuffer[i].count;
                    mNextBuffer = i;
                }
            }
        }
        if (mNextBuffer == -1) {
            return UNKNOWN_ERROR;
        }
        mMetaBuffer[mNextBuffer].valid = 0;
        size_t len = ::read(file, mMetaBuffer[mNextBuffer].data, FILE_SOURCE_META_BUFFER_DATA_SIZE);
        if (len <= 0) {
            return UNKNOWN_ERROR;
        }

        mMetaBuffer[mNextBuffer].len = len;
        mMetaBuffer[mNextBuffer].offset = block_offset;
        mMetaBuffer[mNextBuffer].valid = 1;
        mMetaBuffer[mNextBuffer].count = 0;
        if (mMetaBuffer[mNextBuffer].len < FILE_SOURCE_META_BUFFER_DATA_SIZE) {
            if (local_offset >= mMetaBuffer[mNextBuffer].len) {
                return 0;
            }
            if ((local_offset + size) > mMetaBuffer[mNextBuffer].len) {
                size = mMetaBuffer[mNextBuffer].len - local_offset;
            }
        }
        memcpy(data, mMetaBuffer[mNextBuffer].data + local_offset, size);
        return size;
    }
    return NOT_ENOUGH_DATA;
}


FileSource::FileSource(const char *filename)
    : mFd(-1),
      mOffset(0),
      mLength(-1),
      mDecryptHandle(NULL),
      mDrmManagerClient(NULL),
      mDrmBufOffset(0),
      mDrmBufSize(0),
      mDrmBuf(NULL){

    mFd = open(filename, O_LARGEFILE | O_RDONLY);

    if (mFd >= 0) {
        mLength = lseek64(mFd, 0, SEEK_END);
    } else {
        ALOGE("Failed to open file '%s'. (%s)", filename, strerror(errno));
    }
}

FileSource::FileSource(int fd, int64_t offset, int64_t length)
    : mFd(fd),
      mOffset(offset),
      mLength(length),
      mDecryptHandle(NULL),
      mDrmManagerClient(NULL),
      mDrmBufOffset(0),
      mDrmBufSize(0),
      mDrmBuf(NULL){
    CHECK(offset >= 0);
    CHECK(length >= 0);
}

FileSource::~FileSource() {
    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }

    if (mDrmBuf != NULL) {
        delete[] mDrmBuf;
        mDrmBuf = NULL;
    }

    if (mDecryptHandle != NULL) {
        // To release mDecryptHandle
        CHECK(mDrmManagerClient);
        mDrmManagerClient->closeDecryptSession(mDecryptHandle);
        mDecryptHandle = NULL;
    }

    if (mDrmManagerClient != NULL) {
        delete mDrmManagerClient;
        mDrmManagerClient = NULL;
    }
}

status_t FileSource::initCheck() const {
    return mFd >= 0 ? OK : NO_INIT;
}

ssize_t FileSource::readAt(off64_t offset, void *data, size_t size) {
    if (mFd < 0) {
        return NO_INIT;
    }

    Mutex::Autolock autoLock(mLock);

    if (mLength >= 0) {
        if (offset >= mLength) {
            return 0;  // read beyond EOF.
        }
        int64_t numAvailable = mLength - offset;
        if ((int64_t)size > numAvailable) {
            size = numAvailable;
        }
    }

    if (mDecryptHandle != NULL && DecryptApiType::CONTAINER_BASED
            == mDecryptHandle->decryptApiType) {
        return readAtDRM(offset, data, size);
   } else {
        nsecs_t now = systemTime();
        int ret = mBuffer.readFromBuffer(offset + mOffset, data, size, mFd);
        unsigned int t = ns2us(systemTime() - now);
        if ( t > MAX_READ_TIME_US ) {
            ALOGE("Source file read took too long: %d us (%d bytes)\n", t, ret);
        }

        // ret >= 0 is the number of bytes read
        if (ret >= 0) {
            return ret;
        }
        off64_t result = lseek64(mFd, offset + mOffset, SEEK_SET);
        if (result == -1) {
            ALOGE("seek to %lld failed", offset + mOffset);
            return UNKNOWN_ERROR;
        }

        now = systemTime();
        int res = ::read(mFd, data, size);
        t = ns2us(systemTime() - now);
        if ( t > MAX_READ_TIME_US ) {
            ALOGE("Direct source file read took too long: %d us (%d bytes)\n", t, res);
        }
        return res;
    }
}

status_t FileSource::getSize(off64_t *size) {
    Mutex::Autolock autoLock(mLock);

    if (mFd < 0) {
        return NO_INIT;
    }

    *size = mLength;

    return OK;
}

sp<DecryptHandle> FileSource::DrmInitialization(const char *mime) {
    if (mDrmManagerClient == NULL) {
        mDrmManagerClient = new DrmManagerClient();
    }

    if (mDrmManagerClient == NULL) {
        return NULL;
    }

    if (mDecryptHandle == NULL) {
        mDecryptHandle = mDrmManagerClient->openDecryptSession(
                mFd, mOffset, mLength, mime);
    }

    if (mDecryptHandle == NULL) {
        delete mDrmManagerClient;
        mDrmManagerClient = NULL;
    }

    return mDecryptHandle;
}

void FileSource::getDrmInfo(sp<DecryptHandle> &handle, DrmManagerClient **client) {
    handle = mDecryptHandle;

    *client = mDrmManagerClient;
}

ssize_t FileSource::readAtDRM(off64_t offset, void *data, size_t size) {
    size_t DRM_CACHE_SIZE = 1024;
    if (mDrmBuf == NULL) {
        mDrmBuf = new unsigned char[DRM_CACHE_SIZE];
    }

    if (mDrmBuf != NULL && mDrmBufSize > 0 && (offset + mOffset) >= mDrmBufOffset
            && (offset + mOffset + size) <= (mDrmBufOffset + mDrmBufSize)) {
        /* Use buffered data */
        memcpy(data, (void*)(mDrmBuf+(offset+mOffset-mDrmBufOffset)), size);
        return size;
    } else if (size <= DRM_CACHE_SIZE) {
        /* Buffer new data */
        mDrmBufOffset =  offset + mOffset;
        mDrmBufSize = mDrmManagerClient->pread(mDecryptHandle, mDrmBuf,
                DRM_CACHE_SIZE, offset + mOffset);
        if (mDrmBufSize > 0) {
            int64_t dataRead = 0;
            dataRead = size > mDrmBufSize ? mDrmBufSize : size;
            memcpy(data, (void*)mDrmBuf, dataRead);
            return dataRead;
        } else {
            return mDrmBufSize;
        }
    } else {
        /* Too big chunk to cache. Call DRM directly */
        return mDrmManagerClient->pread(mDecryptHandle, data, size, offset + mOffset);
    }
}
}  // namespace android
