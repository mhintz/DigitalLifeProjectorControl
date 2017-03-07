// Alpha blending, "over" method
vec4 alphaBlend(in vec4 under, in vec4 over) {
  vec3 pmUnder = under.rgb * under.a;
  vec3 pmOver = over.rgb * over.a;
  return vec4(pmOver + pmUnder * (1.0 - over.a), over.a + under.a * (1.0 - over.a));
}
