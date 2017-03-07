#version 410

in vec4 aWorldSpacePosition;
in vec3 aWorldSpaceNormal;
in highp vec3 aCubeMapTexCoord;

out vec4 FragColor;

uniform samplerCube uCubeMapTex;
uniform vec3 uProjectorPos;

void main() {
  // vec3 normal = normalize(aWorldSpaceNormal);
  // float lightFactor = clamp(dot(normal, normalize(uProjectorPos - aWorldSpacePosition.xyz)), 0, 1);
  // FragColor = vec4(lightFactor * lightFactor * texture(uCubeMapTex, aCubeMapTexCoord).rgb, 1);
  FragColor = texture(uCubeMapTex, aCubeMapTexCoord);
}
