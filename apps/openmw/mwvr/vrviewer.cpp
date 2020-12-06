#include "vrviewer.hpp"

#include "openxrmanagerimpl.hpp"
#include "openxrswapchain.hpp"
#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "vrframebuffer.hpp"
#include "vrview.hpp"

#include "../mwrender/vismask.hpp"

#include <osgViewer/Renderer>

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <components/misc/stringops.hpp>

namespace MWVR
{

    const std::array<const char*, 2> VRViewer::sViewNames = {
            "LeftEye",
            "RightEye"
    };

    // Callback to do construction with a graphics context
    class RealizeOperation : public osg::GraphicsOperation
    {
    public:
        RealizeOperation() : osg::GraphicsOperation("VRRealizeOperation", false) {};
        void operator()(osg::GraphicsContext* gc) override;
        bool realized();

    private:
    };

    VRViewer::VRViewer(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
        , mMainCamera(viewer->getCamera())
        , mConfigured(false)
    {
        mViewer->setRealizeOperation(new RealizeOperation());
    }

    VRViewer::~VRViewer(void)
    {
    }

    int parseResolution(std::string conf, int recommended, int max)
    {
        if (Misc::StringUtils::isNumber(conf))
        {
            int res = std::atoi(conf.c_str());
            if (res <= 0)
                return recommended;
            if (res > max)
                return max;
            return res;
        }
        conf = Misc::StringUtils::lowerCase(conf);
        if (conf == "auto" || conf == "recommended")
        {
            return recommended;
        }
        if (conf == "max")
        {
            return max;
        }
        return recommended;
    }

    class AdvancePhaseCallback : public osg::NodeCallback
    {
    public:
        AdvancePhaseCallback(VRSession::FramePhase phase)
            : mPhase(phase)
        {}

        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            Environment::get().getSession()->beginPhase(VRSession::FramePhase::Cull);
            traverse(node, nv);
        }

        VRSession::FramePhase mPhase;
    };

    class InitialDrawCallback : public osg::Camera::DrawCallback
    {
    public:
        virtual void operator()(osg::RenderInfo& renderInfo) const
        {
            const auto& name = renderInfo.getCurrentCamera()->getName();
            Environment::get().getSession()->beginPhase(VRSession::FramePhase::Draw);
                
            osg::GraphicsOperation* graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
            osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
            if (renderer != nullptr)
            {
                // Disable normal OSG FBO camera setup
                renderer->setCameraRequiresSetUp(false);
            }
        }
    };

