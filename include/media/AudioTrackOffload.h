
#ifndef ANDROID_AUDIOTRACKOFFLOAD_H
#define ANDROID_AUDIOTRACKOFFLOAD_H

#include <stdint.h>
#include <sys/types.h>

#include <media/IAudioFlinger.h>
#include <media/IAudioTrack.h>
#include <media/AudioTrack.h>
#include <media/AudioSystem.h>

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <binder/IMemory.h>
#include <cutils/sched_policy.h>
#include <utils/threads.h>

namespace android {

// ----------------------------------------------------------------------------

class audio_track_cblk_t;

// ----------------------------------------------------------------------------

class AudioTrackOffload : public AudioTrack
{

public:
    enum event_type {  //If new event type is added (after AudioTrack::EVENT_BUFFER_END), please take care of numbering.
       EVENT_STREAM_END = EVENT_BUFFER_END + 1,       // Sent after all the buffers queued in AF and HW are played back (after stop is called)
       EVENT_TEAR_DOWN = EVENT_STREAM_END + 1         // AudioTrack needs to be flushed, deleted and new track to be created
    };

    AudioTrackOffload();
    /* Overload constructor for offload
     * Creates an audio track and registers it with AudioFlinger.
     * Once created, the track needs to be started before it can be used.
     * Unspecified values are set to the audio hardware's current
     * values.
     *
     * Parameters:
     *
     * streamType:         Select the type of audio stream this track is attached to
     *                     (e.g. AUDIO_STREAM_MUSIC).
     * bitRate             Required for offload buffer size calculation
     * sampleRate:         Track sampling rate in Hz.
     * format:             Audio format (e.g AUDIO_FORMAT_PCM_16_BIT for signed
     *                     16 bits per sample).
     * channelMask:        Channel mask: see audio_channels_t.
     * frameCount:         Minimum size of track PCM buffer in frames. This defines the
     *                     latency of the track. The actual size selected by the AudioTrack could be
     *                     larger if the requested size is not compatible with current audio HAL
     *                     latency.
     * flags:              Reserved for future use.
     * cbf:                Callback function. If not null, this function is called periodically
     *                     to request new PCM data.
     * notificationFrames: The callback function is called each time notificationFrames PCM
     *                     frames have been comsumed from track input buffer.
     * user                Context for use by the callback receiver.
     */

    AudioTrackOffload(audio_stream_type_t streamType,
                      int bitRate,
                      uint32_t sampleRate,
                      audio_format_t format,
                      int channelMask,
                      int frameCount,
                      audio_output_flags_t flags,
                      callback_t cbf,
                      void* user,
                      int notificationFrames,
                      int sessionId);

    status_t    set(audio_stream_type_t streamType = AUDIO_STREAM_DEFAULT,
                    uint32_t sampleRate = 0,
                    audio_format_t format = AUDIO_FORMAT_DEFAULT,
                    audio_channel_mask_t channelMask = 0,
                    int frameCount      = 0,
                    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE,
                    callback_t cbf      = NULL,
                    void* user          = NULL,
                    int notificationFrames = 0,
                    const sp<IMemory>& sharedBuffer = 0,
                    bool threadCanCallJava = false,
                    int sessionId       = 0);

    enum {
          TEAR_DOWN       = 0x80000002,
    };

    /* Get offload buffer size based on bit rate, sample rate and channel count
     * for each track
     */
    size_t      getOffloadBufferSize(uint32_t bitRate,
                                    uint32_t sampleRate,
                                    uint32_t channel,
                                    audio_io_handle_t output = 0);

    /* Set parameters - only possible when using direct output */
    status_t    setParameters(const String8& keyValuePairs);

    status_t    setVolume(float left, float right);

    status_t    getPosition(uint32_t *position);
    status_t    obtainBuffer(Buffer* audioBuffer, int32_t waitCount);

    /* Set offload EOS reached */
    status_t    setOffloadEOSReached(bool value);

    void flush_l();
    bool processAudioBuffer(const sp<AudioTrackThread>& thread);

protected:

    sp<IAudioTrack>         mAudioTrack;

    int                     mWakeTimeMs;
    audio_io_handle_t       mOutput;
    int                     mBitRate;
    bool                    mOffloadEOSReached;
};


}; // namespace android

#endif // ANDROID_AUDIOTRACKOFFLOAD_H
