#include "session.hpp"
#include "instance.hpp"
#include "debug.hpp"
#include "typeconversion.hpp"

#include <components/vr/trackingsource.hpp>
#include <components/vr/frame.hpp>
#include <components/vr/layer.hpp>

#include <cassert>

namespace XR
{
    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
    {
        // Now do the computation
        osg::Matrixd m2(osg::Matrixd::rotate(quat));
        double* mat = (double*)m2.ptr();
        double angle_x = 0.0;
        double angle_y = 0.0;
        double angle_z = 0.0;
        double D, C, tr_x, tr_y;
        angle_y = D = asin(mat[2]); /* Calculate Y-axis angle */
        C = cos(angle_y);
        if (fabs(C) > 0.005) /* Test for Gimball lock? */
        {
            tr_x = mat[10] / C; /* No, so get X-axis angle */
            tr_y = -mat[6] / C;
            angle_x = atan2(tr_y, tr_x);
            tr_x = mat[0] / C; /* Get Z-axis angle */
            tr_y = -mat[1] / C;
            angle_z = atan2(tr_y, tr_x);
        }
        else /* Gimball lock has occurred */
        {
            angle_x = 0; /* Set X-axis angle to zero
            */
            tr_x = mat[5]; /* And calculate Z-axis angle
            */
            tr_y = mat[4];
            angle_z = atan2(tr_y, tr_x);
        }

        yaw = angle_z;
        pitch = angle_x;
        roll = angle_y;
    }

    static Session* sSession = nullptr;

    Session& Session::instance()
    {
        assert(sSession);
        return *sSession;
    }

    Session::Session(XrSession session, XrViewConfigurationType viewConfigType)
        : mXrSession(session)
        , mViewConfigType(viewConfigType)
        , mTracker(nullptr)
    {
        if (!sSession)
            sSession = this;
        else
            throw std::logic_error("Duplicated XR::Session singleton");

        Debugging::setName(mXrSession, "OpenMW XR Session");

        init();
    }

    Session::~Session()
    {
        cleanup();
    }

    void Session::xrResourceAcquired()
    {
        std::scoped_lock lock(mMutex);
        mAcquiredResources++;
    }

    void Session::xrResourceReleased()
    {
        assert(mAcquiredResources != 0);

        std::scoped_lock lock(mMutex);
        mAcquiredResources--;
    }

    void Session::newFrame(uint64_t frameNo, bool& shouldSyncFrame, bool& shouldSyncInput)
    {
        handleEvents();
        shouldSyncFrame = mAppShouldSyncFrameLoop;
        shouldSyncInput = mAppShouldReadInput;
    }

    void Session::syncFrameUpdate(uint64_t frameNo, bool& shouldRender, uint64_t& predictedDisplayTime, uint64_t& predictedDisplayPeriod)
    {
        XrFrameWaitInfo frameWaitInfo;
        frameWaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
        frameWaitInfo.next = nullptr;
        
        XrFrameState frameState;
        frameState.type = XR_TYPE_FRAME_STATE;
        frameState.next = nullptr;

        CHECK_XRCMD(xrWaitFrame(mXrSession, &frameWaitInfo, &frameState));
        shouldRender = frameState.shouldRender && mAppShouldRender;
        predictedDisplayTime = frameState.predictedDisplayTime;
        predictedDisplayPeriod = frameState.predictedDisplayPeriod;
    }

    void Session::syncFrameRender(VR::Frame& frame)
    {
        XrFrameBeginInfo frameBeginInfo;
        frameBeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
        frameBeginInfo.next = nullptr;
        CHECK_XRCMD(xrBeginFrame(mXrSession, &frameBeginInfo));
    }

    void Session::syncFrameEnd(VR::Frame& frame)
    {
        XrCompositionLayerProjection layer;
        layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        layer.next = nullptr;
        auto* xrLayerStack = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);
        
        std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews;
        compositionLayerProjectionViews[0].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        compositionLayerProjectionViews[0].next = nullptr;
        compositionLayerProjectionViews[1].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        compositionLayerProjectionViews[1].next = nullptr;
        
