typedef struct {
    float3 c0, c1, c2;
} mat3;

float3 mul_mat3_float3(mat3 m, float3 v) {
    return m.c0 * v.x + m.c1 * v.y + m.c2 * v.z;
}

float2 mapRefract(float3 p);
float2 mapSolid(float3 p);

float2 calcRayIntersection_3975550108(float3 rayOrigin, float3 rayDir, float maxd, float precis) {
  float latest = precis * 2.0f;
  float dist   = +0.0f;
  float type   = -1.0f;
  float2  res    = (float2)(-1.0f, -1.0f);

  for (int i = 0; i < 50; i++) {
    if (latest < precis || dist > maxd) break;

    float2 result = mapRefract(rayOrigin + rayDir * dist);

    latest = result.x;
    type   = result.x;
    dist  += latest;
  }

  if (dist < maxd) {
    res = (float2)(dist, type);
  }

  return res;
}

float2 calcRayIntersection_3975550108(float3 rayOrigin, float3 rayDir) {
  return calcRayIntersection_3975550108(rayOrigin, rayDir, 20.0f, 0.001f);
}

float2 calcRayIntersection_766934105(float3 rayOrigin, float3 rayDir, float maxd, float precis) {
  float latest = precis * 2.0f;
  float dist   = +0.0f;
  float type   = -1.0f;
  float2  res    = (float2)(-1.0f, -1.0f);

  for (int i = 0; i < 60; i++) {
    if (latest < precis || dist > maxd) break;

    float2 result = mapSolid(rayOrigin + rayDir * dist);

    latest = result.x;
    type   = result.x;
    dist  += latest;
  }

  if (dist < maxd) {
    res = (float2)(dist, type);
  }

  return res;
}

float2 calcRayIntersection_766934105(float3 rayOrigin, float3 rayDir) {
  return calcRayIntersection_766934105(rayOrigin, rayDir, 20.0f, 0.001f);
}

float3 calcNormal_3606979787(float3 pos, float eps) {
  const float3 v1 = (float3)( 1.0f,-1.0f,-1.0f);
  const float3 v2 = (float3)(-1.0f,-1.0f, 1.0f);
  const float3 v3 = (float3)(-1.0f, 1.0f,-1.0f);
  const float3 v4 = (float3)( 1.0f, 1.0f, 1.0f);

  return normalize( v1 * mapRefract( pos + v1*eps ).x +
                    v2 * mapRefract( pos + v2*eps ).x +
                    v3 * mapRefract( pos + v3*eps ).x +
                    v4 * mapRefract( pos + v4*eps ).x );
}

float3 calcNormal_3606979787(float3 pos) {
  return calcNormal_3606979787(pos, 0.002f);
}

float3 calcNormal_1245821463(float3 pos, float eps) {
  const float3 v1 = (float3)( 1.0f,-1.0f,-1.0f);
  const float3 v2 = (float3)(-1.0f,-1.0f, 1.0f);
  const float3 v3 = (float3)(-1.0f, 1.0f,-1.0f);
  const float3 v4 = (float3)( 1.0f, 1.0f, 1.0f);

  return normalize( v1 * mapSolid( pos + v1*eps ).x +
                    v2 * mapSolid( pos + v2*eps ).x +
                    v3 * mapSolid( pos + v3*eps ).x +
                    v4 * mapSolid( pos + v4*eps ).x );
}

float3 calcNormal_1245821463(float3 pos) {
  return calcNormal_1245821463(pos, 0.002f);
}

float beckmannDistribution_2315452051(float x, float roughness) {
  float NdotH = max(x, 0.0001f);
  float cos2Alpha = NdotH * NdotH;
  float tan2Alpha = (cos2Alpha - 1.0f) / cos2Alpha;
  float roughness2 = roughness * roughness;
  float denom = 3.141592653589793f * roughness2 * cos2Alpha * cos2Alpha;
  return exp(tan2Alpha / roughness2) / denom;
}

float cookTorranceSpecular_1460171947(
  float3 lightDirection,
  float3 viewDirection,
  float3 surfaceNormal,
  float roughness,
  float fresnel) {

  float VdotN = max(dot(viewDirection, surfaceNormal), 0.0f);
  float LdotN = max(dot(lightDirection, surfaceNormal), 0.0f);

  //Half angle vector
  float3 H = normalize(lightDirection + viewDirection);

  //Geometric term
  float NdotH = max(dot(surfaceNormal, H), 0.0f);
  float VdotH = max(dot(viewDirection, H), 0.000001f);
  float LdotH = max(dot(lightDirection, H), 0.000001f);
  float G1 = (2.0f * NdotH * VdotN) / VdotH;
  float G2 = (2.0f * NdotH * LdotN) / LdotH;
  float G = min(1.0f, min(G1, G2));

  //Distribution term
  float D = beckmannDistribution_2315452051(NdotH, roughness);

  //Fresnel term
  float F = pow(1.0f - VdotN, fresnel);

  //Multiply terms and done
  return  G * F * D / max(3.14159265f * VdotN, 0.000001f);
}

