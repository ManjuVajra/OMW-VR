#ifndef OPENXR_SWAPCHAINIMPL_HPP
#define OPENXR_SWAPCHAINIMPL_HPP

#include "openxrswapchain.hpp"
#include "openxrmanagerimpl.hpp"

struct XrSwapchainSubImage;

namespace MWVR
{
    /// \brief Implementation of OpenXRSwapchain
    class OpenXRSwapchainImpl
    {
    public:
        OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config);
        ~OpenXRSwapchainImpl();

        void beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc);

        VRFramebuffer* renderBuffer() const;
        uint32_t acquiredColorTexture() const;
        uint32_t acquiredDepthTexture() const;

        bool isAcquired() const;
        XrSwapchain xrSwapchain(void) const { return mSwapchain; };
        XrSwapchainSubImage xrSubImage(void) const { return mSubImage; };
        int width() const { return mWidth; };
        int height() const { return mHeight; };
        int samples() const { return mSamples; };

    protected:
        OpenXRSwapchainImpl(const OpenXRSwapchainImpl&) = delete;
        void operator=(const OpenXRSwapchainImpl&) = delete;

        void acquire(osg::GraphicsContext* gc);
        void release(osg::GraphicsContext* gc);
        void checkAcquired() const;

    private:
        XrSwapchain mSwapchain = XR_NULL_HANDLE;
        XrSwapchain mDepthSwapchain = XR_NULL_HANDLE;
        std::vector<XrSwapchainImageOpenGLKHR> mSwapchainImageBuffers{};
        std::vector<XrSwapchainImageOpenGLKHR> mDepthSwapchainImageBuffers{};
        XrSwapchainSubImage mSubImage{};
        int32_t mWidth = -1;
        int32_t mHeight = -1;
        int32_t mSamples = -1;
        int64_t mSwapchainColorFormat = -1;
        int64_t mSwapchainDepthFormat = -1;
        uint32_t mFBO = 0;
        std::vector<std::unique_ptr<VRFramebuffer> > mRenderBuffers{};
        int mRenderBuffer{ 0 };
        uint32_t mAcquiredImageIndex{ 0 };
        bool mIsAcquired{ false };
    };
}

#endif