// KWin's OffscreenEffect does not wrap custom shaders with the stock fragment
// color-management pipeline. Include and call it explicitly so output brightness,
// dimming, HDR/ICC transforms, and saturation match normal windows.
#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;

uniform float opaqueThreshold;      // luminance at/above this → fully opaque
uniform float transparentThreshold; // luminance at/below this → fully transparent
uniform float backgroundOpacity;    // background layer behind window pixels
uniform vec3 backgroundColor;

varying vec2 texcoord0;

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);

    // Un-premultiply alpha
    vec3 rgb = tex.rgb / max(tex.a, 0.001);

    // Brightness = max channel (HSV Value), so vivid saturated colors (red, blue)
    // stay opaque instead of fading like they do under Rec. 709 luminance.
    float luminance = max(rgb.r, max(rgb.g, rgb.b));

    float range = max(opaqueThreshold - transparentThreshold, 0.001);
    float windowOpacity = clamp((luminance - transparentThreshold) / range, 0.0, 1.0);

    // KWin composites OffscreenEffect shader output as premultiplied alpha.
    // Compose the faded window pixel normally over the optional background
    // layer, then let KWin blend that premultiplied result over the desktop.
    float fgAlpha = tex.a * windowOpacity;
    vec3 fgRgb = rgb * fgAlpha;

    float bgAlpha = backgroundOpacity * tex.a;
    vec3 bgRgb = backgroundColor * bgAlpha;

    vec3 outRgb = fgRgb + bgRgb * (1.0 - fgAlpha);
    float outAlpha = fgAlpha + bgAlpha * (1.0 - fgAlpha);

    vec4 outColor = vec4(outRgb, outAlpha);
    outColor = sourceEncodingToNitsInDestinationColorspace(outColor);
    outColor = adjustSaturation(outColor);
    outColor *= modulation;

    gl_FragColor = nitsToDestinationEncoding(outColor);
}