        std::array<XrCompositionLayerDepthInfoKHR, 2> compositionLayerDepth;
        compositionLayerDepth[0].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
        compositionLayerDepth[0].next = nullptr;
        compositionLayerDepth[1].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
        compositionLayerDepth[1].next = nullptr;
        
        XrFrameEndInfo frameEndInfo;
        frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
        frameEndInfo.next = nullptr;
        frameEndInfo.displayTime = frame.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if (frame.shouldRender && frame.layers.size() > 0)
        {
            // For now, hardcode assumption that it's a projection layer
            VR::ProjectionLayer* projectionLayer = static_cast<VR::ProjectionLayer*>(frame.layers[0].get());
            
            layer.space = mReferenceSpaceStage;
            layer.viewCount = 2;
            layer.views = compositionLayerProjectionViews.data();

            for (uint32_t i = 0; i < 2; i++)
            {
                auto& xrView = compositionLayerProjectionViews[i];
                auto& view = projectionLayer->views[i];
                xrView.fov = toXR(view.view.fov);
                xrView.pose = toXR(view.view.pose);
                xrView.subImage.imageArrayIndex = 0;
                xrView.subImage.imageRect.extent.width = view.subImage.width;
                xrView.subImage.imageRect.extent.height = view.subImage.height;
                xrView.subImage.imageRect.offset.x = view.subImage.x;
                xrView.subImage.imageRect.offset.y = view.subImage.y;
                xrView.subImage.swapchain = static_cast<XrSwapchain>(view.colorSwapchain->handle());
            }

            bool includeDepth = XR::Extensions::instance().extensionEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);

            if (includeDepth)
            {
                // TODO: Cache these values instead?
                GLfloat depthRange[2] = { 0.f, 1.f };
                glGetFloatv(GL_DEPTH_RANGE, depthRange);
                auto nearClip = Settings::Manager::getFloat("near clip", "Camera");
                auto farClip = Settings::Manager::getFloat("viewing distance", "Camera");
                for (uint32_t i = 0; i < 2; i++)
                {

                    auto& view = projectionLayer->views[i];
                    if (!view.depthSwapchain)
                        continue;

                    auto& xrDepth = compositionLayerDepth[i];
                    xrDepth.minDepth = depthRange[0];
                    xrDepth.maxDepth = depthRange[1];
                    xrDepth.nearZ = nearClip;
                    xrDepth.farZ = farClip;
                    xrDepth.subImage.imageArrayIndex = 0;
                    xrDepth.subImage.imageRect.extent.width = view.subImage.width;
                    xrDepth.subImage.imageRect.extent.height = view.subImage.height;
                    xrDepth.subImage.imageRect.offset.x = view.subImage.x;
                    xrDepth.subImage.imageRect.offset.y = view.subImage.y;
                    xrDepth.subImage.swapchain = static_cast<XrSwapchain>(view.depthSwapchain->handle());
                    
                    auto& xrView = compositionLayerProjectionViews[i];
                    xrView.next = &xrDepth;
                }
            }

