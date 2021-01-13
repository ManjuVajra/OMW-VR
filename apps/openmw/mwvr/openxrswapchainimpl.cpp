#include "openxrswapchainimpl.hpp"
#include "openxrdebug.hpp"
#include "vrenvironment.hpp"
#include "vrframebuffer.hpp"

#include <components/debug/debuglog.hpp>

#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>

#elif __linux__
#include <X11/Xlib.h>
#include <GL/glx.h>
#undef None

#else
#error Unsupported platform
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace MWVR {
    OpenXRSwapchainImpl::OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config)
        : mConfig(config)
    {
        if (mConfig.selectedWidth <= 0)
            throw std::invalid_argument("Width must be a positive integer");
        if (mConfig.selectedHeight <= 0)
            throw std::invalid_argument("Height must be a positive integer");
        if (mConfig.selectedSamples <= 0)
            throw std::invalid_argument("Samples must be a positive integer");

        mSwapchain.reset(new SwapchainPrivate(state, mConfig, SwapchainPrivate::Use::COLOR));
        mConfig.selectedSamples = mSwapchain->samples();

        auto* xr = Environment::get().getManager();

        if (xr->xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
        {
            try
            {
                mSwapchainDepth.reset(new SwapchainPrivate(state, mConfig, SwapchainPrivate::Use::DEPTH));
            }
            catch (std::exception& e)
            {
                Log(Debug::Warning) << "XR_KHR_composition_layer_depth was enabled but creating depth swapchain failed: " << e.what();
                mSwapchainDepth = nullptr;
            }
        }

        uint32_t imageCount = mSwapchain->count();
        for (uint32_t i = 0; i < imageCount; i++)
        {
            uint32_t colorBuffer = mSwapchain->bufferAt(i);
            uint32_t depthBuffer = mSwapchainDepth ? mSwapchainDepth->bufferAt(i) : 0;
            mRenderBuffers.emplace_back(new VRFramebuffer(state, mSwapchain->width(), mSwapchain->height(), mSwapchain->samples(), colorBuffer, depthBuffer));
        }
    }

    OpenXRSwapchainImpl::~OpenXRSwapchainImpl()
    {
    }

    VRFramebuffer* OpenXRSwapchainImpl::renderBuffer() const
    {
        checkAcquired();
        // Note that I am trusting that the openxr runtime won't diverge the indexes of the depth and color swapchains so long as these are always acquired together.
        // If some dumb ass implementation decides to violate this we'll just have to work around that if it actually happens.
        return mRenderBuffers[mSwapchain->acuiredIndex()].get();
    }

    uint32_t OpenXRSwapchainImpl::acquiredColorTexture() const
    {
        checkAcquired();
        return mSwapchain->acuiredBuffer();
    }

    uint32_t OpenXRSwapchainImpl::acquiredDepthTexture() const
    {
        if (mSwapchainDepth)
        {
            checkAcquired();
            return mSwapchainDepth->acuiredBuffer();
        }
        return 0;
    }

    bool OpenXRSwapchainImpl::isAcquired() const
    {
        return mFormallyAcquired;
    }

    void OpenXRSwapchainImpl::beginFrame(osg::GraphicsContext* gc)
    {
        acquire();
        renderBuffer()->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
    }

    int swapCount = 0;

    void OpenXRSwapchainImpl::endFrame(osg::GraphicsContext* gc)
    {
        checkAcquired();
        release();
    }

    void OpenXRSwapchainImpl::acquire()
    {
        if (isAcquired())
            throw std::logic_error("Trying to acquire already acquired swapchain");

        if (!mShouldRelease)
        {
            mSwapchain->acquire();
            mShouldRelease = mSwapchain->isAcquired();
            if (mSwapchainDepth && mSwapchain->isAcquired())
            {
                mSwapchainDepth->acquire();
                mShouldRelease = mSwapchainDepth->isAcquired();
            }
        }

        mFormallyAcquired = true;
    }

    void OpenXRSwapchainImpl::release()
    {
        if (mShouldRelease)
        {
            mSwapchain->release();
            mShouldRelease = mSwapchain->isAcquired();
            if (mSwapchainDepth)
            {
                mSwapchainDepth->release();
                mShouldRelease = mSwapchainDepth->isAcquired();
            }
        }

        mFormallyAcquired = false;
    }

    void OpenXRSwapchainImpl::checkAcquired() const
    {
        if (!isAcquired())
            throw std::logic_error("Swapchain must be acquired before use. Call between OpenXRSwapchain::beginFrame() and OpenXRSwapchain::endFrame()");
    }

    static int64_t selectFormat(const std::vector<int64_t>& eligibleFormats, const std::vector<int64_t>& requestedFormats)
    {
        auto it =
            std::find_first_of(std::begin(requestedFormats), std::end(requestedFormats),
                eligibleFormats.begin(), eligibleFormats.end());
        if (it == std::end(requestedFormats))
        {
            return 0;
        }
        return *it;
    }

    static int64_t selectColorFormat(const std::vector<int64_t>& eligibleFormats)
    {
        // Find supported color swapchain format.
        std::vector<int64_t> requestedColorSwapchainFormats = {
            0x8058, // GL_RGBA8
            0x8F97, // GL_RGBA8_SNORM
            0x881A, // GL_RGBA16F
            0x881B, // GL_RGB16F
            // Offered by SteamVR but is broken: // 0x805B, // GL_RGBA16
            0x8C3A, // GL_R11F_G11F_B10F
            // We manage gamma ourselves: 0x8C43, // GL_SRGB8_ALPHA8
            // We manage gamma ourselves: 0x8C41, // GL_SRGB8
        };

        return selectFormat(eligibleFormats, requestedColorSwapchainFormats);
    }

    static int64_t selectDepthFormat(const std::vector<int64_t>& eligibleFormats)
    {
        // Find supported depth swapchain format.
        std::vector<int64_t> requestedDepthSwapchainFormats = {
            0x81A6, // GL_DEPTH_COMPONENT24
            0x88F0, // GL_DEPTH24_STENCIL8
            0x8CAC, // GL_DEPTH_COMPONENT32F
            0x81A7, // GL_DEPTH_COMPONENT32
            0x8DAB, // GL_DEPTH_COMPONENT32F_NV
            0x8CAD, // GL_DEPTH32_STENCIL8
            // Need 32bit minimum: // 0x81A5, // GL_DEPTH_COMPONENT16
        };

        return selectFormat(eligibleFormats, requestedDepthSwapchainFormats);
    }

    OpenXRSwapchainImpl::SwapchainPrivate::SwapchainPrivate(osg::ref_ptr<osg::State> state, SwapchainConfig config, Use use)
        : mBuffers()
        , mWidth(config.selectedWidth)
        , mHeight(config.selectedHeight)
        , mSamples(config.selectedSamples)
    {
        auto* xr = Environment::get().getManager();

        // Select a swapchain format.
        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().xrSession(), 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().xrSession(), (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

        if (use == Use::COLOR)
            mFormat = selectColorFormat(swapchainFormats);
        else
            mFormat = selectDepthFormat(swapchainFormats);
        std::string typeString = use == Use::COLOR ? "color" : "depth";

        if (mFormat == 0) {
            throw std::runtime_error(std::string("Swapchain ") + typeString + " format not supported");
        }
        Log(Debug::Verbose) << "Selected " << typeString << " format: " << std::dec << mFormat << " (" << std::hex << mFormat << ")" << std::dec;

        if (xr->xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
        {
            // TODO
        }

        XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.width = mWidth;
        swapchainCreateInfo.height = mHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;

        while (mSamples > 0 && mSwapchain == XR_NULL_HANDLE)
        {
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << mWidth << " Heigh=" << mHeight << " SampleCount=" << mSamples;
            // First create the swapchain of color buffers.
            swapchainCreateInfo.format = mFormat;
            swapchainCreateInfo.sampleCount = mSamples;
            swapchainCreateInfo.usageFlags =
                use == Use::COLOR ? XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
                                  : XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchain);
            if (!XR_SUCCEEDED(res))
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << mSamples << ": " << XrResultString(res);
                mSamples /= 2;
                if (mSamples == 0)
                    throw std::runtime_error(XrResultString(res));
                continue;
            }
            VrDebug::setName(mSwapchain, "OpenMW XR Color Swapchain " + config.name);
        }

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));
        mBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mBuffers.data())));
        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };
    }

    OpenXRSwapchainImpl::SwapchainPrivate::~SwapchainPrivate()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    uint32_t OpenXRSwapchainImpl::SwapchainPrivate::bufferAt(uint32_t index) const
    {
        return mBuffers[index].image;
    }

    uint32_t OpenXRSwapchainImpl::SwapchainPrivate::count() const
    {
        return mBuffers.size();
    }

    uint32_t OpenXRSwapchainImpl::SwapchainPrivate::acuiredBuffer() const
    {
        checkAcquired();
        return mBuffers[mAcquiredIndex].image;
    }
    bool OpenXRSwapchainImpl::SwapchainPrivate::isAcquired() const
    {
        return mIsReady;
    }

    void OpenXRSwapchainImpl::SwapchainPrivate::acquire()
    {
        auto xr = Environment::get().getManager();
        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        if (!mIsIndexAcquired)
        {
            mIsIndexAcquired = XR_SUCCEEDED(CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &mAcquiredIndex)));
            if (mIsIndexAcquired)
                xr->xrResourceAcquired();
        }
        if (mIsIndexAcquired && !mIsReady)
        {
            mIsReady = XR_SUCCEEDED(CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo)));
        }
    }
    void OpenXRSwapchainImpl::SwapchainPrivate::release()
    {
        auto xr = Environment::get().getManager();

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        if (mIsReady)
        {
            mIsReady = !XR_SUCCEEDED(CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo)));
            if (!mIsReady)
            {
                mIsIndexAcquired = false;
                xr->xrResourceReleased();
            }
        }
    }
    void OpenXRSwapchainImpl::SwapchainPrivate::checkAcquired() const
    {
        if (!isAcquired())
            throw std::logic_error("Swapchain must be acquired before use. Call between OpenXRSwapchain::beginFrame() and OpenXRSwapchain::endFrame()");
    }
}
