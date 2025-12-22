#pragma once

#include <fmod.hpp>
#include <string>
#include <memory>

namespace RTBEngine {
    namespace Audio {

        struct FMODSoundDeleter {
            void operator()(FMOD::Sound* sound) const {
                if (sound) {
                    sound->release();
                }
            }
        };

        class AudioClip {
        public:
            AudioClip();
            ~AudioClip();

            bool LoadFromFile(const std::string& filePath, bool stream = false);
            void Unload();

            bool IsLoaded() const { return sound != nullptr; }
            FMOD::Sound* GetSound() const { return sound.get(); }

            const std::string& GetFilePath() const { return filePath; }
            float GetLength() const;

        private:
            std::unique_ptr<FMOD::Sound, FMODSoundDeleter> sound;
            std::string filePath;
        };

    }
}
