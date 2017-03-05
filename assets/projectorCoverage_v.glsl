#version 330

in vec4 ciPosition;
in vec3 ciNormal;
in vec3 ciTexCoord1;

out vec4 aWorldSpacePosition;
out vec3 aWorldSpaceNormal;
out vec3 aCubeMapTexCoord;

uniform mat4 ciModelMatrix;
uniform mat4 ciModelViewProjection;

uniform mat3 ciModelMatrixInverseTranspose;

void main() {
  aWorldSpacePosition = ciModelMatrix * ciPosition;
  aWorldSpaceNormal = ciModelMatrixInverseTranspose * ciNormal;
  aCubeMapTexCoord = ciTexCoord1;
  gl_Position = ciModelViewProjection * ciPosition;
}
