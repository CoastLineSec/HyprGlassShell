#version 450

// Fluid Glass — Labs preview shader. A faithful Qt/ShaderEffect port of the
// WebGL lab's canonical fluid_glass.frag: a rounded-rect window into a backdrop
// with frost (mip-LOD), edge lensing, convex bevel, specular rim and a tint
// input. Drives the Fluid Glass Labs preview pane only.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 uResolution;        // preview size, px
    vec2 uRectCenter;        // glass centre, px
    vec2 uRectSize;          // glass size (w,h), px
    float uCornerRadius;     // px
    float uBlurPx;           // frost radius, px
    float uRefractPx;        // max edge displacement, px
    float uEdgeBandPx;       // width of refractive rim, px
    float uBevelPx;          // width of bevel shading, px
    float uHighlightStrength;// inner highlight 0..1
    float uShadowStrength;   // inner shadow 0..1
    float uLightAngleDeg;    // light direction, degrees
    float uSpecularStrength; // rim glint 0..1
    float uRimWidthPx;       // glint thinness, px
    float uRimWrap;          // 0 lit-edge .. 1 all-round
    vec4 uTint;              // rgb tint + strength(a)
} ubuf;

layout(binding = 1) uniform sampler2D uBackdrop;

float sdRoundRect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - (b - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// Mip-based frost: map blur radius (px) to a LOD and let the pre-filtered mip
// chain do the averaging, with a small cross of taps for a gaussian-ish falloff.
vec3 frost(vec2 uv, float radiusPx) {
    if (radiusPx < 0.5)
        return texture(uBackdrop, uv).rgb;
    float lod = log2(max(radiusPx, 1.0));
    vec2 toUv = 1.0 / ubuf.uResolution;
    float r = radiusPx * 0.5;
    vec3 c  = textureLod(uBackdrop, uv,                        lod).rgb * 0.40;
    c      += textureLod(uBackdrop, uv + vec2( r, 0.0) * toUv, lod).rgb * 0.15;
    c      += textureLod(uBackdrop, uv + vec2(-r, 0.0) * toUv, lod).rgb * 0.15;
    c      += textureLod(uBackdrop, uv + vec2(0.0,  r) * toUv, lod).rgb * 0.15;
    c      += textureLod(uBackdrop, uv + vec2(0.0, -r) * toUv, lod).rgb * 0.15;
    return c;
}

void main() {
    vec2  uv0    = qt_TexCoord0;
    vec2  fragPx = uv0 * ubuf.uResolution;
    vec2  p      = fragPx - ubuf.uRectCenter;
    vec2  b      = 0.5 * ubuf.uRectSize;
    float r      = min(ubuf.uCornerRadius, min(b.x, b.y));
    float d      = sdRoundRect(p, b, r);               // <0 inside

    vec3  bg     = texture(uBackdrop, uv0).rgb;
    float aa     = fwidth(d) + 1e-4;
    float inside = 1.0 - smoothstep(-aa, aa, d);

    if (inside <= 0.0) {
        fragColor = vec4(bg, 1.0);
        return;
    }

    // Edge lensing — outward normal from the SDF gradient.
    vec2  e = vec2(1.0, 0.0);
    vec2  n = normalize(vec2(
                  sdRoundRect(p + e.xy, b, r) - sdRoundRect(p - e.xy, b, r),
                  sdRoundRect(p + e.yx, b, r) - sdRoundRect(p - e.yx, b, r)
              ) + 1e-6);
    float depth = -d;
    float edge  = 1.0 - smoothstep(0.0, ubuf.uEdgeBandPx, depth);
    float bend  = edge * edge;
    vec2  uv    = uv0 - n * bend * (ubuf.uRefractPx / ubuf.uResolution);

    // Frost the refracted backdrop.
    vec3 glass = frost(uv, ubuf.uBlurPx);

    // Tint input (colour from the shell), blended before the lighting.
    glass = mix(glass, ubuf.uTint.rgb, ubuf.uTint.a);

    // Convex bevel — directional inner highlight + inner shadow. Qt is y-down, so
    // flip the light's vertical to read screen-natural (0 = right, 90 = up).
    float ang         = radians(ubuf.uLightAngleDeg);
    vec2  L           = vec2(cos(ang), -sin(ang));
    float towardLight = dot(n, L);
    float bevel       = 1.0 - smoothstep(0.0, ubuf.uBevelPx, depth);
    float hi          = bevel * clamp( towardLight, 0.0, 1.0) * ubuf.uHighlightStrength;
    float sh          = bevel * clamp(-towardLight, 0.0, 1.0) * ubuf.uShadowStrength;
    glass = mix(glass, vec3(1.0), hi);
    glass *= (1.0 - sh);

    // Specular rim — a thin bright glint at the perimeter, wrapping by uRimWrap.
    float rim     = 1.0 - smoothstep(0.0, ubuf.uRimWidthPx, depth);
    float litWrap = ubuf.uRimWrap + (1.0 - ubuf.uRimWrap) * clamp(towardLight, 0.0, 1.0);
    float spec    = rim * litWrap * ubuf.uSpecularStrength;
    glass = mix(glass, vec3(1.0), spec);

    fragColor = vec4(mix(bg, glass, inside), 1.0);
}
