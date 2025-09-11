import numpy as np
import matplotlib.pyplot as plt

def mapRefract(p):
  d  = icosahedral(p, 1.0)
  id = 0.0
  return np.array([d, id])

def mapSolid(p, iTime):
  p[0], p[2] = rotate2D(np.array([p[0], p[2]]), iTime * 1.25)
  p[1], p[0] = rotate2D(np.array([p[1], p[0]]), iTime * 1.85)
  p[1] += np.sin(iTime) * 0.25
  p[0] += np.cos(iTime) * 0.25

  d = np.linalg.norm(p) - 0.25
  id = 1.0
  pulse = (np.sin(iTime * 2.) * 0.5 + 0.5) ** 9.0 * 2.

  d = d * (1 - pulse) + sdBox_1117569599(p, np.array([0.175, 0.175, 0.175])) * pulse

  return np.array([d, id])

def calcRayIntersection_3975550108(rayOrigin, rayDir, maxd=20.0, precis=0.001):
  latest = precis * 2.0
  dist   = 0.0
  type   = -1.0
  res    = np.array([-1.0, -1.0])

  for i in range(50):
    if latest < precis or dist > maxd:
      break

    result = mapRefract(rayOrigin + rayDir * dist)

    latest = result[0]
    type   = result[0]
    dist  += latest

  if dist < maxd:
    res = np.array([dist, type])

  return res

def calcRayIntersection_766934105(rayOrigin, rayDir, maxd=20.0, precis=0.001, iTime=0):
  latest = precis * 2.0
  dist   = 0.0
  type   = -1.0
  res    = np.array([-1.0, -1.0])

  for i in range(60):
    if latest < precis or dist > maxd:
      break

    result = mapSolid(rayOrigin + rayDir * dist, iTime)

    latest = result[0]
    type   = result[0]
    dist  += latest

  if dist < maxd:
    res = np.array([dist, type])

  return res

def calcNormal_3606979787(pos, eps=0.002):
  v1 = np.array([ 1.0,-1.0,-1.0])
  v2 = np.array([-1.0,-1.0, 1.0])
  v3 = np.array([-1.0, 1.0,-1.0])
  v4 = np.array([ 1.0, 1.0, 1.0])

  return np.linalg.norm(v1 * mapRefract( pos + v1*eps )[0] +
                        v2 * mapRefract( pos + v2*eps )[0] +
                        v3 * mapRefract( pos + v3*eps )[0] +
                        v4 * mapRefract( pos + v4*eps )[0] )

def calcNormal_1245821463(pos, eps=0.002, iTime=0):
  v1 = np.array([ 1.0,-1.0,-1.0])
  v2 = np.array([-1.0,-1.0, 1.0])
  v3 = np.array([-1.0, 1.0,-1.0])
  v4 = np.array([ 1.0, 1.0, 1.0])

  return np.linalg.norm(v1 * mapSolid( pos + v1*eps, iTime )[0] +
                        v2 * mapSolid( pos + v2*eps, iTime )[0] +
                        v3 * mapSolid( pos + v3*eps, iTime )[0] +
                        v4 * mapSolid( pos + v4*eps, iTime )[0] )

def beckmannDistribution_2315452051(x, roughness):
  NdotH = max(x, 0.0001)
  cos2Alpha = NdotH * NdotH
  tan2Alpha = (cos2Alpha - 1.0) / cos2Alpha
  roughness2 = roughness * roughness
  denom = np.pi * roughness2 * cos2Alpha * cos2Alpha
  return np.exp(tan2Alpha / roughness2) / denom

