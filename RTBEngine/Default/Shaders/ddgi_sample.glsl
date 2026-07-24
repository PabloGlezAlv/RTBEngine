// DDGI sampling for forward lit shader
// Requires: DDGIUBO at binding 7, irradiance/distance atlases at 8/9

layout(std140, binding = 7) uniform DDGIData {
    vec3 ddgiOrigin;
    float _ddgiPad0;
    vec3 ddgiSpacing;
    float _ddgiPad1;
    ivec3 ddgiGridDims;
    int ddgiEnabled;
    float ddgiHysteresis;
    float ddgiNormalBias;
    float ddgiViewBias;
    float ddgiProbeRadius;
    vec3 ddgiAmbientColor;
    float ddgiAmbientIntensity;
    float ddgiIntensity;
};

#if defined(VULKAN)
layout(set = 0, binding = 8) uniform sampler2D uDDGIIrradiance;
layout(set = 0, binding = 9) uniform sampler2D uDDGIDistance;
#else
uniform sampler2D uDDGIIrradiance;
uniform sampler2D uDDGIDistance;
uniform bool uDDGIEnabled;
#endif

vec2 DDGIOctEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z >= 0.0) n.xy = n.xy * 0.5 + 0.5;
    else n.xy = (1.0 - abs(n.yx)) * sign(n.xy) * 0.5 + 0.5;
    return n.xy;
}

vec3 DDGIOctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

vec3 SampleDDGI(vec3 worldPos, vec3 normal) {
#if defined(VULKAN)
    if (ddgiEnabled == 0) return ddgiAmbientColor * ddgiAmbientIntensity;
#else
    if (!uDDGIEnabled) return vec3(0.1);
#endif

    vec3 rel = (worldPos - ddgiOrigin) / ddgiSpacing;
    vec3 gridPos = rel - 0.5;
    ivec3 base = ivec3(floor(gridPos));
    vec3 frac = fract(gridPos);

    vec3 irradiance = vec3(0.0);
    float totalWeight = 0.0;
    const int octSize = 16;

    for (int z = 0; z <= 1; z++) {
        for (int y = 0; y <= 1; y++) {
            for (int x = 0; x <= 1; x++) {
                ivec3 coord = base + ivec3(x, y, z);
                if (coord.x < 0 || coord.y < 0 || coord.z < 0) continue;
                if (coord.x >= ddgiGridDims.x || coord.y >= ddgiGridDims.y || coord.z >= ddgiGridDims.z) continue;

                vec3 probePos = ddgiOrigin + (vec3(coord) + 0.5) * ddgiSpacing;
                vec3 toProbe = normalize(probePos - worldPos);
                float normalWeight = max(dot(normal, toProbe), 0.0);
                float viewWeight = 1.0;
                vec3 triWeight = vec3(x == 0 ? 1.0 - frac.x : frac.x,
                                      y == 0 ? 1.0 - frac.y : frac.y,
                                      z == 0 ? 1.0 - frac.z : frac.z);
                float weight = normalWeight * viewWeight * triWeight.x * triWeight.y * triWeight.z;
                if (weight < 0.0001) continue;

                vec2 octUv = DDGIOctEncode(normal);
                vec2 atlasOrigin = vec2(float(coord.x * octSize), float((coord.z * ddgiGridDims.y + coord.y) * octSize));
                vec2 atlasUv = (atlasOrigin + octUv * float(octSize - 1) + 0.5)
                    / vec2(float(ddgiGridDims.x * octSize), float(ddgiGridDims.z * ddgiGridDims.y * octSize));
                irradiance += texture(uDDGIIrradiance, atlasUv).rgb * weight;
                totalWeight += weight;
            }
        }
    }

    if (totalWeight > 0.0001) {
        return irradiance / totalWeight;
    }
    return vec3(0.1);
}
