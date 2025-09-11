#include <cmath>

struct vec2 { float x, y; };
struct vec3 { float x, y, z; };

vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec3 operator+(vec3 a, vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
vec3 operator-(vec3 a, vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
vec3 operator*(vec3 a, float b) { return {a.x * b, a.y * b, a.z * b}; }
vec3 operator*(float a, vec3 b) { return b * a; }
float length(vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
float length(vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
vec3 normalize(vec3 v) { float len = length(v); return {v.x / len, v.y / len, v.z / len}; }
float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
vec3 cross(vec3 a, vec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
vec3 pow(vec3 v, vec3 p) { return {std::pow(v.x, p.x), std::pow(v.y, p.y), std::pow(v.z, p.z)}; }
vec3 mix(vec3 a, vec3 b, float t) { return a * (1.0f - t) + b * t; }
vec3 reflect(vec3 i, vec3 n) { return i - n * 2.0f * dot(i, n); }

const float PI = 3.14159265f;

vec2 path(float z) {
    return {0.5f * std::sin(z), 0.5f * std::sin(z * 0.7f)};
}

float map(vec3 p) {
    vec2 pp = {p.x, p.y};
    return -length(pp - path(p.z)) + 1.2f + 0.3f * std::sin(p.z * 0.4f);
}

vec3 normal(vec3 p) {
    float d = map(p);
    vec2 e = {0.01f, 0.0f};
    vec3 n = {d - map({p.x - e.x, p.y - e.y, p.z - e.y}),
              d - map({p.x - e.y, p.y - e.x, p.z - e.y}),
              d - map({p.x - e.y, p.y - e.y, p.z - e.x})};
    return normalize(n);
}

float sMax(float a, float b, float k) {
    return std::log(std::exp(k * a) + std::exp(k * b)) / k;
}

float bumpFunction(vec3 p) {
    vec2 c = path(p.z);
    float id = std::floor(p.z * 4.0f - 0.25f);
    float h = 0.5f + 0.5f * std::sin(std::atan2(p.y - c.y, p.x - c.x) * 20.0f + 1.5f * (2.0f * std::fmod(id, 2.0f) - 1.0f) + iTime * 5.0f);
    h = sMax(h, 0.5f + 0.5f * std::sin(p.z * 8.0f * PI), 16.0f);
    h *= h;
    h *= h * h;
    return 1.0f - h;
}

vec3 bumpNormal(vec3 p, vec3 n, float bumpFactor) {
    vec3 e = {0.01f, 0.0f, 0.0f};
    float f = bumpFunction(p);
    float fx1 = bumpFunction({p.x - e.x, p.y, p.z});
    float fy1 = bumpFunction({p.x, p.y - e.x, p.z});
    float fz1 = bumpFunction({p.x, p.y, p.z - e.x});
    float fx2 = bumpFunction({p.x + e.x, p.y, p.z});
    float fy2 = bumpFunction({p.x, p.y + e.x, p.z});
    float fz2 = bumpFunction({p.x, p.y, p.z + e.x});
    vec3 grad = {(fx1 - fx2) / (e.x * 2.0f), (fy1 - fy2) / (e.x * 2.0f), (fz1 - fz2) / (e.x * 2.0f)};
    grad = grad - n * dot(n, grad);
    return normalize(n + grad * bumpFactor);
}

void shader(float *output, float iTime, int width, int height) {
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=iTime
#pragma HLS INTERFACE s_axilite port=width
#pragma HLS INTERFACE s_axilite port=height
#pragma HLS INTERFACE s_axilite port=return

    loop_y: for (int y = 0; y < height; y++) {
    loop_x: for (int x = 0; x < width; x++) {
#pragma HLS PIPELINE II=1
        vec2 fragCoord = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
        vec2 uv = {(fragCoord.x * 2.0f - static_cast<float>(width)) / static_cast<float>(height),
                   (fragCoord.y * 2.0f - static_cast<float>(height)) / static_cast<float>(height)};  // Adjusted for aspect

        float vel = iTime * 1.5f;
        vec2 path_vel1 = path(vel - 1.0f);
        vec3 ro = {path_vel1.x, path_vel1.y, vel - 1.0f};
        vec2 path_vel = path(vel);
        vec3 ta = {path_vel.x, path_vel.y, vel};
        vec3 fwd = normalize(ta - ro);
        vec3 upv = {0.0f, 1.0f, 0.0f};
        vec3 right = cross(fwd, upv);
        upv = cross(right, fwd);
        float fl = 1.2f;
        vec3 rd = normalize(fwd + fl * (uv.x * right + uv.y * upv));

        float glow = 0.0f;
        vec3 glowCol = {9.0f, 7.0f, 4.0f};
        float t = 0.0f;
        vec3 col = {0.0f, 0.0f, 0.0f};

        loop_rm: for (int i = 0; i < 125; i++) {
#pragma HLS UNROLL factor=4  // Partial unroll for performance
            vec3 p = ro + rd * t;
            float d = map(p);
            glow += std::exp(-d * 8.0f) * 0.005f;
            if (d < 0.01f) {
                vec3 n = normal(p);
                vec3 lightDir = n;
                n = bumpNormal(p, n, 0.02f);
                vec2 c = path(p.z);
                vec2 id = {std::floor(p.z * 4.0f - 0.25f), std::atan2(p.y - c.y, p.x - c.x)};
                vec3 tileCol = {0.7f, 0.7f, 0.7f};
                tileCol = tileCol + (vec3){0.4f * std::sin(id.x), 0.4f * std::cos(id.x), 0.0f};
                tileCol = tileCol + 0.3f * std::sin(id.x * 0.5f + id.y * 6.0f - iTime * 4.0f);
                vec3 tileGray = {0.5f, 0.5f, 0.5f};
                float height = bumpFunction(p);
                vec3 baseCol = mix(tileGray, tileCol, height);
                float diffuseL = std::fmax(dot(n, lightDir), 0.0f);
                col = col + baseCol * diffuseL;
                vec3 h = normalize(lightDir - rd);
                float specL = std::pow(std::fmax(dot(n, h), 0.0f), 64.0f);
                col = col + specL * 0.3f;
                vec3 r = reflect(rd, n);
                vec3 reflCol = {0.5f, 0.5f, 0.5f};  // Placeholder; replace with texture lookup if array passed
                col = mix(col, reflCol, 0.3f);
                break;
            }
            t += d;
        }

        col = col + glowCol * glow;
        col = pow(col, {2.2f, 2.2f, 2.2f});

        int idx = (y * width + x) * 4;
        output[idx + 0] = col.x;
        output[idx + 1] = col.y;
        output[idx + 2] = col.z;
        output[idx + 3] = 1.0f;
    }}
}