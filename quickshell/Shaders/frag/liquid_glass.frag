#version 450

// Liquid-glass surface: refracts a (pre-frosted) backdrop at its rounded edges,
// lifts saturation, applies a tint, and draws a specular rim — the layered look
// of Apple's "liquid glass". Sample the backdrop (already blurred for frost) in
// this surface's local UV; the rounded-rect SDF drives both the edge lensing and
// the antialiased mask.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float widthPx;          // surface width in px
    float heightPx;         // surface height in px
    float cornerRadiusPx;   // corner radius in px
    float refractionPx;     // max edge displacement in px (lens strength)
    float bevelPx;          // width of the refractive edge band in px
    float saturation;       // backdrop saturation multiplier (1.0 = unchanged)
    float rimStrength;      // 0..1 specular rim intensity
    float shadowStrength;   // 0..1 inner-shadow depth on the light-opposite edge
    float lightDirX;        // light direction (points FROM the light), screen space
    float lightDirY;
    vec4 tintColor;         // rgb tint, a = tint strength (0 = no tint)
} ubuf;

layout(binding = 1) uniform sampler2D src;    // frosted backdrop, local UV

float sdRoundRect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - (b - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 sizePx = vec2(ubuf.widthPx, ubuf.heightPx);
    vec2 px = qt_TexCoord0 * sizePx;
    vec2 halfPx = sizePx * 0.5;
    float r = clamp(ubuf.cornerRadiusPx, 0.0, min(halfPx.x, halfPx.y));
    vec2 p = px - halfPx;

    float d = sdRoundRect(p, halfPx, r); // <0 inside, 0 at edge

    // Outward surface normal from the SDF gradient.
    vec2 n = normalize(vec2(
        sdRoundRect(p + vec2(1.0, 0.0), halfPx, r) - sdRoundRect(p - vec2(1.0, 0.0), halfPx, r),
        sdRoundRect(p + vec2(0.0, 1.0), halfPx, r) - sdRoundRect(p - vec2(0.0, 1.0), halfPx, r)
    ) + 1e-6);

    // Bevel profile: 0 in the flat interior, ramping to 1 at the very edge.
    // Squared so the bend stays subtle inward and curls sharply at the rim.
    float depth = -d; // positive inside
    float edge = 1.0 - smoothstep(0.0, ubuf.bevelPx, depth);
    float bend = edge * edge;

    // Refraction: pull the sample inward along the normal near the edge, so the
    // background compresses into the glass rim like a real lens. We sample the
    // FROSTED backdrop (not a sharp copy) so the blur carries continuously
    // through the refractive bevel — frost and refraction are one glass.
    vec2 uv = qt_TexCoord0 - n * bend * (ubuf.refractionPx / sizePx);
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    vec3 col = texture(src, uv).rgb;

    // Saturation lift on the refracted backdrop.
    float luma = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(luma), col, ubuf.saturation);

    // Tint.
    col = mix(col, ubuf.tintColor.rgb, ubuf.tintColor.a);

    vec2 lightDir = normalize(vec2(ubuf.lightDirX, ubuf.lightDirY) + 1e-6);
    float towardLight = dot(n, -lightDir); // +1 edge faces the light, -1 faces away

    // Specular sheen on the lit edge: a wide, gentle highlight that fades inward
    // across the bevel and BLENDS toward white (translucent) instead of adding pure
    // white — a subtle lighting effect that sits inside the glass rather than a hard
    // hairline painted on top. Mirrors how the inner shadow blends multiplicatively.
    float sheen = 1.0 - smoothstep(0.0, max(ubuf.bevelPx, 1.0), depth);
    float rim = sheen * clamp(towardLight, 0.0, 1.0) * ubuf.rimStrength;
    col = mix(col, vec3(1.0), rim);

    // Inner shadow: gently darken the bevel on the side facing away from the
    // light, giving the lozenge perceived depth instead of a flat cutout.
    float shade = edge * clamp(-towardLight, 0.0, 1.0) * ubuf.shadowStrength;
    col *= (1.0 - shade);

    // Antialiased rounded-rect mask.
    float aa = max(fwidth(d), 0.001);
    float mask = 1.0 - smoothstep(-aa, aa, d);

    float a = mask * ubuf.qt_Opacity;
    fragColor = vec4(col * a, a); // premultiplied
}
