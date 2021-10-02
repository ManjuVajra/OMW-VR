#include "vrutil.hpp"
#include "vranimation.hpp"
#include "vrenvironment.hpp"
#include "vrgui.hpp"
#include "vrpointer.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/vr/trackingmanager.hpp>

#include "osg/Transform"

namespace MWVR
{
    namespace Util
    {
        std::pair<MWWorld::Ptr, float> getPointerTarget()
        {
            auto pointer = Environment::get().getGUIManager()->getUserPointer();
            return std::pair<MWWorld::Ptr, float>(pointer->getPointerTarget().mHitObject, pointer->distanceToPointerTarget());
        }

        std::pair<MWWorld::Ptr, float> getTouchTarget()
        {
            MWRender::RayResult result;
            auto rightHandPath = VR::stringToVRPath("/world/user/hand/right/input/aim/pose");
            auto pose = VR::TrackingManager::instance().locate(rightHandPath, 0).pose;
            auto distance = getPoseTarget(result, pose, true);
            return std::pair<MWWorld::Ptr, float>(result.mHitObject, distance);
        }

        std::pair<MWWorld::Ptr, float> getWeaponTarget()
        {
            auto* anim = MWVR::Environment::get().getPlayerAnimation();

            MWRender::RayResult result;
            auto distance = getPoseTarget(result, getNodePose(anim->getNode("weapon bone")), false);
            return std::pair<MWWorld::Ptr, float>(result.mHitObject, distance);
        }

        float getPoseTarget(MWRender::RayResult& result, const Misc::Pose& pose, bool allowTelekinesis)
        {
            auto* wm = MWBase::Environment::get().getWindowManager();
            auto* world = MWBase::Environment::get().getWorld();

            if (wm->isGuiMode() && wm->isConsoleMode())
                return world->getTargetObject(result, pose.position, pose.orientation, world->getMaxActivationDistance() * 50, true);
            else
            {
                float activationDistance = 0.f;
                if (allowTelekinesis)
                    activationDistance = world->getActivationDistancePlusTelekinesis();
                else
                    activationDistance = world->getMaxActivationDistance();

                auto distance = world->getTargetObject(result, pose.position, pose.orientation, activationDistance, true);

                if (!result.mHitObject.isEmpty() && !result.mHitObject.getClass().allowTelekinesis(result.mHitObject)
                    && distance > activationDistance && !MWBase::Environment::get().getWindowManager()->isGuiMode())
                {
                    result.mHit = false;
                    result.mHitObject = nullptr;
                    distance = 0.f;
                };
                return distance;
            }
        }

        Misc::Pose getNodePose(const osg::Node* node)
        {
            osg::Matrix worldMatrix = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
            Misc::Pose pose;
            pose.position = worldMatrix.getTrans();
            pose.orientation = worldMatrix.getRotate();
            return pose;
        }
    }
}