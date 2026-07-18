#include "Framebuffer.h"
#include "RHI/RenderDevice.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {

        namespace {
            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        Framebuffer::Framebuffer() {}

        Framebuffer::~Framebuffer() {
            if (!RHI::RenderDevice::HasDevice()) {
                fboID = colorTextureID = depthTextureID = RHI::kInvalidGpuId;
                return;
            }
            DeleteTextures();
            if (fboID != RHI::kInvalidGpuId) {
                Device().DestroyFramebuffer(fboID);
                fboID = RHI::kInvalidGpuId;
            }
        }

        bool Framebuffer::Create() {
            fboID = Device().CreateFramebuffer();
            return fboID != RHI::kInvalidGpuId;
        }

        bool Framebuffer::CreateWithColorAndDepth(int w, int h) {
            width = w;
            height = h;

            if (!Create()) {
                RTB_ERROR("Framebuffer: Failed to create FBO");
                return false;
            }

            Bind();
            CreateTextures();

            if (!IsComplete()) {
                RTB_ERROR("Framebuffer: Framebuffer is not complete");
                Unbind();
                return false;
            }

            Unbind();
            return true;
        }

        void Framebuffer::Resize(int w, int h) {
            if (width == w && height == h) return;

            width = w;
            height = h;

            DeleteTextures();

            Bind();
            CreateTextures();
            Unbind();
        }

        void Framebuffer::CreateTextures() {
            auto& device = Device();
            colorTextureID = device.CreateColorTextureForFramebuffer(width, height);
            device.AttachFramebufferColorTexture(fboID, colorTextureID);

            depthTextureID = device.CreateDepthTextureForFramebuffer(width, height);
            device.AttachFramebufferDepthTexture(fboID, depthTextureID);
        }

        void Framebuffer::DeleteTextures() {
            auto& device = Device();
            if (colorTextureID != RHI::kInvalidGpuId) {
                device.DestroyTexture(colorTextureID);
                colorTextureID = RHI::kInvalidGpuId;
            }
            if (depthTextureID != RHI::kInvalidGpuId) {
                device.DestroyTexture(depthTextureID);
                depthTextureID = RHI::kInvalidGpuId;
            }
        }

        void Framebuffer::Bind() const {
            Device().BindFramebuffer(fboID);
        }

        void Framebuffer::Unbind() const {
            Device().UnbindFramebuffer();
        }

        void Framebuffer::AttachDepthTexture(unsigned int textureID) {
            Device().AttachFramebufferDepthTexture(fboID, textureID);
            Device().SetFramebufferDrawReadNone();
        }

        void Framebuffer::AttachColorTexture(unsigned int textureID) {
            Device().AttachFramebufferColorTexture(fboID, textureID);
        }

        bool Framebuffer::IsComplete() const {
            return Device().IsFramebufferComplete();
        }

    }
}
