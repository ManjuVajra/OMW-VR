#ifndef OPENXR_MANAGER_IMPL_HPP
#define OPENXR_MANAGER_IMPL_HPP

#include "openxrmanager.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

// The OpenXR SDK assumes we've included Windows.h
#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <vector>
#include <array>
#include <map>
#include <iostream>
#include <thread>
#include <chrono>

namespace MWVR
{

    // Error management macros and functions. Should be used on every openxr call.
#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);
    XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr);
    std::string XrResultString(XrResult res);

    /// Conversion methods from openxr types to osg/mwvr types. Includes managing the differing conventions.
    MWVR::Pose          fromXR(XrPosef pose);
    MWVR::FieldOfView   fromXR(XrFovf fov);
    osg::Vec3           fromXR(XrVector3f);
    osg::Quat           fromXR(XrQuaternionf quat);

    /// Conversion methods from osg/mwvr types to openxr types. Includes managing the differing conventions.
    XrPosef             toXR(MWVR::Pose pose);
    XrFovf              toXR(MWVR::FieldOfView fov);
    XrVector3f          toXR(osg::Vec3 v);
    XrQuaternionf       toXR(osg::Quat quat);

    XrCompositionLayerProjectionView toXR(MWVR::CompositionLayerProjectionView layer);

    /// \brief Implementation of OpenXRManager
    struct OpenXRManagerImpl
    {
        OpenXRManagerImpl(void);
        ~OpenXRManagerImpl(void);

        void waitFrame();
        void beginFrame();
        void endFrame(int64_t displayTime, int layerCount, const std::array<CompositionLayerProjectionView, 2>& layerStack);
        bool xrSessionRunning() const { return mSessionRunning; }
        std::array<View, 2> getPredictedViews(int64_t predictedDisplayTime, ReferenceSpace space);
        MWVR::Pose getPredictedHeadPose(int64_t predictedDisplayTime, ReferenceSpace space);
        void handleEvents();
        void enablePredictions();
        void disablePredictions();
        long long getLastPredictedDisplayTime();
        long long getLastPredictedDisplayPeriod();
        std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const;
        XrSpace getReferenceSpace(ReferenceSpace space);
        XrSession xrSession() const { return mSession; };
        XrInstance xrInstance() const { return mInstance; };

    protected:
        void LogLayersAndExtensions();
        void LogInstanceInfo();
        void LogReferenceSpaces();
        const XrEventDataBaseHeader* nextEvent();
        void HandleSessionStateChanged(const XrEventDataSessionStateChanged& stateChangedEvent);

    private:
        bool initialized = false;
        bool mPredictionsEnabled = false;
        XrInstance mInstance = XR_NULL_HANDLE;
        XrSession mSession = XR_NULL_HANDLE;
        XrSpace mSpace = XR_NULL_HANDLE;
        XrFormFactor mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode mEnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrGraphicsBindingOpenGLWin32KHR mGraphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
        XrSystemProperties mSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        std::array<XrViewConfigurationView, 2> mConfigViews{ { {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} } };
        XrSpace mReferenceSpaceView = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceStage = XR_NULL_HANDLE;
        XrEventDataBuffer mEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
        XrFrameState mFrameState{};
        XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
        bool mSessionRunning = false;
        std::mutex mFrameStateMutex{};
        std::mutex mEventMutex{};
    };
}

#endif 