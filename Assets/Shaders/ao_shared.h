#ifndef RAY_TRACING_AO_SHARED_H
#define RAY_TRACING_AO_SHARED_H

#include "../../Includes/glsl_cpp_common.h"

// Ray payload for both primary rays and ambient occlusion.
// Note that although this payload is flexible, we only need the isVisible
// field for ambient occlusion.
// In a more complex sample, we could have two miss shaders, for instance,
// one of which would use a payload of type {worldPosition, worldNormal} for
// primary rays, and another which would use a payload of type {isVisible} for
// ambient occlusion. This would involve an SBT with one ray generation shader,
// two miss shaders, and one closest hit shader.
struct RayPayload
{
  float hitSky;         // 0: occluded, 1: visible
  vec3  worldPosition;  // Position of intersection in world-space
  vec3  worldNormal;    // Normal at intersection in world-space
  //float isGround;
};

#endif  // RAY_TRACING_AO_SHARED_H