            frameEndInfo.layerCount = 1;
            frameEndInfo.layers = &xrLayerStack;
        }
        else
        {
            frameEndInfo.layerCount = 0;
            frameEndInfo.layers = nullptr;
        }
        CHECK_XRCMD(xrEndFrame(mXrSession, &frameEndInfo));
    }

    void Session::handleEvents()
    {
        xrQueueEvents();

        while (auto* event = nextEvent())
        {
            if (!processEvent(event))
            {
                // Do not consider processing an event optional.
                // Retry once per frame until every event has been successfully processed
                return;
            }
            popEvent();
        }

        if (mXrSessionShouldStop)
        {
            if (checkStopCondition())
            {
                CHECK_XRCMD(xrEndSession(mXrSession));
                mXrSessionShouldStop = false;
            }
        }
    }

    const XrEventDataBaseHeader* Session::nextEvent()
    {
        if (mEventQueue.size() > 0)
            return reinterpret_cast<XrEventDataBaseHeader*> (&mEventQueue.front());
        return nullptr;
    }

    bool Session::processEvent(const XrEventDataBaseHeader* header)
    {
        Log(Debug::Verbose) << "OpenXR: Event received: " << to_string(header->type);
        switch (header->type)
        {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            const auto* stateChangeEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>(header);
            return handleSessionStateChanged(*stateChangeEvent);
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            // TODO:
            //MWVR::Environment::get().getInputManager()->notifyInteractionProfileChanged();
            break;
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        default:
        {
            Log(Debug::Verbose) << "OpenXR: Event ignored";
            break;
        }
        }
        return true;
    }

    bool
        Session::handleSessionStateChanged(
            const XrEventDataSessionStateChanged& stateChangedEvent)
    {
        Log(Debug::Verbose) << "XrEventDataSessionStateChanged: state " << to_string(mState) << "->" << to_string(stateChangedEvent.state);
        mState = stateChangedEvent.state;

        // Ref: https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#session-states
        switch (mState)
        {
        case XR_SESSION_STATE_IDLE:
        {
            mAppShouldSyncFrameLoop = false;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_READY:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;

            XrSessionBeginInfo beginInfo;
            beginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
            beginInfo.next = nullptr;
            beginInfo.primaryViewConfigurationType = mViewConfigType;
            CHECK_XRCMD(xrBeginSession(mXrSession, &beginInfo));

            break;
        }
        case XR_SESSION_STATE_STOPPING:
        {
            mAppShouldSyncFrameLoop = false;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = true;
            break;
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_VISIBLE:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = true;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_FOCUSED:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = true;
            mAppShouldReadInput = true;
            mXrSessionShouldStop = false;
            break;
        }
        default:
            Log(Debug::Warning) << "XrEventDataSessionStateChanged: Ignoring new state " << to_string(mState);
        }

        return true;
    }

    bool Session::checkStopCondition()
    {
        return mAcquiredResources == 0;
    }

    void Session::init()
    {
        createXrReferenceSpaces();
        createXrTracker();
    }

    void Session::cleanup()
    {
        destroyXrReferenceSpaces();
        destroyXrSession();
    }

    void Session::destroyXrReferenceSpaces()
    {
        for (auto space : { mReferenceSpaceLocal, mReferenceSpaceStage, mReferenceSpaceView })
        {
            if (space)
            {
                CHECK_XRCMD(xrDestroySpace(space));
            }
        }
    }

    void Session::destroyXrSession()
    {
        if (mXrSession)
            CHECK_XRCMD(xrDestroySession(mXrSession));
    }

    void Session::createXrTracker()
    {
        auto stageUserPath = VR::stringToVRPath("/stage/user");
        auto stageUserHeadPath = VR::stringToVRPath("/stage/user/head/input/pose");

        mTracker.reset(new Tracker(stageUserPath, mReferenceSpaceStage));
        mTracker->addTrackingSpace(stageUserHeadPath, mReferenceSpaceView);

        auto worldUserPath = VR::stringToVRPath("/world/user");
        auto worldUserHeadPath = VR::stringToVRPath("/world/user/head/input/pose");
        mTrackerToWorldBinding = std::make_unique<VR::StageToWorldBinding>(worldUserPath, stageUserHeadPath);
        mTrackerToWorldBinding->bindPaths(worldUserHeadPath, stageUserHeadPath);
    }

    VR::Swapchain* Session::createSwapchain(uint32_t width, uint32_t height, uint32_t samples, VR::SwapchainUse use, const std::string& name)
    {
        return Instance::instance().platform().createSwapchain(width, height, samples, use, name);
    }

    bool Session::xrNextEvent(XrEventDataBuffer& eventBuffer)
    {
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&eventBuffer);
        baseHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
        baseHeader->next = nullptr;
        const XrResult result = xrPollEvent(Instance::instance().xrInstance(), &eventBuffer);
        if (result == XR_SUCCESS)
        {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log(Debug::Warning) << "OpenXRManagerImpl: Lost " << eventsLost->lostEventCount << " events";
            }

            return baseHeader;
        }

        if (result != XR_EVENT_UNAVAILABLE)
            CHECK_XRRESULT(result, "xrPollEvent");
        return false;
    }

    void Session::popEvent()
    {
        if (mEventQueue.size() > 0)
            mEventQueue.pop();
    }

    void
        Session::xrQueueEvents()
    {
        XrEventDataBuffer eventBuffer;
        while (xrNextEvent(eventBuffer))
        {
            mEventQueue.push(eventBuffer);
        }
    }

    void Session::createXrReferenceSpaces()
    {
        XrReferenceSpaceCreateInfo createInfo;
        createInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        createInfo.next = nullptr;
        createInfo.poseInReferenceSpace.orientation.w = 1.f; // Identity pose

        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceView));
        Debugging::setName(mReferenceSpaceView, "OpenMW XR Reference Space View");

        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceStage));
        Debugging::setName(mReferenceSpaceStage, "OpenMW XR Reference Space Stage");

        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceLocal));
        Debugging::setName(mReferenceSpaceLocal, "OpenMW XR Reference Space Local");

    }

    void Session::logXrReferenceSpaces() {
        uint32_t spaceCount = 0;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mXrSession, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mXrSession, spaceCount, &spaceCount, spaces.data()));

        std::stringstream ss;
        ss << "Available reference spaces=" << spaceCount << std::endl;

        for (XrReferenceSpaceType space : spaces)
            ss << "  Name: " << to_string(space) << std::endl;
        Log(Debug::Verbose) << ss.str();
    }

    XrSpace Session::getReferenceSpace(VR::ReferenceSpace space)
    {
        switch (space)
        {
        case VR::ReferenceSpace::Stage:
            return mReferenceSpaceStage;
        case VR::ReferenceSpace::View:
            return mReferenceSpaceView;
        }
        return XR_NULL_HANDLE;
    }


    std::array<Misc::View, 2>
        Session::getPredictedViews(
            int64_t predictedDisplayTime,
            VR::ReferenceSpace space)
    {
        std::array<XrView, 2> xrViews;
        xrViews[0].type = XR_TYPE_VIEW;
        xrViews[0].next = nullptr;
        xrViews[1].type = XR_TYPE_VIEW;
        xrViews[1].next = nullptr;
        
        XrViewState viewState;
        viewState.type = XR_TYPE_VIEW_STATE;
        viewState.next = nullptr;
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo;
        viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
        viewLocateInfo.next = nullptr;
        viewLocateInfo.viewConfigurationType = mViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        switch (space)
        {
        case VR::ReferenceSpace::Stage:
            viewLocateInfo.space = mReferenceSpaceStage;
            break;
        case VR::ReferenceSpace::View:
            viewLocateInfo.space = mReferenceSpaceView;
            break;
        }
        CHECK_XRCMD(xrLocateViews(mXrSession, &viewLocateInfo, &viewState, viewCount, &viewCount, xrViews.data()));

        std::array<Misc::View, 2> vrViews{};
        for (auto side : { VR::Side_Left, VR::Side_Right })
        {
            vrViews[side].pose = fromXR(xrViews[side].pose);
            vrViews[side].fov = fromXR(xrViews[side].fov);
        }
        return vrViews;
    }

    std::array<VR::SwapchainConfig, 2> Session::getRecommendedSwapchainConfig() const
    {
        auto xrConfigs = Instance::instance().getRecommendedXrSwapchainConfig();
        std::array<VR::SwapchainConfig, 2> configs{};
        for (uint32_t i = 0; i < 2; i++)
        {
            configs[i].recommendedWidth = xrConfigs[i].recommendedImageRectWidth;
            configs[i].recommendedHeight = xrConfigs[i].recommendedImageRectHeight;
            configs[i].recommendedSamples = xrConfigs[i].recommendedSwapchainSampleCount;
            configs[i].maxWidth = xrConfigs[i].maxImageRectWidth;
            configs[i].maxHeight = xrConfigs[i].maxImageRectHeight;
            configs[i].maxSamples = xrConfigs[i].maxSwapchainSampleCount;
        }

        return configs;
    }
}
