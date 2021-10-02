#include "vrcamera.hpp"
#include "vrgui.hpp"
#include "vrinputmanager.hpp"
#include "vranimation.hpp"
#include "vrenvironment.hpp"

#include <components/sceneutil/visitor.hpp>

#include <components/misc/constants.hpp>
#include <components/vr/trackingmanager.hpp>
#include <components/vr/session.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/player.hpp"
#include "../mwworld/class.hpp"

#include "../mwmechanics/movement.hpp"

#include <osg/Quat>

namespace MWVR
{
    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    static inline void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
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

    VRCamera::VRCamera(osg::Camera* camera)
        : MWRender::Camera(camera)
    {
        mVanityAllowed = false;
        mFirstPersonView = true;
    }

    VRCamera::~VRCamera()
    {
    }

    void VRCamera::setShouldTrackPlayerCharacter(bool track)
    {
        mShouldTrackPlayerCharacter = track;
    }

    void VRCamera::recenter()
    {
        if (!mHasTrackingData)
            return;

        // Move position of head to center of character 
        // Z should not be affected

        auto path = VR::stringToVRPath("/world/user");
        auto* stageToWorldBinding = static_cast<VR::StageToWorldBinding*>(VR::TrackingManager::instance().getTrackingSource(path));

        stageToWorldBinding->setSeatedPlay(VR::Session::instance().seatedPlay());
        stageToWorldBinding->setEyeLevel(VR::Session::instance().eyeLevel() * Constants::UnitsPerMeter);
        stageToWorldBinding->recenter(mShouldResetZ);

        mShouldRecenter = false;
        Log(Debug::Verbose) << "Recentered";
    }

    void VRCamera::applyTracking()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        auto& player = world->getPlayer();
        auto playerPtr = player.getPlayer();

        float yaw = 0.f;
        float pitch = 0.f;
        float roll = 0.f;
        getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);

        if (!player.isDisabled() && mTrackingNode)
        {
            world->rotateObject(playerPtr, osg::Vec3f(pitch, 0.f, yaw), MWBase::RotationFlag_none);
        }
    }

    void VRCamera::onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime)
    {
        auto path = VR::stringToVRPath("/world/user/head/input/pose");
        auto tp = manager.locate(path, predictedDisplayTime);

        if (!!tp.status)
        {
            mHeadPose = tp.pose;
            mHasTrackingData = true;
        }

        if (mShouldRecenter)
        {
            recenter();
            Camera::updateCamera(mCamera);
            auto* vrGuiManager = MWVR::Environment::get().getGUIManager();
            vrGuiManager->updateTracking();
        }
        else
        {
            if (mShouldTrackPlayerCharacter && !MWBase::Environment::get().getWindowManager()->isGuiMode())
                applyTracking();

            Camera::updateCamera(mCamera);
        }
    }

    void VRCamera::updateCamera(osg::Camera* cam)
    {
        // The regular update call should do nothing while tracking the player
    }

    void VRCamera::updateCamera()
    {
        Camera::updateCamera();
    }

    void VRCamera::reset()
    {
        Camera::reset();
    }

    void VRCamera::rotateCamera(float pitch, float roll, float yaw, bool adjust)
    {
        if (adjust)
        {
            pitch += getPitch();
            yaw += getYaw();
        }
        setYaw(yaw);
        setPitch(pitch);
    }

    void VRCamera::toggleViewMode(bool force)
    {
        mFirstPersonView = true;
    }
    bool VRCamera::toggleVanityMode(bool enable)
    {
        // Vanity mode makes no sense in VR
        return Camera::toggleVanityMode(false);
    }
    void VRCamera::allowVanityMode(bool allow)
    {
        // Vanity mode makes no sense in VR
        mVanityAllowed = false;
    }
    void VRCamera::getPosition(osg::Vec3d& focal, osg::Vec3d& camera) const
    {
        camera = focal = mHeadPose.position;
    }
    void VRCamera::getOrientation(osg::Quat& orientation) const
    {
        orientation = mHeadPose.orientation;
    }

    void VRCamera::processViewChange()
    {
        SceneUtil::FindByNameVisitor findRootVisitor("Player Root", osg::NodeVisitor::TRAVERSE_PARENTS);
        mAnimation->getObjectRoot()->accept(findRootVisitor);
        mTrackingNode = findRootVisitor.mFoundNode;

        if (!mTrackingNode)
            throw std::logic_error("Unable to find tracking node for VR camera");
        mHeightScale = 1.f;
    }

    void VRCamera::instantTransition()
    {
        Camera::instantTransition();

        // When the cell changes, openmw rotates the character.
        // To make sure the player faces the same direction regardless of current orientation,
        // compute the offset from character orientation to player orientation and reset yaw offset to this.
        float yaw = 0.f;
        float pitch = 0.f;
        float roll = 0.f;
        getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);
        yaw = - mYaw - yaw;
        auto path = VR::stringToVRPath("/world/user");
        auto* stageToWorldBinding = static_cast<VR::StageToWorldBinding*>(VR::TrackingManager::instance().getTrackingSource(path));
        stageToWorldBinding->setWorldOrientation(yaw, true);
    }

    void VRCamera::requestRecenter(bool resetZ)
    {
        mShouldRecenter = true;

        // Use OR so we don't negate a pending requests.
        mShouldResetZ |= resetZ;
    }
}