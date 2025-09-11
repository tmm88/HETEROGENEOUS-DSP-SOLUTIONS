#include <hls_math.h>
#include <hls_stream.h>
#include <ap_fixed.h>
#include <ap_int.h>

// Use 32-bit fixed point for better precision
typedef ap_fixed<32,16> float_t;

// Vector types
struct vec2 {
    float_t x, y;
    
    vec2() : x(0), y(0) {}
    vec2(float_t x, float_t y) : x(x), y(y) {}
};

struct vec3 {
    float_t x, y, z;
    
    vec3() : x(0), y(0), z(0) {}
    vec3(float_t x, float_t y, float_t z) : x(x), y(y), z(z) {}
};

struct vec4 {
    float_t x, y, z, w;
    
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4(float_t x, float_t y, float_t z, float_t w) : x(x), y(y), z(z), w(w) {}
};

// Math operations
vec3 operator+(const vec3& a, const vec3& b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3 operator-(const vec3& a, const vec3& b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3 operator*(const vec3& a, float_t b) {
    return vec3(a.x * b, a.y * b, a.z * b);
}

vec3 operator*(float_t b, const vec3& a) {
    return a * b;
}

float_t dot(const vec3& a, const vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 cross(const vec3& a, const vec3& b) {
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float_t length(const vec3& v) {
    return hls::sqrt(dot(v, v));
}

vec3 normalize(const vec3& v) {
    float_t len = length(v);
    if (len > 0.0001f) {
        return v * (1.0f / len);
    }
    return v;
}

vec3 refract(const vec3& I, const vec3& N, float_t eta) {
    float_t NdotI = dot(N, I);
    float_t k = 1.0f - eta * eta * (1.0f - NdotI * NdotI);
    if (k < 0.0f) {
        return vec3(0, 0, 0);
    }
    return eta * I - (eta * NdotI + hls::sqrt(k)) * N;
}

// Constants
const vec3 n4 = vec3(0.577f, 0.577f, 0.577f);
const vec3 n5 = vec3(-0.577f, 0.577f, 0.577f);
const vec3 n6 = vec3(0.577f, -0.577f, 0.577f);
const vec3 n7 = vec3(0.577f, 0.577f, -0.577f);
const vec3 n8 = vec3(0.000f, 0.357f, 0.934f);
const vec3 n9 = vec3(0.000f, -0.357f, 0.934f);
const vec3 n10 = vec3(0.934f, 0.000f, 0.357f);
const vec3 n11 = vec3(-0.934f, 0.000f, 0.357f);
const vec3 n12 = vec3(0.357f, 0.934f, 0.000f);
const vec3 n13 = vec3(-0.357f, 0.934f, 0.000f);

// Function declarations
vec2 mapRefract(vec3 p);
vec2 mapSolid(vec3 p, float_t iTime);
vec2 calcRayIntersection_3975550108(vec3 rayOrigin, vec3 rayDir, float_t maxd = 20.0f, float_t precis = 0.001f);
vec2 calcRayIntersection_766934105(vec3 rayOrigin, vec3 rayDir, float_t iTime, float_t maxd = 20.0f, float_t precis = 0.001f);
vec3 calcNormal_3606979787(vec3 pos, float_t eps = 0.002f);
vec3 calcNormal_1245821463(vec3 pos, float_t iTime, float_t eps = 0.002f);
float_t beckmannDistribution_2315452051(float_t x, float_t roughness);
float_t cookTorranceSpecular_1460171947(vec3 lightDirection, vec3 viewDirection, vec3 surfaceNormal, float_t roughness, float_t fresnel);
vec2 squareFrame_1062606552(vec2 screenSize, vec2 coord);
vec3 getRay_870892966(vec3 camMat[3], vec2 screenPos, float_t lensLength);
void calcLookAtMatrix_1535977339(vec3 origin, vec3 target, float_t roll, vec3 camMat[3]);
vec3 getRay_870892966_with_target(vec3 origin, vec3 target, vec2 screenPos, float_t lensLength);
void orbitCamera_421267681(float_t camAngle, float_t camHeight, float_t camDistance, vec2 screenResolution, vec2 coord, vec3& rayOrigin, vec3& rayDirection);
float_t sdBox_1117569599(vec3 position, vec3 dimensions);
float_t random_2281831123(vec2 co);
float_t fogFactorExp2_529295689(float_t dist, float_t density);
float_t intersectPlane(vec3 ro, vec3 rd, vec3 nor, float_t dist);
float_t icosahedral(vec3 p, float_t r);
vec2 rotate2D(vec2 p, float_t a);
vec3 palette(float_t t, vec3 a, vec3 b, vec3 c, vec3 d);
vec3 bg(vec3 ro, vec3 rd, float_t iTime);

// Main rendering function
void render_image(
    hls::stream<vec4>& output_stream,
    vec2 resolution = vec2(800, 600),
    float_t iTime = 0.0f,
    vec4 iMouse = vec4(0, 0, 0, 0)
) {
    #pragma HLS PIPELINE II=1
    #pragma HLS INTERFACE axis port=output_stream
    
    for (int y = 0; y < (int)resolution.y; y++) {
        for (int x = 0; x < (int)resolution.x; x++) {
            #pragma HLS PIPELINE II=1
            
            vec2 fragCoord = vec2(x, y);
            vec2 uv = squareFrame_1062606552(resolution, fragCoord);
            
            float_t dist = 4.5f;
            float_t rotation = (iMouse.z > 0) ? (6.0f * iMouse.x / resolution.x) : (iTime * 0.45f);
            float_t height = (iMouse.z > 0) ? (5.0f * (iMouse.y / resolution.y * 2.0f - 1.0f)) : -0.2f;
            
            vec3 ro, rd;
            orbitCamera_421267681(rotation, height, dist, resolution, fragCoord, ro, rd);
            
            vec3 color = bg(ro, rd, iTime);
            vec2 t = calcRayIntersection_3975550108(ro, rd);
            
            if (t.x > -0.5f) {
                vec3 pos = ro + rd * t.x;
                vec3 nor = calcNormal_3606979787(pos);
                
                vec3 ldir1 = normalize(vec3(0.8f, 1.0f, 0.0f));
                vec3 ldir2 = normalize(vec3(-0.4f, -1.3f, 0.0f));
                vec3 lcol1 = vec3(0.6f, 0.5f, 1.1f);
                vec3 lcol2 = vec3(1.4f, 0.9f, 0.8f) * 0.7f;
                
                vec3 ref = refract(rd, nor, 0.97f);
                vec2 u = calcRayIntersection_766934105(ro + ref * 0.1f, ref, iTime);
                
                if (u.x > -0.5f) {
                    vec3 pos2 = ro + ref * u.x;
                    vec3 nor2 = calcNormal_1245821463(pos2, iTime);
                    
                    float_t spec = cookTorranceSpecular_1460171947(ldir1, -ref, nor2, 0.6f, 0.95f) * 2.0f;
                    float_t diff1 = 0.05f + hls::max(0.0f, dot(ldir1, nor2));
                    float_t diff2 = hls::max(0.0f, dot(ldir2, nor2));
                    
                    color = vec3(spec) + (diff1 * lcol1 + diff2 * lcol2);
                } else {
                    color = bg(ro + ref * 0.1f, ref, iTime) * 1.1f;
                }
                
                color = color + color * cookTorranceSpecular_1460171947(ldir1, -rd, nor, 0.2f, 0.9f) * 2.0f;
                color = color + vec3(0.05f);
            }
            
            float_t vignette = 1.0f - hls::max(0.0f, dot(uv * 0.155f, uv));
            
            // Approximate smoothstep
            color.x = (color.x - 0.05f) * (1.0f / 0.945f);
            color.z = (color.z + 0.05f) * (1.0f / 1.0f);
            color.y = (color.y + 0.1f) * (1.0f / 1.05f);
            
            color.z *= vignette;
            
            float_t alpha = hls::max(0.5f, hls::min(1.0f, t.x));
            output_stream.write(vec4(color.x, color.y, color.z, alpha));
        }
    }
}

// Implementation of all the functions
vec2 mapRefract(vec3 p) {
    float_t d = icosahedral(p, 1.0f);
    return vec2(d, 0.0f);
}

vec2 mapSolid(vec3 p, float_t iTime) {
    // Rotations
    vec2 xz = rotate2D(vec2(p.x, p.z), iTime * 1.25f);
    p.x = xz.x; p.z = xz.y;
    
    vec2 yx = rotate2D(vec2(p.y, p.x), iTime * 1.85f);
    p.y = yx.x; p.x = yx.y;
    
    p.y += hls::sin(iTime) * 0.25f;
    p.x += hls::cos(iTime) * 0.25f;
    
    float_t d = length(p) - 0.25f;
    float_t id = 1.0f;
    
    float_t pulse = hls::pow(hls::sin(iTime * 2.0f) * 0.5f + 0.5f, 9.0f) * 2.0f;
    
    float_t box_d = sdBox_1117569599(p, vec3(0.175f, 0.175f, 0.175f));
    d = d * (1.0f - pulse) + box_d * pulse;
    
    return vec2(d, id);
}

vec2 calcRayIntersection_3975550108(vec3 rayOrigin, vec3 rayDir, float_t maxd, float_t precis) {
    float_t latest = precis * 2.0f;
    float_t dist = 0.0f;
    vec2 res = vec2(-1.0f, -1.0f);
    
    for (int i = 0; i < 50; i++) {
        #pragma HLS UNROLL factor=10
        if (latest < precis || dist > maxd) break;
        
        vec2 result = mapRefract(rayOrigin + rayDir * dist);
        latest = result.x;
        dist += latest;
    }
    
    if (dist < maxd) {
        res = vec2(dist, 0.0f);
    }
    return res;
}

vec2 calcRayIntersection_766934105(vec3 rayOrigin, vec3 rayDir, float_t iTime, float_t maxd, float_t precis) {
    float_t latest = precis * 2.0f;
    float_t dist = 0.0f;
    vec2 res = vec2(-1.0f, -1.0f);
    
    for (int i = 0; i < 60; i++) {
        #pragma HLS UNROLL factor=12
        if (latest < precis || dist > maxd) break;
        
        vec2 result = mapSolid(rayOrigin + rayDir * dist, iTime);
        latest = result.x;
        dist += latest;
    }
    
    if (dist < maxd) {
        res = vec2(dist, 1.0f);
    }
    return res;
}

vec3 calcNormal_3606979787(vec3 pos, float_t eps) {
    vec3 v1 = vec3(1.0f, -1.0f, -1.0f);
    vec3 v2 = vec3(-1.0f, -1.0f, 1.0f);
    vec3 v3 = vec3(-1.0f, 1.0f, -1.0f);
    vec3 v4 = vec3(1.0f, 1.0f, 1.0f);
    
    vec3 grad = v1 * mapRefract(pos + v1 * eps).x +
                v2 * mapRefract(pos + v2 * eps).x +
                v3 * mapRefract(pos + v3 * eps).x +
                v4 * mapRefract(pos + v4 * eps).x;
    
    return normalize(grad);
}

vec3 calcNormal_1245821463(vec3 pos, float_t iTime, float_t eps) {
    vec3 v1 = vec3(1.0f, -1.0f, -1.0f);
    vec3 v2 = vec3(-1.0f, -1.0f, 1.0f);
    vec3 v3 = vec3(-1.0f, 1.0f, -1.0f);
    vec3 v4 = vec3(1.0f, 1.0f, 1.0f);
    
    vec3 grad = v1 * mapSolid(pos + v1 * eps, iTime).x +
                v2 * mapSolid(pos + v2 * eps, iTime).x +
                v3 * mapSolid(pos + v3 * eps, iTime).x +
                v4 * mapSolid(pos + v4 * eps, iTime).x;
    
    return normalize(grad);
}

float_t beckmannDistribution_2315452051(float_t x, float_t roughness) {
    float_t NdotH = hls::max(x, 0.0001f);
    float_t cos2Alpha = NdotH * NdotH;
    float_t tan2Alpha = (cos2Alpha - 1.0f) / cos2Alpha;
    float_t roughness2 = roughness * roughness;
    float_t denom = 3.141592653589793f * roughness2 * cos2Alpha * cos2Alpha;
    return hls::exp(tan2Alpha / roughness2) / denom;
}

float_t cookTorranceSpecular_1460171947(vec3 lightDirection, vec3 viewDirection, vec3 surfaceNormal, float_t roughness, float_t fresnel) {
    float_t VdotN = hls::max(dot(viewDirection, surfaceNormal), 0.0f);
    float_t LdotN = hls::max(dot(lightDirection, surfaceNormal), 0.0f);
    
    vec3 H = normalize(lightDirection + viewDirection);
    
    float_t NdotH = hls::max(dot(surfaceNormal, H), 0.0f);
    float_t VdotH = hls::max(dot(viewDirection, H), 0.000001f);
    float_t LdotH = hls::max(dot(lightDirection, H), 0.000001f);
    
    float_t G1 = (2.0f * NdotH * VdotN) / VdotH;
    float_t G2 = (2.0f * NdotH * LdotN) / LdotH;
    float_t G = hls::min(1.0f, hls::min(G1, G2));
    
    float_t D = beckmannDistribution_2315452051(NdotH, roughness);
    float_t F = hls::pow(1.0f - VdotN, fresnel);
    
    return G * F * D / hls::max(3.141592653589793f * VdotN, 0.000001f);
}

vec2 squareFrame_1062606552(vec2 screenSize, vec2 coord) {
    float_t position = 2.0f * (coord.x / screenSize.x) - 1.0f;
    return vec2(position, position * (screenSize.x / screenSize.y));
}

void calcLookAtMatrix_1535977339(vec3 origin, vec3 target, float_t roll, vec3 camMat[3]) {
    vec3 rr = vec3(hls::sin(roll), hls::cos(roll), 0.0f);
    vec3 ww = normalize(target - origin);
    vec3 uu = normalize(cross(ww, rr));
    vec3 vv = normalize(cross(uu, ww));
    
    camMat[0] = uu;
    camMat[1] = vv;
    camMat[2] = ww;
}

vec3 getRay_870892966(vec3 camMat[3], vec2 screenPos, float_t lensLength) {
    vec3 ray = vec3(
        camMat[0].x * screenPos.x + camMat[1].x * screenPos.y + camMat[2].x * lensLength,
        camMat[0].y * screenPos.x + camMat[1].y * screenPos.y + camMat[2].y * lensLength,
        camMat[0].z * screenPos.x + camMat[1].z * screenPos.y + camMat[2].z * lensLength
    );
    return normalize(ray);
}

vec3 getRay_870892966_with_target(vec3 origin, vec3 target, vec2 screenPos, float_t lensLength) {
    vec3 camMat[3];
    calcLookAtMatrix_1535977339(origin, target, 0.0f, camMat);
    return getRay_870892966(camMat, screenPos, lensLength);
}

void orbitCamera_421267681(float_t camAngle, float_t camHeight, float_t camDistance, vec2 screenResolution, vec2 coord, vec3& rayOrigin, vec3& rayDirection) {
    vec2 screenPos = squareFrame_1062606552(screenResolution, coord);
    vec3 rayTarget = vec3(0.0f, 0.0f, 0.0f);
    
    rayOrigin = vec3(
        camDistance * hls::sin(camAngle),
        camHeight,
        camDistance * hls::cos(camAngle)
    );
    
    rayDirection = getRay_870892966_with_target(rayOrigin, rayTarget, screenPos, 2.0f);
}

float_t sdBox_1117569599(vec3 position, vec3 dimensions) {
    vec3 d = vec3(
        hls::abs(position.x) - dimensions.x,
        hls::abs(position.y) - dimensions.y,
        hls::abs(position.z) - dimensions.z
    );
    
    float_t inside = hls::min(hls::max(d.x, hls::max(d.y, d.z)), 0.0f);
    vec3 positive = vec3(
        hls::max(d.x, 0.0f),
        hls::max(d.y, 0.0f),
        hls::max(d.z, 0.0f)
    );
    return inside + length(positive);
}

float_t random_2281831123(vec2 co) {
    float_t a = 12.9898f;
    float_t b = 78.233f;
    float_t c = 43758.5453f;
    float_t dt = dot(co, vec2(a, b));
    float_t sn = hls::fmod(dt, 3.14f);
    return hls::fmod(hls::sin(sn) * c, 1.0f);
}

float_t fogFactorExp2_529295689(float_t dist, float_t density) {
    const float_t LOG2 = -1.442695f;
    float_t d = density * dist;
    return 1.0f - hls::max(0.0f, hls::min(1.0f, hls::exp2(d * d * LOG2)));
}

float_t intersectPlane(vec3 ro, vec3 rd, vec3 nor, float_t dist) {
    float_t denom = dot(rd, nor);
    return -(dot(ro, nor) + dist) / denom;
}

float_t icosahedral(vec3 p, float_t r) {
    float_t s = hls::abs(dot(p, n4));
    s = hls::max(s, hls::abs(dot(p, n5)));
    s = hls::max(s, hls::abs(dot(p, n6)));
    s = hls::max(s, hls::abs(dot(p, n7)));
    s = hls::max(s, hls::abs(dot(p, n8)));
    s = hls::max(s, hls::abs(dot(p, n9)));
    s = hls::max(s, hls::abs(dot(p, n10)));
    s = hls::max(s, hls::abs(dot(p, n11)));
    s = hls::max(s, hls::abs(dot(p, n12)));
    s = hls::max(s, hls::abs(dot(p, n13)));
    return s - r;
}

vec2 rotate2D(vec2 p, float_t a) {
    return vec2(
        p.x * hls::cos(a) - p.y * hls::sin(a),
        p.x * hls::sin(a) + p.y * hls::cos(a)
    );
}

vec3 palette(float_t t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return a + b * hls::cos(6.28318f * (c * t + d));
}

vec3 bg(vec3 ro, vec3 rd, float_t iTime) {
    vec2 rd_xz = vec2(rd.x, rd.z);
    float_t t_val = random_2281831123(rd_xz + vec2(hls::sin(iTime * 0.1f), 0.0f)) * 0.5f + 0.5f;
    t_val = t_val * 0.035f - rd.y * 0.5f + 0.35f;
    t_val = hls::max(-1.0f, hls::min(1.0f, t_val));
    
    vec3 col = vec3(0.1f) + palette(t_val,
        vec3(0.5f, 0.45f, 0.55f),
        vec3(0.5f, 0.5f, 0.5f),
        vec3(1.05f, 1.0f, 1.0f),
        vec3(0.275f, 0.2f, 0.19f)
    );
    
    float_t t = intersectPlane(ro, rd, vec3(0, 1, 0), 4.0f);
    
    if (t > 0.0f) {
        vec3 p = ro + rd * t;
        float_t g = hls::pow(1.0f - hls::abs(hls::sin(p.x) * hls::cos(p.z)), 0.25f);
        
        float_t fog = 1.0f - fogFactorExp2_529295689(t, 0.04f);
        col = col + fog * g * vec3(5.0f, 4.0f, 2.0f) * 0.075f;
    }
    
    return col;
}