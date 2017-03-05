#version 410

in highp vec2 aTexCoord0;
in highp vec4 aFaceColor;

out highp vec4 FragColor;

uniform vec2 uSourceTexDims;
uniform sampler2DRect uSourceTex;

void main() {
  // FragColor = texture(uSourceTex, aTexCoord0 * uSourceTexDims);
  FragColor = texture(uSourceTex, aTexCoord0);
  // FragColor = vec4(aTexCoord0, 0, 1);
  FragColor = aFaceColor;
  // FragColor = vec4(1, 0, 0, 1);
}
