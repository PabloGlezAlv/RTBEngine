// Shared DDGI utilities (octahedral encoding, probe indexing)
// No #version here — included into host shaders that already declare version/extensions.

const float DDGI_PI = 3.14159265;

vec2 OctEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z >= 0.0) {
        n.xy = n.xy * 0.5 + 0.5;
    } else {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy) * 0.5 + 0.5;
    }
    return n.xy;
}

vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}

ivec3 ProbeCoordFromIndex(int probeIndex, ivec3 gridDims) {
    int slice = gridDims.x * gridDims.y;
    int z = probeIndex / slice;
    int rem = probeIndex - z * slice;
    int y = rem / gridDims.x;
    int x = rem - y * gridDims.x;
    return ivec3(x, y, z);
}

vec3 ProbeWorldPos(ivec3 coord, vec3 origin, vec3 spacing) {
    return origin + (vec3(coord) + 0.5) * spacing;
}

ivec2 ProbeAtlasOrigin(ivec3 coord, int octSize, ivec3 gridDims) {
    return ivec2(coord.x * octSize, (coord.z * gridDims.y + coord.y) * octSize);
}
