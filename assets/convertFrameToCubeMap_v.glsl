#version 410

in vec4 ciPosition;
in vec2 ciTexCoord0;
in int faceIndex;

out VertexData {
  int gFaceIndex;
  vec2 gTexCoord0;
} vs_out;

uniform mat4 ciModelViewProjection;

void main() {
  vs_out.gFaceIndex = faceIndex;
  vs_out.gTexCoord0 = ciTexCoord0;
  gl_Position = ciModelViewProjection * ciPosition;
}