def cookTorranceSpecular_1460171947(lightDirection, viewDirection, surfaceNormal, roughness, fresnel):
  VdotN = max(np.dot(viewDirection, surfaceNormal), 0.0)
  LdotN = max(np.dot(lightDirection, surfaceNormal), 0.0)

  H = np.normalize(lightDirection + viewDirection)

  NdotH = max(np.dot(surfaceNormal, H), 0.0)
  VdotH = max(np.dot(viewDirection, H), 0.000001)
  LdotH = max(np.dot(lightDirection, H), 0.000001)
  G1 = (2.0 * NdotH * VdotN) / VdotH
  G2 = (2.0 * NdotH * LdotN) / LdotH
  G = min(1.0, min(G1, G2))

  D = beckmannDistribution_2315452051(NdotH, roughness)

  F = (1.0 - VdotN) ** fresnel

  return  G * F * D / max(np.pi * VdotN, 0.000001)

def squareFrame_1062606552(screenSize, coord):
  position = 2.0 * (coord[0] / screenSize[0]) - 1.0
  position = np.array([position, position])
  position[0] *= screenSize[0] / screenSize[0]
  return position

def calcLookAtMatrix_1535977339(origin, target, roll):
  rr = np.array([np.sin(roll), np.cos(roll), 0.0])
  ww = np.normalize(target - origin)
  uu = np.normalize(np.cross(ww, rr))
  vv = np.normalize(np.cross(uu, ww))

  return np.stack([uu, vv, ww], axis=1)  # mat3 as 3x3 array

