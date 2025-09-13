#include <hls_math.h>
#include <hls_video.h>
#include <ap_fixed.h>

// Use fixed-point arithmetic for FPGA optimization
typedef ap_fixed<16,8> fixed_t;
typedef ap_fixed<32,16> fixed32_t;

// Structure for pixel data
struct pixel_t {
    fixed_t r, g, b, a;
};

// Smoothstep function implementation for HLS
fixed_t smoothstep(fixed_t edge0, fixed_t edge1, fixed_t x) {
    x = hls::clamp((x - edge0) / (edge1 - edge0), fixed_t(0), fixed_t(1));
    return x * x * (fixed_t(3) - fixed_t(2) * x);
}

// Vector operations for fixed-point types
struct vec2_t { fixed_t x, y; };
struct vec3_t { fixed_t x, y, z; };

vec3_t normalize(vec3_t v) {
    fixed32_t len_sq = v.x*v.x + v.y*v.y + v.z*v.z;
    fixed_t len = hls::sqrt(fixed_t(len_sq));
    return {v.x/len, v.y/len, v.z/len};
}

vec3_t fract(vec3_t v) {
    return {v.x - hls::floor(v.x), 
            v.y - hls::floor(v.y), 
            v.z - hls::floor(v.z)};
}

vec3_t abs(vec3_t v) {
    return {hls::abs(v.x), hls::abs(v.y), hls::abs(v.z)};
}

fixed_t length(vec2_t v) {
    return hls::sqrt(v.x*v.x + v.y*v.y);
}

