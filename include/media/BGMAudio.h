#ifndef BGM_AUDIO_H_

#define BGM_AUDIO_H_

namespace android {

class BGMAudio {
public:
    BGMAudio();
    ~BGMAudio();
    bool mAudioPlayerPaused;
    bool mBGMEnabled;
    bool mBGMAudioAvailable;
    int mBGMAudioSessionID;
};

}  // namespace android

#endif  // BGM_AUDIO_H_
