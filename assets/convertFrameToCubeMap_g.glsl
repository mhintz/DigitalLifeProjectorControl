#version 410

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VertexData {
  int gFaceIndex;
  vec2 gTexCoord0;
} gs_in[];

out vec2 aTexCoord0;

void main() {
  gl_Layer = gs_in[0].gFaceIndex;

  for (int i = 0; i < gl_in.length(); i++) {
    aTexCoord0 = gs_in[i].gTexCoord0;
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
  }

  EndPrimitive();
}
