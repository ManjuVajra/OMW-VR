#version @GLSLVersion

#include "multiview_vertex.glsl"

#include "skypasses.glsl"

uniform int pass;

varying vec4 passColor;
varying vec2 diffuseMapUV;

mat4 selectModelViewMatrix()
{
#if @useOVR_multiview
    mat4 viewOffsetMatrix = viewMatrixMultiView[gl_ViewID_OVR];
    // Sky geometries aren't actually all that distant. So delete view translation to keep them looking distant.
    viewOffsetMatrix[3][0] = 0;
    viewOffsetMatrix[3][1] = 0;
    viewOffsetMatrix[3][2] = 0;
    return viewOffsetMatrix * gl_ModelViewMatrix;
#else
    return gl_ModelViewMatrix;
#endif
}

void main()
{
    gl_Position = mw_stereoAwareProjectionMatrix() * (selectModelViewMatrix() * gl_Vertex);
    passColor = gl_Color;

    if (pass == PASS_CLOUDS)
        diffuseMapUV = (gl_TextureMatrix[0] * gl_MultiTexCoord0).xy;
    else
        diffuseMapUV = gl_MultiTexCoord0.xy;
}
