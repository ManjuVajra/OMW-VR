#ifndef XR_TYPECONVERSIONS_H
#define XR_TYPECONVERSIONS_H

#include <openxr/openxr.h>
#include <components/stereo/stereomanager.hpp>
#include <osg/Vec3>
#include <osg/Quat>

namespace XR
{
    /// Conversion methods between openxr types to osg/mwvr types. Includes managing the differing conventions.
    Stereo::Pose          fromXR(XrPosef pose);
    Stereo::FieldOfView   fromXR(XrFovf fov);
    osg::Vec3           fromXR(XrVector3f);
    osg::Quat           fromXR(XrQuaternionf quat);
    XrPosef             toXR(Stereo::Pose pose);
    XrFovf              toXR(Stereo::FieldOfView fov);
    XrVector3f          toXR(osg::Vec3 v);
    XrQuaternionf       toXR(osg::Quat quat);
}

#endif
