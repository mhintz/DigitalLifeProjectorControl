#version 330

in vec4 aWorldSpacePosition;
in vec3 aWorldSpaceNormal;
in vec3 aCubeMapTexCoord;

out highp vec4 FragColor;

#define MAX_PROJECTORS 10

// For the time being, assume that the projector's actual
// projection frustum is irrelevant, and if the projector
// can see the pixel, it can shine on it
// (this is not a great assumption to make, and should be changed)
struct Projector {
  vec3 mPosition;
  float pad1;
  vec3 mTarget;
  float pad2;
  vec3 mColor;
  float pad3;
};

layout(std140) uniform uProjectors {
  Projector projectorList[MAX_PROJECTORS];
};

uniform int uNumProjectors;

void main() {
  vec3 normal = normalize(aWorldSpaceNormal);
  // vec4 baseColor = vec4(0, 0, 0, 1);
  vec4 baseColor = vec4(0, 0, 0, 0);
  float maxLightFactor = 0.0;

  for (int idx = 0; idx < uNumProjectors; idx++) {
    float lightFactor = clamp(dot(normal, normalize(projectorList[idx].mPosition - aWorldSpacePosition.xyz)), 0, 1);
    float alphaBase = baseColor.a;
    vec3 premultBase = alphaBase * baseColor.rgb;
    vec3 premultLight = lightFactor * projectorList[idx].mColor;
    // Alpha blending, "over" method
    baseColor = vec4(premultLight + premultBase * (1.0 - lightFactor), lightFactor + alphaBase * (1.0 - lightFactor));
  }

  baseColor.a = 1;
  FragColor = baseColor;

  // For testing custom cube map tex coordinates
  // FragColor = vec4(aCubeMapTexCoord, 1);
}