float2 squareFrame_1062606552(float2 screenSize, float2 coord) {
  float2 position = 2.0f * (coord.xx / screenSize.xx) - 1.0f;
  position.x *= screenSize.x / screenSize.x;
  return position;
}

mat3 calcLookAtMatrix_1535977339(float3 origin, float3 target, float roll) {
  float3 rr = (float3)(sin(roll), cos(roll), 0.0f);
  float3 ww = normalize(target - origin);
  float3 uu = normalize(cross(ww, rr));
  float3 vv = normalize(cross(uu, ww));

  mat3 m;
  m.c0 = uu;
  m.c1 = vv;
  m.c2 = ww;
  return m;
}

float3 getRay_870892966(mat3 camMat, float2 screenPos, float lensLength) {
  return normalize(mul_mat3_float3(camMat, (float3)(screenPos, lensLength)));
}

float3 getRay_870892966(float3 origin, float3 target, float2 screenPos, float lensLength) {
  mat3 camMat = calcLookAtMatrix_1535977339(origin, target, 0.0f);
  return getRay_870892966(camMat, screenPos, lensLength);
}

void orbitCamera_421267681(
  float camAngle,
  float camHeight,
  float camDistance,
  float2 screenResolution,
  float3 *rayOrigin,
  float3 *rayDirection,
  float2 coord
) {
  float2 screenPos = squareFrame_1062606552(screenResolution, coord);
  float3 rayTarget = (float3)(0.0f, 0.0f, 0.0f);

  *rayOrigin = (float3)(
    camDistance * sin(camAngle),
    camHeight,
    camDistance * cos(camAngle)
  );

  *rayDirection = getRay_870892966(*rayOrigin, rayTarget, screenPos, 2.0f);
}

float sdBox_1117569599(float3 position, float3 dimensions) {
  float3 d = fabs(position) - dimensions;

  return min(max(d.x, max(d.y,d.z)), 0.0f) + length(max(d, 0.0f));
}

float random_2281831123(float2 co)
{
    float a = 12.9898f;
    float b = 78.233f;
    float c = 43758.5453f;
    float dt= dot(co.xy ,(float2)(a,b));
    float sn= mod(dt,3.14f);
    return fract(sin(sn) * c);
}

float fogFactorExp2_529295689(
  float dist,
  float density
) {
  const float LOG2 = -1.442695f;
  float d = density * dist;
  return 1.0f - clamp(native_exp2(d * d * LOG2), 0.0f, 1.0f);
}

float intersectPlane(float3 ro, float3 rd, float3 nor, float dist) {
  float denom = dot(rd, nor);
  float t = -(dot(ro, nor) + dist) / denom;

  return t;
}

float3 n4 = (float3)(0.577f,0.577f,0.577f);
float3 n5 = (float3)(-0.577f,0.577f,0.577f);
float3 n6 = (float3)(0.577f,-0.577f,0.577f);
float3 n7 = (float3)(0.577f,0.577f,-0.577f);
float3 n8 = (float3)(0.000f,0.357f,0.934f);
float3 n9 = (float3)(0.000f,-0.357f,0.934f);
float3 n10 = (float3)(0.934f,0.000f,0.357f);
float3 n11 = (float3)(-0.934f,0.000f,0.357f);
float3 n12 = (float3)(0.357f,0.934f,0.000f);
float3 n13 = (float3)(-0.357f,0.934f,0.000f);

float icosahedral(float3 p, float r) {
  float s = fabs(dot(p,n4));
  s = max(s, fabs(dot(p,n5)));
  s = max(s, fabs(dot(p,n6)));
  s = max(s, fabs(dot(p,n7)));
  s = max(s, fabs(dot(p,n8)));
  s = max(s, fabs(dot(p,n9)));
  s = max(s, fabs(dot(p,n10)));
  s = max(s, fabs(dot(p,n11)));
  s = max(s, fabs(dot(p,n12)));
  s = max(s, fabs(dot(p,n13)));
  return s - r;
}

float2 rotate2D(float2 p, float a) {
  float ca = cos(a);
  float sa = sin(a);
  return (float2)(p.x * ca - p.y * sa, p.x * sa + p.y * ca);
}

float2 mapRefract(float3 p) {
  float d  = icosahedral(p, 1.0f);
  float id = 0.0f;

  return (float2)(d, id);
}

