#pragma once

#include <fmod.hpp>
#include <memory>
#include <string>

namespace RTBEngine {
    namespace Audio {

        struct FMODSystemDeleter {
            void operator()(FMOD::System* system) const {
                if (system) {
                    system->release();
                }
            }
        };

        class AudioSystem {
        public:
            static AudioSystem& GetInstance();

            bool Initialize(int maxChannels = 512);
            void Update();
            void Shutdown();

            FMOD::System* GetFMODSystem() const { return fmodSystem.get(); }

        private:
            AudioSystem();
            ~AudioSystem();

            AudioSystem(const AudioSystem&) = delete;
            AudioSystem& operator=(const AudioSystem&) = delete;

            //Second parameter personalized deleter
            std::unique_ptr<FMOD::System, FMODSystemDeleter> fmodSystem;
            bool isInitialized = false;
        };

    }
}
