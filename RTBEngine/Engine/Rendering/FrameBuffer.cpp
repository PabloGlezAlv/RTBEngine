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
            colorOnlyContinue.reset();
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
            ownsColorTexture = true;
            ownsDepthTexture = true;

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

        bool Framebuffer::SyncColorOnlyContinue(const Framebuffer& source)
        {
            if (source.GetColorTextureID() == RHI::kInvalidGpuId
                || source.GetWidth() <= 0 || source.GetHeight() <= 0) {
                return false;
            }

            const bool needsRebuild =
                fboID == RHI::kInvalidGpuId
                || colorTextureID != source.GetColorTextureID()
                || width != source.GetWidth()
                || height != source.GetHeight();

            if (!needsRebuild) {
                return IsComplete();
            }

            if (fboID != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyFramebuffer(fboID);
                fboID = RHI::kInvalidGpuId;
            }

            width = source.GetWidth();
            height = source.GetHeight();
            colorTextureID = source.GetColorTextureID();
            depthTextureID = RHI::kInvalidGpuId;
            ownsColorTexture = false;
            ownsDepthTexture = false;

            if (!Create()) {
                RTB_ERROR("Framebuffer: Failed to create color-only continue FBO");
                return false;
            }

            Bind();
            AttachColorTexture(colorTextureID);
            // No depth attachment on purpose — allows sampling source depth safely.
            const bool ok = IsComplete();
            Unbind();
            if (!ok) {
                RTB_ERROR("Framebuffer: color-only continue FBO is not complete");
            }
            return ok;
        }

        Framebuffer* Framebuffer::GetColorOnlyContinueTarget()
        {
            if (!colorOnlyContinue) {
                colorOnlyContinue = std::make_unique<Framebuffer>();
            }
            if (!colorOnlyContinue->SyncColorOnlyContinue(*this)) {
                return nullptr;
            }
            return colorOnlyContinue.get();
        }

        void Framebuffer::Resize(int w, int h) {
            if (width == w && height == h) return;

            width = w;
            height = h;

            if (!ownsColorTexture) {
                // Color-only continue targets are rebuilt via SyncColorOnlyContinue.
                return;
            }

            DeleteTextures();

            Bind();
            CreateTextures();
            Unbind();

            if (colorOnlyContinue) {
                colorOnlyContinue->SyncColorOnlyContinue(*this);
            }
        }

        void Framebuffer::CreateTextures() {
            auto& device = Device();
            colorTextureID = device.CreateColorTextureForFramebuffer(width, height);
            device.AttachFramebufferColorTexture(fboID, colorTextureID);

            depthTextureID = device.CreateDepthTextureForFramebuffer(width, height);
            device.AttachFramebufferDepthTexture(fboID, depthTextureID);
            ownsColorTexture = true;
            ownsDepthTexture = true;
        }

        void Framebuffer::DeleteTextures() {
            auto& device = Device();
            if (ownsColorTexture && colorTextureID != RHI::kInvalidGpuId) {
                device.DestroyTexture(colorTextureID);
            }
            colorTextureID = RHI::kInvalidGpuId;

            if (ownsDepthTexture && depthTextureID != RHI::kInvalidGpuId) {
                device.DestroyTexture(depthTextureID);
            }
            depthTextureID = RHI::kInvalidGpuId;
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