fixed_t length(vec3_t v) {
    return hls::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

// Main hyperspatial construct function
void hyperspatial_construct(
    hls::stream<ap_axiu<24,1,1,1>> &src_axi,
    hls::stream<ap_axiu<24,1,1,1>> &dst_axi,
    fixed_t iTime,
    int width,
    int height
) {
    #pragma HLS INTERFACE axis port=src_axi
    #pragma HLS INTERFACE axis port=dst_axi
    #pragma HLS INTERFACE s_axilite port=iTime bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=width bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=height bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL

    fixed_t iResolution_x = fixed_t(width);
    fixed_t iResolution_y = fixed_t(height);
    
    ap_axiu<24,1,1,1> pixel;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS LOOP_FLATTEN off
            
            src_axi.read(pixel);
            
            vec2_t fragCoord = {fixed_t(x), fixed_t(y)};
            vec2_t uv = {fragCoord.x / iResolution_x, fragCoord.y / iResolution_y};
            uv.x -= fixed_t(0.5);
            uv.y -= fixed_t(0.5);
            uv.x *= iResolution_x / iResolution_y;
            
            fixed_t time = iTime;
            
            // Create perspective effect
            fixed_t zoom = fixed_t(1.0) + hls::sin(time * fixed_t(0.5)) * fixed_t(0.2);
            vec3_t rayDir = normalize({uv.x, uv.y, fixed_t(1.5)});
            vec3_t rayOrigin = {fixed_t(0.0), fixed_t(0.0), -time * fixed_t(0.5)};
            
            vec3_t col = {fixed_t(0.0), fixed_t(0.0), fixed_t(0.0)};
            
            // Main rendering loop - unrolled for better performance
            for (int i = 0; i < 5; i++) {
                #pragma HLS UNROLL factor=2
                #pragma HLS PIPELINE II=1
                
                fixed_t iter = fixed_t(i);
                fixed_t z = hls::fmod(rayOrigin.z + iter * fixed_t(0.3), fixed_t(1.0));
                vec3_t p = {rayOrigin.x + rayDir.x * z,
                           rayOrigin.y + rayDir.y * z,
                           rayOrigin.z + rayDir.z * z};
                
                // Create 3D grid
                vec3_t grid = abs(fract({p.x * fixed_t(20.0) * zoom,
                                       p.y * fixed_t(20.0) * zoom,
                                       p.z * fixed_t(20.0) * zoom}) - fixed_t(0.5));
                
                // Grid lines with perspective
                fixed_t gridLines = smoothstep(fixed_t(0.08), fixed_t(0.06), length({grid.x, grid.y}));
                gridLines *= smoothstep(fixed_t(0.1), fixed_t(0.0), grid.z);
                
                vec3_t gridColor = {fixed_t(0.3), fixed_t(0.6), fixed_t(1.0)};
                col.x += gridLines * gridColor.x * (fixed_t(1.0) - z);
                col.y += gridLines * gridColor.y * (fixed_t(1.0) - z);
                col.z += gridLines * gridColor.z * (fixed_t(1.0) - z);
                
                // Complex 3D nodes
                vec3_t nodePos = {hls::floor(p.x * fixed_t(20.0) * zoom),
                                 hls::floor(p.y * fixed_t(20.0) * zoom),
                                 hls::floor(p.z * fixed_t(20.0) * zoom)};
                
                fixed_t node = hls::sin(nodePos.x * fixed_t(1.2) + 
                                      nodePos.y * fixed_t(1.8) + 
                                      nodePos.z * fixed_t(2.1) + 
                                      time * fixed_t(3.0));
                node = fixed_t(0.5) + fixed_t(0.5) * node;
                
                fixed_t nodeSize = fixed_t(0.1) + 
                                 fixed_t(0.05) * hls::sin(time * fixed_t(4.0) + 
                                                         (nodePos.x * fixed_t(1.0) + 
                                                          nodePos.y * fixed_t(2.0) + 
                                                          nodePos.z * fixed_t(3.0)));
                
                vec3_t fractP = fract({p.x * fixed_t(20.0) * zoom,
                                      p.y * fixed_t(20.0) * zoom,
                                      p.z * fixed_t(20.0) * zoom});
                fixed_t nodes = smoothstep(nodeSize, nodeSize - fixed_t(0.05), 
                                         length({fractP.x - fixed_t(0.5), 
                                                fractP.y - fixed_t(0.5), 
                                                fractP.z - fixed_t(0.5)}));
                nodes *= node;
                
                vec3_t nodeColor = {fixed_t(0.8), fixed_t(0.3), fixed_t(1.0)};
                col.x += nodes * nodeColor.x * (fixed_t(1.0) - z);
                col.y += nodes * nodeColor.y * (fixed_t(1.0) - z);
                col.z += nodes * nodeColor.z * (fixed_t(1.0) - z);
            }
            
            // Light beams
            fixed_t beam = fixed_t(0.0);
            for (int j = 0; j < 3; j++) {
                #pragma HLS UNROLL
                fixed_t j_iter = fixed_t(j);
                fixed_t beamTime = time * (fixed_t(1.0) + j_iter * fixed_t(0.2));
                vec2_t beamDir = {hls::cos(beamTime), hls::sin(beamTime)};
                fixed_t p_val = uv.x * beamDir.x + uv.y * beamDir.y + hls::sin(time * fixed_t(2.0));
                beam += smoothstep(fixed_t(0.3), fixed_t(0.0), hls::abs(p_val)) * (fixed_t(1.0) - hls::abs(p_val));
            }
            
            vec3_t beamColor = {fixed_t(0.4), fixed_t(0.8), fixed_t(1.0)};
            col.x += beam * beamColor.x;
            col.y += beam * beamColor.y;
            col.z += beam * beamColor.z;
            
            // Apply glow and final effects
            vec3_t glow = {col.x * (fixed_t(0.5) + fixed_t(0.5) * hls::sin(time * fixed_t(10.0))),
                          col.y * (fixed_t(0.5) + fixed_t(0.5) * hls::sin(time * fixed_t(10.0))),
                          col.z * (fixed_t(0.5) + fixed_t(0.5) * hls::sin(time * fixed_t(10.0)))};
            
            col.x = hls::pow(col.x, fixed_t(1.2));
            col.y = hls::pow(col.y, fixed_t(1.2));
            col.z = hls::pow(col.z, fixed_t(1.2));
            
            col.x += glow.x * fixed_t(0.5);
            col.y += glow.y * fixed_t(0.5);
            col.z += glow.z * fixed_t(0.5);
            
            // Add ambient light
            fixed_t ambient = fixed_t(0.1) * smoothstep(fixed_t(0.8), fixed_t(0.0), length(uv));
            col.x += ambient * fixed_t(0.1);
            col.y += ambient * fixed_t(0.2);
            col.z += ambient * fixed_t(0.3);
            
            // Clamp and convert to 8-bit
            unsigned char r = (unsigned char)(hls::clamp(col.x, fixed_t(0), fixed_t(1)) * 255);
            unsigned char g = (unsigned char)(hls::clamp(col.y, fixed_t(0), fixed_t(1)) * 255);
            unsigned char b = (unsigned char)(hls::clamp(col.z, fixed_t(0), fixed_t(1)) * 255);
            
            pixel.data = (r << 16) | (g << 8) | b;
            dst_axi.write(pixel);
        }
    }
}

// Top-level function for Vivado HLS
void top_function(
    hls::stream<ap_axiu<24,1,1,1>> &input,
    hls::stream<ap_axiu<24,1,1,1>> &output,
    float time,
    int width,
    int height
) {
    #pragma HLS INTERFACE axis port=input
    #pragma HLS INTERFACE axis port=output
    #pragma HLS INTERFACE s_axilite port=time bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=width bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=height bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL
    
    hyperspatial_construct(input, output, fixed_t(time), width, height);
}