#pragma once

#include "Component.h"

namespace FMOD {
    class Channel;
}

namespace RTBEngine {
    namespace Audio {
        class AudioClip;
    }

    namespace ECS {

        class AudioSourceComponent : public Component {
        public:
            AudioSourceComponent();
            virtual ~AudioSourceComponent();

            void SetClip(Audio::AudioClip* clip);
            Audio::AudioClip* GetClip() const { return audioClip; }

            void Play();
            void Stop();
            void Pause();
            void Resume();

            bool IsPlaying() const;

            void SetVolume(float volume);
            float GetVolume() const { return volume; }

            void SetPitch(float pitch);
            float GetPitch() const { return pitch; }

            void SetLoop(bool loop);
            bool IsLooping() const { return loop; }

            void SetPlayOnStart(bool playOnStart) { this->playOnStart = playOnStart; }
            bool GetPlayOnStart() const { return playOnStart; }

            virtual void OnStart() override;

            const char* GetTypeName() const override;
        private:
            Audio::AudioClip* audioClip = nullptr;
            FMOD::Channel* channel = nullptr;

            float volume = 1.0f;
            float pitch = 1.0f;
            bool loop = false;
            bool playOnStart = false;
        };

    }
}
