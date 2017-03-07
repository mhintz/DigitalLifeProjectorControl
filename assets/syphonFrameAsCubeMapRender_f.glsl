#version 410

#include "alphaBlend_m.glsl"

in vec4 aWorldSpacePosition;
in vec3 aWorldSpaceNormal;
in highp vec3 aCubeMapTexCoord;

out vec4 FragColor;

uniform samplerCube uCubeMapTex;

uniform vec3 uProjectorPos;

#ifdef EXTERNAL_VIEW
  struct Projector {
    vec3 mPosition;
    float pad1;
    vec3 mTarget;
    float pad2;
    vec3 mColor;
    float pad3;
  };

  #define MAX_PROJECTORS 10

  layout(std140) uniform uProjectors {
    Projector projectorList[MAX_PROJECTORS];
  };

  uniform int uNumProjectors;
#endif

vec4 getProjectorValue(in vec3 toProjector, in vec3 normal, in vec4 color) {
  float lightFactor = clamp(dot(normal, toProjector), 0, 1);
  return vec4(color.rgb, lightFactor);
}

void main() {
  vec3 normal = normalize(aWorldSpaceNormal);

  #ifdef EXTERNAL_VIEW
    vec4 texColor = texture(uCubeMapTex, aCubeMapTexCoord);

    vec4 baseColor = vec4(0, 0, 0, 0);

    for (int idx = 0; idx < uNumProjectors; idx++) {
      vec3 toProjector = normalize(projectorList[idx].mPosition - aWorldSpacePosition.xyz);
      vec4 projLight = getProjectorValue(toProjector, normal, texColor);
      baseColor = alphaBlend(baseColor, projLight);
    }

    FragColor = vec4(baseColor.rgb, 1.0);
  #else
    FragColor = getProjectorValue(normalize(uProjectorPos - aWorldSpacePosition.xyz), normal, texture(uCubeMapTex, aCubeMapTexCoord));
  #endif
}
