#include "vrview.hpp"

#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "vrsession.hpp"
#include "vrenvironment.hpp"

#include <components/debug/debuglog.hpp>

#include <osgViewer/Renderer>

namespace MWVR {

    VRView::VRView(
        std::string name,
        SwapchainConfig config,
        osg::ref_ptr<osg::State> state)
        : mSwapchainConfig{ config }
        , mSwapchain(new OpenXRSwapchain(state, mSwapchainConfig))
        , mName(name)
    {
    }

    VRView::~VRView()
    {
    }

    class CullCallback : public osg::NodeCallback
    {
        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            const auto& name = node->getName();
            if (name == "LeftEye")
                Environment::get().getSession()->beginPhase(VRSession::FramePhase::Cull);
            traverse(node, nv);
        }
    };

    osg::Camera* VRView::createCamera(int order, const osg::Vec4& clearColor, osg::GraphicsContext* gc)
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera();
        camera->setClearColor(clearColor);
        camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setRenderOrder(osg::Camera::PRE_RENDER, order);
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setAllowEventFocus(false);
        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setViewport(0, 0, mSwapchain->width(), mSwapchain->height());
        camera->setGraphicsContext(gc);

        camera->setInitialDrawCallback(new VRView::InitialDrawCallback());
        camera->setCullCallback(new CullCallback);

        return camera.release();
    }

    void VRView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        if(Environment::get().getSession()->getFrame(VRSession::FramePhase::Draw)->mShouldRender)
            mSwapchain->beginFrame(renderInfo.getState()->getGraphicsContext());
    }

    void VRView::InitialDrawCallback::operator()(osg::RenderInfo& renderInfo) const
    {
        const auto& name = renderInfo.getCurrentCamera()->getName();
        if (name == "LeftEye")
            Environment::get().getSession()->beginPhase(VRSession::FramePhase::Draw);

        osg::GraphicsOperation* graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup
            renderer->setCameraRequiresSetUp(false);
        }
    }
    void VRView::UpdateSlaveCallback::updateSlave(
        osg::View& view,
        osg::View::Slave& slave)
    {
        auto* camera = slave._camera.get();
        auto name = camera->getName();

        // Update current cached cull mask of camera if it is active
        auto mask = camera->getCullMask();
        if (mask == 0)
            camera->setCullMask(mCullMask);
        else
            mCullMask = mask;

        if (Environment::get().getSession()->getFrame(VRSession::FramePhase::Update)->mShouldRender)
        {
            Side side = Side::RIGHT_SIDE;
            if (name == "LeftEye")
                side = Side::LEFT_SIDE;

            auto* session = Environment::get().getSession();
            auto viewMatrix = view.getCamera()->getViewMatrix();

            bool haveView = viewMatrix.getTrans().length() > 0.01;

            if (haveView)
            {
                viewMatrix = viewMatrix * session->viewMatrix(VRSession::FramePhase::Update, side, true);
            }
            else
            {
                viewMatrix = session->viewMatrix(VRSession::FramePhase::Update, side, false);
            }
            camera->setViewMatrix(viewMatrix);

            auto projectionMatrix = session->projectionMatrix(VRSession::FramePhase::Update, side);
            camera->setProjectionMatrix(projectionMatrix);
        }
        else
        {
            // If the session is not active, we do not want to waste resources rendering frames.
            camera->setCullMask(0);
        }
        slave.updateSlaveImplementation(view);
    }

    void VRView::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        auto name = renderInfo.getCurrentCamera()->getName();
    }

    void VRView::swapBuffers(osg::GraphicsContext* gc)
    {
        mSwapchain->endFrame(gc);
    }
}
