#pragma once
#include <memory>
#include "Framebuffer.h"
#include "Texture.h"

namespace RTBEngine {
    namespace Rendering {

        class ShadowMap {
        public:
            ShadowMap(int resolution = 1024);
            ~ShadowMap();

            ShadowMap(const ShadowMap&) = delete;
            ShadowMap& operator=(const ShadowMap&) = delete;

            bool Initialize();
            void BindForWriting() const;
            void BindForReading(unsigned int textureUnit) const;
            void Unbind() const;

            int GetResolution() const { return resolution; }
            Texture* GetDepthTexture() { return depthTexture.get(); }

        private:
            int resolution;
            std::unique_ptr<Framebuffer> framebuffer;
            std::unique_ptr<Texture> depthTexture;
        };

    }
}
