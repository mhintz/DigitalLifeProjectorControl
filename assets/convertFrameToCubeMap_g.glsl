#version 410

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VertexData {
  int gFaceIndex;
  vec2 gTexCoord0;
} gs_in[];

out vec2 aTexCoord0;
out vec4 aFaceColor;

#define NUM_SIDES 6

vec4[NUM_SIDES] faceColors = vec4[](
  vec4(1, 0, 0, 1),
  vec4(0, 1, 1, 1),
  vec4(0, 1, 0, 1),
  vec4(1, 0, 1, 1),
  vec4(0, 0, 1, 1),
  vec4(1, 1, 0, 1)
);

void main() {
  // gl_Layer = gs_in[0].gFaceIndex;
  gl_Layer = 0;

  for (int i = 0; i < gl_in.length(); i++) {
    aTexCoord0 = gs_in[i].gTexCoord0;
    aFaceColor = faceColors[gs_in[0].gFaceIndex];
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
  }

  EndPrimitive();
}
