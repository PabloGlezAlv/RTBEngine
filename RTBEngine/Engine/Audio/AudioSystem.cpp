#include "AudioSystem.h"
#include <iostream>
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Audio {

        AudioSystem& AudioSystem::GetInstance() {
            static AudioSystem instance;
            return instance;
        }

        AudioSystem::AudioSystem()
            : fmodSystem(nullptr)
            , isInitialized(false) {
        }

        AudioSystem::~AudioSystem() {
            Shutdown();
        }

        bool AudioSystem::Initialize(int maxChannels) {
            if (isInitialized) {
                return true;
            }

            FMOD::System* rawSystem = nullptr;
            FMOD_RESULT result = FMOD::System_Create(&rawSystem);
            if (result != FMOD_OK) {
                RTB_ERROR("FMOD: Failed to create system");
                return false;
            }

            result = rawSystem->init(maxChannels, FMOD_INIT_NORMAL, nullptr);
            if (result != FMOD_OK) {
                RTB_ERROR("FMOD: Failed to initialize system");
                rawSystem->release();
                return false;
            }

            fmodSystem = std::unique_ptr<FMOD::System, FMODSystemDeleter>(rawSystem);
            isInitialized = true;
            return true;
        }

        void AudioSystem::Update() {
            if (fmodSystem) {
                fmodSystem->update();
            }
        }

        void AudioSystem::Shutdown() {
            fmodSystem.reset();
            isInitialized = false;
        }

    }
}
