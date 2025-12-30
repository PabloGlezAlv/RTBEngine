#include "AudioClip.h"
#include "AudioSystem.h"
#include <iostream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Audio {

        AudioClip::AudioClip()
            : sound(nullptr)
            , filePath("") {
        }

        AudioClip::~AudioClip() {
            Unload();
        }

        bool AudioClip::LoadFromFile(const std::string& path, bool stream) {
            Unload();

            FMOD::System* fmodSystem = AudioSystem::GetInstance().GetFMODSystem();
            if (!fmodSystem) {
                RTB_ERROR("AudioClip: AudioSystem not initialized");
                return false;
            }

            FMOD_MODE mode = FMOD_DEFAULT;
            if (stream) {
                mode |= FMOD_CREATESTREAM;
            }
            else {
                mode |= FMOD_CREATESAMPLE;
            }

            FMOD::Sound* rawSound = nullptr;
            FMOD_RESULT result = fmodSystem->createSound(path.c_str(), mode, nullptr, &rawSound);

            if (result != FMOD_OK) {
                RTB_ERROR("AudioClip: Failed to load " + path);
                return false;
            }

            sound = std::unique_ptr<FMOD::Sound, FMODSoundDeleter>(rawSound);
            filePath = path;
            
            return true;
        }

        void AudioClip::Unload() {
            sound.reset();
            filePath.clear();
        }

        float AudioClip::GetLength() const {
            if (!sound) {
                return 0.0f;
            }

            unsigned int lengthMs = 0;
            sound->getLength(&lengthMs, FMOD_TIMEUNIT_MS);
            return lengthMs / 1000.0f;
        }

    }
}