def getRay_870892966(camMat, screenPos, lensLength):
  return np.normalize(np.dot(camMat, np.array([screenPos[0], screenPos[1], lensLength]))

def getRay_870892966_with_target(origin, target, screenPos, lensLength):
  camMat = calcLookAtMatrix_1535977339(origin, target, 0.0)
  return getRay_870892966(camMat, screenPos, lensLength)

def orbitCamera_421267681(camAngle, camHeight, camDistance, screenResolution, coord):
  screenPos = squareFrame_1062606552(screenResolution, coord)
  rayTarget = np.array([0.0, 0.0, 0.0])

  rayOrigin = np.array([
    camDistance * np.sin(camAngle),
    camHeight,
    camDistance * np.cos(camAngle)
  ])

  rayDirection = getRay_870892966_with_target(rayOrigin, rayTarget, screenPos, 2.0)
  return rayOrigin, rayDirection

def sdBox_1117569599(position, dimensions):
  d = np.abs(position) - dimensions

  return min(max(d[0], max(d[1],d[2])), 0.0) + np.linalg.norm(np.maximum(d, 0.0))

def random_2281831123(co):
  a = 12.9898
  b = 78.233
  c = 43758.5453
  dt= np.dot(co[:2] ,np.array([a,b]))
  sn= dt % 3.14
  return np.sin(sn) * c % 1.0

def fogFactorExp2_529295689(dist, density):
  LOG2 = -1.442695
  d = density * dist
  return 1.0 - np.clip(np.exp2(d * d * LOG2), 0.0, 1.0)

def intersectPlane(ro, rd, nor, dist):
  denom = np.dot(rd, nor)
  t = -(np.dot(ro, nor) + dist) / denom

  return t

n4 = np.array([0.577,0.577,0.577])
n5 = np.array([-0.577,0.577,0.577])
n6 = np.array([0.577,-0.577,0.577])
n7 = np.array([0.577,0.577,-0.577])
n8 = np.array([0.000,0.357,0.934])
n9 = np.array([0.000,-0.357,0.934])
n10 = np.array([0.934,0.000,0.357])
n11 = np.array([-0.934,0.000,0.357])
n12 = np.array([0.357,0.934,0.000])
n13 = np.array([-0.357,0.934,0.000])

def icosahedral(p, r):
  s = np.abs(np.dot(p,n4))
  s = max(s, np.abs(np.dot(p,n5)))
  s = max(s, np.abs(np.dot(p,n6)))
  s = max(s, np.abs(np.dot(p,n7)))
  s = max(s, np.abs(np.dot(p,n8)))
  s = max(s, np.abs(np.dot(p,n9)))
  s = max(s, np.abs(np.dot(p,n10)))
  s = max(s, np.abs(np.dot(p,n11)))
  s = max(s, np.abs(np.dot(p,n12)))
  s = max(s, np.abs(np.dot(p,n13)))
  return s - r

def rotate2D(p, a):
  return np.array([p[0] * np.cos(a) - p[1] * np.sin(a), p[0] * np.sin(a) + p[1] * np.cos(a)])

def palette(t, a, b, c, d):
    return a + b * np.cos( 6.28318*(c*t+d) )

def bg(ro, rd, iTime):
  col = 0.1 + (
    palette(np.clip((random_2281831123(rd[[0,2]] + np.sin(iTime * 0.1)) * 0.5 + 0.5) * 0.035 - rd[1] * 0.5 + 0.35, -1.0, 1.0),
      np.array([0.5, 0.45, 0.55]),
      np.array([0.5, 0.5, 0.5]),
      np.array([1.05, 1.0, 1.0]),
      np.array([0.275, 0.2, 0.19])
    )
  )

  t = intersectPlane(ro, rd, np.array([0, 1, 0]), 4.)

  if t > 0.0:
    p = ro + rd * t
    g = (1.0 - np.abs(np.sin(p[0]) * np.cos(p[2])) ** 0.25)

    col += (1.0 - fogFactorExp2_529295689(t, 0.04)) * g * np.array([5, 4, 2]) * 0.075

  return col

# Main simulation
def render_image(resolution=(800, 600), iTime=0.0, iMouse=np.array([0,0,0,0])):
  image = np.zeros((resolution[1], resolution[0], 4))

  for y in range(resolution[1]):
    for x in range(resolution[0]):
      fragCoord = np.array([x, y])
      uv = squareFrame_1062606552(resolution, fragCoord)
      dist = 4.5
      rotation = 6. * iMouse[0] / resolution[0] if iMouse[2] > 0 else iTime * 0.45
      height = 5. * (iMouse[1] / resolution[1] * 2.0 - 1.0) if iMouse[2] > 0 else -0.2
      
      ro, rd = orbitCamera_421267681(rotation, height, dist, resolution, fragCoord)

      color = bg(ro, rd, iTime)
      t = calcRayIntersection_3975550108(ro, rd)

      if t[0] > -0.5:
        pos = ro + rd * t[0]
        nor = calcNormal_3606979787(pos)
        ldir1 = np.normalize(np.array([0.8, 1, 0]))
        ldir2 = np.normalize(np.array([-0.4, -1.3, 0]))
        lcol1 = np.array([0.6, 0.5, 1.1])
        lcol2 = np.array([1.4, 0.9, 0.8]) * 0.7

        ref = np.refract(rd, nor, 0.97)
        u = calcRayIntersection_766934105(ro + ref * 0.1, ref, iTime=iTime)
        if u[0] > -0.5:
          pos2 = ro + ref * u[0]
          nor2 = calcNormal_1245821463(pos2, iTime=iTime)
          spec = cookTorranceSpecular_1460171947(ldir1, -ref, nor2, 0.6, 0.95) * 2.
          diff1 = 0.05 + max(0., np.dot(ldir1, nor2))
          diff2 = max(0., np.dot(ldir2, nor2))

          color = spec + (diff1 * lcol1 + diff2 * lcol2)
        else:
          color = bg(ro + ref * 0.1, ref, iTime) * 1.1

        color += color * cookTorranceSpecular_1460171947(ldir1, -rd, nor, 0.2, 0.9) * 2.
        color += 0.05

      vignette = 1.0 - max(0.0, np.dot(uv * 0.155, uv))

      color[0] = np.interp(color[0], [0.05, 0.995], [0,1])  # Approx smoothstep
      color[2] = np.interp(color[2], [-0.05, 0.95], [0,1])
      color[1] = np.interp(color[1], [-0.1, 0.95], [0,1])
      color[2] *= vignette

      image[y, x] = np.append(color, np.clip(t[0], 0.5, 1.0))

  return image

# Example usage
iTime = 0.0  # Replace with time
image = render_image(iResolution=(800, 600), iTime=iTime)
plt.imshow(image[:, :, :3])  # Show RGB
plt.show()