    void VRViewer::realize(osg::GraphicsContext* context)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mConfigured)
        {
            return;
        }

        // Give the main camera an initial draw callback that disables camera setup (we don't want it)
        auto mainCamera = mViewer->getCamera();
        mainCamera->setName("Main");
        mainCamera->setInitialDrawCallback(new InitialDrawCallback());

        auto* xr = Environment::get().getManager();
        xr->realize(context);

        // Run through initial events to start session
        // For the rest of runtime this is handled by vrsession
        xr->handleEvents();

        //// Configure eyes, their cameras, and their enslavement.
        auto config = xr->getRecommendedSwapchainConfig();
        bool mirror = Settings::Manager::getBool("mirror texture", "VR");

        std::array<std::string, 2> xConfString;
        std::array<std::string, 2> yConfString;
        xConfString[0] = Settings::Manager::getString("left eye resolution x", "VR");
        yConfString[0] = Settings::Manager::getString("left eye resolution y", "VR");

        xConfString[1] = Settings::Manager::getString("right eye resolution x", "VR");
        yConfString[1] = Settings::Manager::getString("right eye resolution y", "VR");

        SwapchainConfig flatConfig;
        flatConfig.selectedWidth = 0;
        flatConfig.selectedHeight = 0;
        flatConfig.selectedSamples = 1;

        for (unsigned i = 0; i < sViewNames.size(); i++)
        {
            config[i].selectedWidth = parseResolution(xConfString[i], config[i].recommendedWidth, config[i].maxWidth);
            config[i].selectedHeight = parseResolution(yConfString[i], config[i].recommendedHeight, config[i].maxHeight);

            config[i].selectedSamples = Settings::Manager::getInt("antialiasing", "Video");
            // OpenXR requires a non-zero value
            if (config[i].selectedSamples < 1)
                config[i].selectedSamples = 1;


            mLayerStack[i].subImage.x = flatConfig.selectedWidth;
            mLayerStack[i].subImage.y = 0;
            mLayerStack[i].subImage.w = config[i].selectedWidth;
            mLayerStack[i].subImage.h = config[i].selectedHeight;

            flatConfig.selectedWidth += config[i].selectedWidth;
            flatConfig.selectedHeight = std::max(flatConfig.selectedHeight, config[i].selectedHeight);
            flatConfig.selectedSamples = std::max(flatConfig.selectedSamples, config[i].selectedSamples);

            auto name = sViewNames[i];
            Log(Debug::Verbose) << name << " resolution: Recommended x=" << config[i].recommendedWidth << ", y=" << config[i].recommendedHeight;
            Log(Debug::Verbose) << name << " resolution: Max x=" << config[i].maxWidth << ", y=" << config[i].maxHeight;
            Log(Debug::Verbose) << name << " resolution: Selected x=" << config[i].selectedWidth << ", y=" << config[i].selectedHeight;
        }
        mSwapchain.reset(new OpenXRSwapchain(context->getState(), flatConfig));
        mLayerStack[0].subImage.swapchain = mSwapchain.get();
        mLayerStack[1].subImage.swapchain = mSwapchain.get();

        mUseSlave = true;

        if (mUseSlave)
        {
            mStereoSlave = new osg::Camera();
            mStereoSlave->setClearColor(mMainCamera->getClearColor());
            mStereoSlave->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            mStereoSlave->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
            mStereoSlave->setRenderOrder(osg::Camera::PRE_RENDER);
            mStereoSlave->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
            mStereoSlave->setAllowEventFocus(false);
            mStereoSlave->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
            mStereoSlave->setGraphicsContext(context);
            mStereoSlave->setCullingMode(mainCamera->getCullingMode() | osg::Camera::FAR_PLANE_CULLING);
            mViewer->addSlave(mStereoSlave);
        }
        else
        {
            mStereoSlave = mainCamera;
            mainCamera->setCullMask(mainCamera->getCullMask() & ~MWRender::Mask_GUI);
        }

        mStereoSlave->setViewport(0, 0, flatConfig.selectedWidth, flatConfig.selectedHeight);
        mStereoSlave->setInitialDrawCallback(new InitialDrawCallback());
        mStereoSlave->setCullCallback(new AdvancePhaseCallback(VRSession::FramePhase::Cull));
        mStereoSlave->setPreDrawCallback(mPreDraw);
        mStereoSlave->setFinalDrawCallback(mPostDraw);
        mStereoSlave->setCullMask(~MWRender::Mask_GUI & ~MWRender::Mask_SimpleWater & ~MWRender::Mask_UpdateVisitor);
        mStereoSlave->setName("StereoView");
        mMsaaResolveMirrorTexture.reset(new VRFramebuffer(context->getState(), mStereoSlave->getViewport()->width(), mStereoSlave->getViewport()->height(), 0));
        mMirrorTexture.reset(new VRFramebuffer(context->getState(), mMainCamera->getViewport()->width(), mMainCamera->getViewport()->height(), 0));
        mMainCameraGC = mainCamera->getGraphicsContext();
        mMainCameraGC->setSwapCallback(new VRViewer::SwapBuffersCallback(this));
        if (mUseSlave)
            mainCamera->setGraphicsContext(nullptr);
        mConfigured = true;

        Log(Debug::Verbose) << "Realized";
    }

    void VRViewer::swapBuffers(osg::GraphicsContext* gc)
    {
        auto* session = Environment::get().getSession();
        Environment::get().getSession()->beginPhase(VRSession::FramePhase::Swap);
        if (Environment::get().getSession()->getFrame(VRSession::FramePhase::Swap)->mShouldRender)
        {
            blitEyesToMirrorTexture(gc);
            mSwapchain->endFrame(gc);
            gc->swapBuffersImplementation();
        }
        session->swapBuffers(gc, *this);
    }

    void VRViewer::setStereoView(Misc::StereoView* stereoView)
    {
        mStereoView = stereoView;
        if (mUseSlave)
            mStereoView->useSlaveCamera(mViewer->findSlaveIndexForCamera(mStereoSlave));
        else
            mStereoSlave->setCullMask(mStereoSlave->getCullMask() & ~MWRender::Mask_GUI);
        //mStereoView->setUpdateViewCallback();
    }

    void VRViewer::blitEyesToMirrorTexture(osg::GraphicsContext* gc)
    {
        if (!mMsaaResolveMirrorTexture)
        {
            return;
        }

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        mMsaaResolveMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        mSwapchain->renderBuffer()->blit(gc, 0, 0, mMsaaResolveMirrorTexture->width(), mMsaaResolveMirrorTexture->height());
        mMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        mMsaaResolveMirrorTexture->blit(gc, 0, 0, mMainCamera->getViewport()->width(), mMainCamera->getViewport()->height());
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        mMirrorTexture->blit(gc, 0, 0, mMainCamera->getViewport()->width(), mMainCamera->getViewport()->height());
    }

    void
        VRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        mViewer->swapBuffers(gc);
    }

    void
        RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        return Environment::get().getViewer()->realize(gc);
    }

    bool
        RealizeOperation::realized()
    {
        return Environment::get().getViewer()->realized();
    }

    void VRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        if (Environment::get().getSession()->getFrame(VRSession::FramePhase::Draw)->mShouldRender)
            mSwapchain->beginFrame(info.getState()->getGraphicsContext());
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        if (camera->getPreDrawCallback() != mPreDraw)
        {
            camera->setPreDrawCallback(mPreDraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }
    }
}
