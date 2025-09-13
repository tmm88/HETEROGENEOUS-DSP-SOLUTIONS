#pragma OPENCL EXTENSION cl_khr_fp64 : enable

float smoothstep(float edge0, float edge1, float x) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float3 normalize(float3 v) {
    float len = length(v);
    return v / len;
}

float3 fract(float3 v) {
    return (float3)(v.x - floor(v.x), v.y - floor(v.y), v.z - floor(v.z));
}

float3 abs_vec3(float3 v) {
    return (float3)(fabs(v.x), fabs(v.y), fabs(v.z));
}

__kernel void hyperspatial_construct(
    __global uchar4* output,
    const float iTime,
    const int width,
    const int height
) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    float2 uv = (float2)((float)x / width, (float)y / height);
    uv -= (float2)(0.5f, 0.5f);
    uv.x *= (float)width / height;
    
    float time = iTime;
    
    // Create perspective effect
    float zoom = 1.0f + sin(time * 0.5f) * 0.2f;
    float3 rayDir = normalize((float3)(uv.x, uv.y, 1.5f));
    float3 rayOrigin = (float3)(0.0f, 0.0f, -time * 0.5f);
    
    float3 col = (float3)(0.0f, 0.0f, 0.0f);
    
    // Main rendering loop
    for (int i = 0; i < 5; i++) {
        float z = fmod(rayOrigin.z + i * 0.3f, 1.0f);
        float3 p = rayOrigin + rayDir * z;
        
        // Create 3D grid
        float3 grid = abs_vec3(fract(p * 20.0f * zoom) - (float3)(0.5f, 0.5f, 0.5f));
        
        // Grid lines with perspective
        float gridLines = smoothstep(0.08f, 0.06f, length(grid.xy));
        gridLines *= smoothstep(0.1f, 0.0f, grid.z);
        
        float3 gridColor = (float3)(0.3f, 0.6f, 1.0f);
        col += gridLines * gridColor * (1.0f - z);
        
        // Complex 3D nodes
        float3 nodePos = floor(p * 20.0f * zoom);
        float node = sin(nodePos.x * 1.2f + nodePos.y * 1.8f + nodePos.z * 2.1f + time * 3.0f);
        node = 0.5f + 0.5f * node;
        
        float nodeSize = 0.1f + 0.05f * sin(time * 4.0f + dot(nodePos, (float3)(1.0f, 2.0f, 3.0f)));
        
        float3 fractP = fract(p * 20.0f * zoom);
        float nodes = smoothstep(nodeSize, nodeSize - 0.05f, length(fractP - (float3)(0.5f, 0.5f, 0.5f)));
        nodes *= node;
        
        float3 nodeColor = (float3)(0.8f, 0.3f, 1.0f);
        col += nodes * nodeColor * (1.0f - z);
    }
    
    // Light beams
    float beam = 0.0f;
    for (int j = 0; j < 3; j++) {
        float beamTime = time * (1.0f + j * 0.2f);
        float2 beamDir = (float2)(cos(beamTime), sin(beamTime));
        float p_val = dot(uv, beamDir) + sin(time * 2.0f);
        beam += smoothstep(0.3f, 0.0f, fabs(p_val)) * (1.0f - fabs(p_val));
    }
    
    float3 beamColor = (float3)(0.4f, 0.8f, 1.0f);
    col += beam * beamColor;
    
    // Apply glow and final effects
    float3 glow = col * (0.5f + 0.5f * sin(time * 10.0f));
    col = pow(col, (float3)(1.2f, 1.2f, 1.2f));
    col += glow * 0.5f;
    
    // Add ambient light
    float ambient = 0.1f * smoothstep(0.8f, 0.0f, length(uv));
    col += (float3)(0.1f, 0.2f, 0.3f) * ambient;
    
    // Clamp and output
    col = clamp(col, (float3)(0.0f, 0.0f, 0.0f), (float3)(1.0f, 1.0f, 1.0f));
    output[y * width + x] = (uchar4)(col.x * 255, col.y * 255, col.z * 255, 255);
}

// Alternative kernel with image2d_t for better performance with textures
__kernel void hyperspatial_construct_image(
    __write_only image2d_t output,
    const float iTime,
    const int width,
    const int height
) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height) return;
    
    float2 uv = (float2)((float)x / width, (float)y / height);
    uv -= (float2)(0.5f, 0.5f);
    uv.x *= (float)width / height;
    
    float time = iTime;
    
    // Create perspective effect
    float zoom = 1.0f + sin(time * 0.5f) * 0.2f;
    float3 rayDir = normalize((float3)(uv.x, uv.y, 1.5f));
    float3 rayOrigin = (float3)(0.0f, 0.0f, -time * 0.5f);
    
    float3 col = (float3)(0.0f, 0.0f, 0.0f);
    
    // Main rendering loop - unrolled for better performance
    #pragma unroll 2
    for (int i = 0; i < 5; i++) {
        float z = fmod(rayOrigin.z + i * 0.3f, 1.0f);
        float3 p = rayOrigin + rayDir * z;
        
        // Create 3D grid
        float3 grid = abs_vec3(fract(p * 20.0f * zoom) - (float3)(0.5f, 0.5f, 0.5f));
        
        // Grid lines with perspective
        float gridLines = smoothstep(0.08f, 0.06f, length(grid.xy));
        gridLines *= smoothstep(0.1f, 0.0f, grid.z);
        
        float3 gridColor = (float3)(0.3f, 0.6f, 1.0f);
        col += gridLines * gridColor * (1.0f - z);
        
        // Complex 3D nodes
        float3 nodePos = floor(p * 20.0f * zoom);
        float node = sin(nodePos.x * 1.2f + nodePos.y * 1.8f + nodePos.z * 2.1f + time * 3.0f);
        node = 0.5f + 0.5f * node;
        
        float nodeSize = 0.1f + 0.05f * sin(time * 4.0f + dot(nodePos, (float3)(1.0f, 2.0f, 3.0f)));
        
        float3 fractP = fract(p * 20.0f * zoom);
        float nodes = smoothstep(nodeSize, nodeSize - 0.05f, length(fractP - (float3)(0.5f, 0.5f, 0.5f)));
        nodes *= node;
        
        float3 nodeColor = (float3)(0.8f, 0.3f, 1.0f);
        col += nodes * nodeColor * (1.0f - z);
    }
    
    // Light beams
    float beam = 0.0f;
    #pragma unroll
    for (int j = 0; j < 3; j++) {
        float beamTime = time * (1.0f + j * 0.2f);
        float2 beamDir = (float2)(cos(beamTime), sin(beamTime));
        float p_val = dot(uv, beamDir) + sin(time * 2.0f);
        beam += smoothstep(0.3f, 0.0f, fabs(p_val)) * (1.0f - fabs(p_val));
    }
    
    float3 beamColor = (float3)(0.4f, 0.8f, 1.0f);
    col += beam * beamColor;
    
    // Apply glow and final effects
    float3 glow = col * (0.5f + 0.5f * sin(time * 10.0f));
    col = pow(col, (float3)(1.2f, 1.2f, 1.2f));
    col += glow * 0.5f;
    
    // Add ambient light
    float ambient = 0.1f * smoothstep(0.8f, 0.0f, length(uv));
    col += (float3)(0.1f, 0.2f, 0.3f) * ambient;
    
    // Clamp and output
    col = clamp(col, (float3)(0.0f, 0.0f, 0.0f), (float3)(1.0f, 1.0f, 1.0f));
    
    // Write to image
    write_imagef(output, (int2)(x, y), (float4)(col.x, col.y, col.z, 1.0f));
}