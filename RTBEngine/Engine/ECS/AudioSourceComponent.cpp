#include "AudioSourceComponent.h"
#include "../Audio/AudioSystem.h"
#include "../Audio/AudioClip.h"
#include <fmod.hpp>

namespace RTBEngine {
    namespace ECS {

        AudioSourceComponent::AudioSourceComponent()
            : audioClip(nullptr)
            , channel(nullptr)
            , volume(1.0f)
            , pitch(1.0f)
            , loop(false)
            , playOnStart(false) {
        }

        AudioSourceComponent::~AudioSourceComponent() {
            Stop();
        }

        void AudioSourceComponent::SetClip(Audio::AudioClip* clip) {
            audioClip = clip;
        }

        void AudioSourceComponent::Play() {
            if (!audioClip || !audioClip->IsLoaded()) {
                return;
            }

            FMOD::System* fmodSystem = Audio::AudioSystem::GetInstance().GetFMODSystem();
            if (!fmodSystem) {
                return;
            }

            fmodSystem->playSound(audioClip->GetSound(), nullptr, false, &channel);

            if (channel) {
                channel->setVolume(volume);
                channel->setPitch(pitch);
                channel->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
            }
        }

        void AudioSourceComponent::Stop() {
            if (channel) {
                channel->stop();
                channel = nullptr;
            }
        }

        void AudioSourceComponent::Pause() {
            if (channel) {
                channel->setPaused(true);
            }
        }

        void AudioSourceComponent::Resume() {
            if (channel) {
                channel->setPaused(false);
            }
        }

        bool AudioSourceComponent::IsPlaying() const {
            if (!channel) {
                return false;
            }

            bool isPlaying = false;
            channel->isPlaying(&isPlaying);
            return isPlaying;
        }

        void AudioSourceComponent::SetVolume(float vol) {
            volume = vol;
            if (channel) {
                channel->setVolume(volume);
            }
        }

        void AudioSourceComponent::SetPitch(float p) {
            pitch = p;
            if (channel) {
                channel->setPitch(pitch);
            }
        }

        void AudioSourceComponent::SetLoop(bool l) {
            loop = l;
            if (channel) {
                channel->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
            }
        }

        void AudioSourceComponent::OnStart() {
            if (playOnStart && audioClip) {
                Play();
            }
        }

        const char* AudioSourceComponent::GetTypeName() const {
            return "AudioSourceComponent";
        }

    }
}

