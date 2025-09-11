__kernel void mainImage(__global float4 *fragColor, float iTime, uint width, uint height, __read_only image2d_t iChannel0) {
    uint x = get_global_id(0);
    uint y = get_global_id(1);
    if (x >= width || y >= height) return;

    float2 fragCoord = (float2)((float)x + 0.5f, (float)y + 0.5f);
    float2 uv = (fragCoord * 2.0f - (float2)((float)width, (float)height)) / (float)height;

    const float PI = 3.14159265f;

    float2 path(float z) {
        return (float2)(0.5f * sin(z), 0.5f * sin(z * 0.7f));
    }

    float map(float3 p) {
        return -length(p.xy - path(p.z)) + 1.2f + 0.3f * sin(p.z * 0.4f);
    }

    float3 normal(float3 p) {
        float d = map(p);
        float2 e = (float2)(0.01f, 0.0f);
        float3 n = d - (float3)(
            map(p - (float3)(e.x, e.y, e.y)),
            map(p - (float3)(e.y, e.x, e.y)),
            map(p - (float3)(e.y, e.y, e.x))
        );
        return normalize(n);
    }

    float sMax(float a, float b, float k) {
        return log(exp(k * a) + exp(k * b)) / k;
    }

    float bumpFunction(float3 p) {
        float2 c = path(p.z);
        float id = floor(p.z * 4.0f - 0.25f);
        float h = 0.5f + 0.5f * sin(atan2(p.y - c.y, p.x - c.x) * 20.0f + 1.5f * (2.0f * fmod(id, 2.0f) - 1.0f) + iTime * 5.0f);  // Used atan2 for proper angle
        h = sMax(h, 0.5f + 0.5f * sin(p.z * 8.0f * PI), 16.0f);
        h *= h;
        h *= h * h;
        return 1.0f - h;
    }

    float3 bumpNormal(float3 p, float3 n, float bumpFactor) {
        float3 e = (float3)(0.01f, 0.0f, 0.0f);
        float f = bumpFunction(p);
        float fx1 = bumpFunction(p - (float3)(e.x, 0.0f, 0.0f));
        float fy1 = bumpFunction(p - (float3)(0.0f, e.x, 0.0f));
        float fz1 = bumpFunction(p - (float3)(0.0f, 0.0f, e.x));
        float fx2 = bumpFunction(p + (float3)(e.x, 0.0f, 0.0f));
        float fy2 = bumpFunction(p + (float3)(0.0f, e.x, 0.0f));
        float fz2 = bumpFunction(p + (float3)(0.0f, 0.0f, e.x));
        float3 grad = (float3)(fx1 - fx2, fy1 - fy2, fz1 - fz2) / (e.x * 2.0f);
        grad -= n * dot(n, grad);
        return normalize(n + grad * bumpFactor);
    }

    float vel = iTime * 1.5f;
    float3 ro = (float3)(path(vel - 1.0f).x, path(vel - 1.0f).y, vel - 1.0f);
    float3 ta = (float3)(path(vel).x, path(vel).y, vel);
    float3 fwd = normalize(ta - ro);
    float3 upv = (float3)(0.0f, 1.0f, 0.0f);  // Renamed to avoid conflict with 'up'
    float3 right = cross(fwd, upv);
    upv = cross(right, fwd);
    float fl = 1.2f;
    float3 rd = normalize(fwd + fl * (uv.x * right + uv.y * upv));

    float glow = 0.0f;
    float3 glowCol = (float3)(9.0f, 7.0f, 4.0f);
    float t = 0.0f;
    float3 col = (float3)(0.0f);

    for (int i = 0; i < 125; i++) {
        float3 p = ro + t * rd;
        float d = map(p);
        glow += exp(-d * 8.0f) * 0.005f;
        if (d < 0.01f) {
            float3 n = normal(p);
            float3 lightDir = n;
            n = bumpNormal(p, n, 0.02f);
            float2 c = path(p.z);
            float2 id = (float2)(floor(p.z * 4.0f - 0.25f), atan2(p.y - c.y, p.x - c.x));
            float3 tileCol = (float3)(0.7f);
            tileCol += 0.4f * (float3)(sin(id.x), cos(id.x), 0.0f);
            tileCol += 0.3f * sin(id.x * 0.5f + id.y * 6.0f - iTime * 4.0f);
            float3 tileGray = (float3)(0.5f);
            float height = bumpFunction(p);
            float3 baseCol = mix(tileGray, tileCol, height);
            float diffuseL = max(dot(n, lightDir), 0.0f);
            col += diffuseL * baseCol;
            float3 h = normalize(lightDir - rd);
            float specL = pow(max(dot(n, h), 0.0f), 64.0f);
            col += specL * 0.3f;
            float3 r = reflect(rd, n);
            const sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_REPEAT | CLK_FILTER_LINEAR;
            float4 reflCol4 = read_imagef(iChannel0, sampler, r.xy);  // Assuming 2D projection
            float3 reflCol = reflCol4.xyz;
            col = mix(col, reflCol, 0.3f);
            break;
        }
        t += d;
    }

    col += glowCol * glow;
    col = pow(col, (float3)(2.2f));
    uint idx = y * width + x;
    fragColor[idx] = (float4)(col, 1.0f);
}