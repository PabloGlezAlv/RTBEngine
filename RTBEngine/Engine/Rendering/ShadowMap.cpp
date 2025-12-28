#include "ShadowMap.h"
#include <GL/glew.h>

namespace RTBEngine {
    namespace Rendering {

        ShadowMap::ShadowMap(int resolution) : resolution(resolution) {}

        ShadowMap::~ShadowMap() {}

        bool ShadowMap::Initialize() {
            framebuffer = std::make_unique<Framebuffer>();
            if (!framebuffer->Create()) {
                return false;
            }

            depthTexture = std::make_unique<Texture>();
            if (!depthTexture->CreateDepthTexture(resolution, resolution)) {
                return false;
            }

            framebuffer->Bind();
            framebuffer->AttachDepthTexture(depthTexture->GetID());

            if (!framebuffer->IsComplete()) {
                return false;
            }

            framebuffer->Unbind();
            return true;
        }

        void ShadowMap::BindForWriting() const {
            framebuffer->Bind();
        }

        void ShadowMap::BindForReading(unsigned int textureUnit) const {
            depthTexture->Bind(textureUnit);
        }

        void ShadowMap::Unbind() const {
            framebuffer->Unbind();
        }

    }
}
