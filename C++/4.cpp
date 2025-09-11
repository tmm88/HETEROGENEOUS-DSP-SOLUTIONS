#include <hls_math.h>
#include <hls_stream.h>
#include <ap_fixed.h>

// Use fixed-point for better FPGA performance
typedef ap_fixed<16,8> float_t;

struct vec2 { float_t x, y; };
struct vec2f { float x, y; }; // For interface compatibility
struct vec3 { float_t x, y, z; };
struct vec3f { float x, y, z; }; // For interface compatibility

// Vector operations
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec3 operator+(vec3 a, vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
vec3 operator-(vec3 a, vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
vec3 operator*(vec3 a, float_t b) { return {a.x * b, a.y * b, a.z * b}; }
vec3 operator*(float_t a, vec3 b) { return b * a; }

float_t length(vec2 v) { return hls::sqrt(v.x * v.x + v.y * v.y); }
float_t length(vec3 v) { return hls::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
vec3 normalize(vec3 v) { float_t len = length(v); return {v.x / len, v.y / len, v.z / len}; }
float_t dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
vec3 cross(vec3 a, vec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }

vec3 pow(vec3 v, vec3 p) { 
    return {hls::pow(v.x, p.x), hls::pow(v.y, p.y), hls::pow(v.z, p.z)};
}

vec3 mix(vec3 a, vec3 b, float_t t) { 
    return a * (1.0f - t) + b * t;
}

vec3 reflect(vec3 i, vec3 n) { 
    return i - n * 2.0f * dot(i, n);
}

const float_t PI = 3.14159265f;

vec2 path(float_t z) {
    return {0.5f * hls::sin(z), 0.5f * hls::sin(z * 0.7f)};
}

float_t map(vec3 p) {
    vec2 pp = {p.x, p.y};
    vec2 path_val = path(p.z);
    float_t dist = length(pp - path_val);
    return -dist + 1.2f + 0.3f * hls::sin(p.z * 0.4f);
}

vec3 normal(vec3 p) {
    float_t d = map(p);
    vec2 e = {0.01f, 0.0f};
    vec3 n = {
        d - map({p.x - e.x, p.y - e.y, p.z}),
        d - map({p.x, p.y - e.x, p.z}),
        d - map({p.x, p.y, p.z - e.x})
    };
    return normalize(n);
}

float_t sMax(float_t a, float_t b, float_t k) {
    return hls::log(hls::exp(k * a) + hls::exp(k * b)) / k;
}

float_t bumpFunction(vec3 p, float_t iTime) {
    vec2 c = path(p.z);
    float_t id = hls::floor(p.z * 4.0f - 0.25f);
    float_t angle = hls::atan2(p.y - c.y, p.x - c.x);
    float_t h = 0.5f + 0.5f * hls::sin(angle * 20.0f + 1.5f * (2.0f * hls::fmod(id, 2.0f) - 1.0f) + iTime * 5.0f);
    h = sMax(h, 0.5f + 0.5f * hls::sin(p.z * 8.0f * PI), 16.0f);
    h *= h;
    h *= h * h;
    return 1.0f - h;
}

vec3 bumpNormal(vec3 p, vec3 n, float_t bumpFactor, float_t iTime) {
    vec3 e = {0.01f, 0.0f, 0.0f};
    float_t f = bumpFunction(p, iTime);
    float_t fx1 = bumpFunction({p.x - e.x, p.y, p.z}, iTime);
    float_t fy1 = bumpFunction({p.x, p.y - e.x, p.z}, iTime);
    float_t fz1 = bumpFunction({p.x, p.y, p.z - e.x}, iTime);
    float_t fx2 = bumpFunction({p.x + e.x, p.y, p.z}, iTime);
    float_t fy2 = bumpFunction({p.x, p.y + e.x, p.z}, iTime);
    float_t fz2 = bumpFunction({p.x, p.y, p.z + e.x}, iTime);
    
    vec3 grad = {
        (fx1 - fx2) / (e.x * 2.0f),
        (fy1 - fy2) / (e.x * 2.0f),
        (fz1 - fz2) / (e.x * 2.0f)
    };
    
    grad = grad - n * dot(n, grad);
    return normalize(n + grad * bumpFactor);
}

void shader(
    hls::stream<vec3f>& output_stream,
    float_t iTime,
    int width,
    int height
) {
    #pragma HLS INTERFACE axis port=output_stream
    #pragma HLS INTERFACE s_axilite port=iTime
    #pragma HLS INTERFACE s_axilite port=width
    #pragma HLS INTERFACE s_axilite port=height
    #pragma HLS INTERFACE s_axilite port=return
    
    loop_y: for (int y = 0; y < height; y++) {
        loop_x: for (int x = 0; x < width; x++) {
            #pragma HLS PIPELINE II=1
            
            vec2 fragCoord = {float_t(x) + 0.5f, float_t(y) + 0.5f};
            vec2 uv = {
                (fragCoord.x * 2.0f - float_t(width)) / float_t(height),
                (fragCoord.y * 2.0f - float_t(height)) / float_t(height)
            };

            float_t vel = iTime * 1.5f;
            vec2 path_vel1 = path(vel - 1.0f);
            vec3 ro = {path_vel1.x, path_vel1.y, vel - 1.0f};
            vec2 path_vel = path(vel);
            vec3 ta = {path_vel.x, path_vel.y, vel};
            vec3 fwd = normalize(ta - ro);
            vec3 upv = {0.0f, 1.0f, 0.0f};
            vec3 right = cross(fwd, upv);
            upv = cross(right, fwd);
            float_t fl = 1.2f;
            vec3 rd = normalize(fwd + fl * (uv.x * right + uv.y * upv));

            float_t glow = 0.0f;
            vec3 glowCol = {9.0f, 7.0f, 4.0f};
            float_t t = 0.0f;
            vec3 col = {0.0f, 0.0f, 0.0f};

            loop_rm: for (int i = 0; i < 125; i++) {
                #pragma HLS UNROLL factor=4
                
                vec3 p = ro + rd * t;
                float_t d = map(p);
                glow += hls::exp(-d * 8.0f) * 0.005f;
                
                if (d < 0.01f) {
                    vec3 n = normal(p);
                    vec3 lightDir = normalize(vec3(1.0f, 1.0f, 1.0f)); // Fixed light direction
                    n = bumpNormal(p, n, 0.02f, iTime);
                    
                    vec2 c = path(p.z);
                    float_t id_val = hls::floor(p.z * 4.0f - 0.25f);
                    float_t angle_val = hls::atan2(p.y - c.y, p.x - c.x);
                    
                    vec3 tileCol = {0.7f, 0.7f, 0.7f};
                    tileCol = tileCol + vec3{0.4f * hls::sin(id_val), 0.4f * hls::cos(id_val), 0.0f};
                    tileCol = tileCol + 0.3f * hls::sin(id_val * 0.5f + angle_val * 6.0f - iTime * 4.0f);
                    
                    vec3 tileGray = {0.5f, 0.5f, 0.5f};
                    float_t height_val = bumpFunction(p, iTime);
                    vec3 baseCol = mix(tileGray, tileCol, height_val);
                    
                    float_t diffuseL = hls::max(dot(n, lightDir), 0.0f);
                    col = baseCol * diffuseL;
                    
                    vec3 h = normalize(lightDir - rd);
                    float_t specL = hls::pow(hls::max(dot(n, h), 0.0f), 64.0f);
                    col = col + specL * 0.3f;
                    
                    vec3 r = reflect(rd, n);
                    vec3 reflCol = {0.5f, 0.5f, 0.5f};
                    col = mix(col, reflCol, 0.3f);
                    
                    break;
                }
                t += d;
            }

            col = col + glowCol * glow;
            col = pow(col, vec3{2.2f, 2.2f, 2.2f});
            
            // Convert to float for output and clamp
            vec3f output_col = {
                hls::max(0.0f, hls::min(1.0f, float(col.x))),
                hls::max(0.0f, hls::min(1.0f, float(col.y))),
                hls::max(0.0f, hls::min(1.0f, float(col.z)))
            };
            
            output_stream.write(output_col);
        }
    }
}