float2 mapSolid(float3 p) {
  p.xz = rotate2D(p.xz, i_time * 1.25f); // Assume iTime as kernel arg i_time
  p.yx = rotate2D(p.yx, i_time * 1.85f);
  p.y += sin(i_time) * 0.25f;
  p.x += cos(i_time) * 0.25f;

  float d = length(p) - 0.25f;
  float id = 1.0f;
  float pulse = pow(sin(i_time * 2.0f) * 0.5f + 0.5f, 9.0f) * 2.0f;

  d = mix(d, sdBox_1117569599(p, (float3)(0.175f,0.175f,0.175f)), pulse);

  return (float2)(d, id);
}

float3 palette( float t, float3 a, float3 b, float3 c, float3 d ) {
    return a + b*cos( 6.28318f*(c*t+d) );
}

float3 bg(float3 ro, float3 rd, float i_time) {
  float3 col = 0.1f + (
    palette(clamp((random_2281831123(rd.xz + sin(i_time * 0.1f)) * 0.5f + 0.5f) * 0.035f - rd.y * 0.5f + 0.35f, -1.0f, 1.0f)
      , (float3)(0.5f, 0.45f, 0.55f)
      , (float3)(0.5f, 0.5f, 0.5f)
      , (float3)(1.05f, 1.0f, 1.0f)
      , (float3)(0.275f, 0.2f, 0.19f)
    )
  );

  float t = intersectPlane(ro, rd, (float3)(0, 1, 0), 4.0f);

  if (t > 0.0f) {
    float3 p = ro + rd * t;
    float g = (1.0f - pow(fabs(sin(p.x) * cos(p.z)), 0.25f));

    col += (1.0f - fogFactorExp2_529295689(t, 0.04f)) * g * (float3)(5, 4, 2) * 0.075f;
  }

  return col;
}

__kernel void mainImage(__global float4 *output, float2 iResolution, float i_time, float4 iMouse) {
  int x = get_global_id(0);
  int y = get_global_id(1);
  float2 fragCoord = (float2)((float)x + 0.5f, (float)y + 0.5f); // Center pixel

  float3 ro, rd;

  float2  uv       = squareFrame_1062606552(iResolution, fragCoord);
  float dist     = 4.5f;
  float rotation = iMouse.z > 0.0f
    ? 6.0f * iMouse.x / iResolution.x
    : i_time * 0.45f;
  float height = iMouse.z > 0.0f
    ? 5.0f * (iMouse.y / iResolution.y * 2.0f - 1.0f)
    : -0.2f;
    
  orbitCamera_421267681(rotation, height, dist, iResolution, &ro, &rd, fragCoord);

  float3 color = bg(ro, rd, i_time);
  float2 t = calcRayIntersection_3975550108(ro, rd);
  if (t.x > -0.5f) {
    float3 pos = ro + rd * t.x;
    float3 nor = calcNormal_3606979787(pos);
    float3 ldir1 = normalize((float3)(0.8f, 1.0f, 0.0f));
    float3 ldir2 = normalize((float3)(-0.4f, -1.3f, 0.0f));
    float3 lcol1 = (float3)(0.6f, 0.5f, 1.1f);
    float3 lcol2 = (float3)(1.4f, 0.9f, 0.8f) * 0.7f;

    float3 ref = refract(rd, nor, 0.97f);
    float2 u = calcRayIntersection_766934105(ro + ref * 0.1f, ref);
    if (u.x > -0.5f) {
      float3 pos2 = ro + ref * u.x;
      float3 nor2 = calcNormal_1245821463(pos2);
      float spec = cookTorranceSpecular_1460171947(ldir1, -ref, nor2, 0.6f, 0.95f) * 2.0f;
      float diff1 = 0.05f + max(0.0f, dot(ldir1, nor2));
      float diff2 = max(0.0f, dot(ldir2, nor2));

      color = spec + (diff1 * lcol1 + diff2 * lcol2);
    } else {
      color = bg(ro + ref * 0.1f, ref, i_time) * 1.1f;
    }

    color += color * cookTorranceSpecular_1460171947(ldir1, -rd, nor, 0.2f, 0.9f) * 2.0f;
    color += 0.05f;
  }

  float vignette = 1.0f - max(0.0f, dot(uv * 0.155f, uv));

  color.x = smoothstep(0.05f, 0.995f, color.x);
  color.z = smoothstep(-0.05f, 0.95f, color.z);
  color.y = smoothstep(-0.1f, 0.95f, color.y);
  color.z *= vignette;

  int index = y * (int)iResolution.x + x;
  output[index] = (float4)(color, clamp(t.x, 0.5f, 1.0f));
}