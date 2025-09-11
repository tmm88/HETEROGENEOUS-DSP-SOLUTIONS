from myhdl import block, always_comb, Signal, float_signal  # float_signal for simulation only

def vec2(x, y):
    return (x, y)

def vec3(x, y, z):
    return (x, y, z)

@block
def Shader(frag_x, frag_y, iTime, resolution_x, resolution_y, col_r, col_g, col_b, col_a):
    # Signals are floats for sim, but not synthesizable
    PI = 3.14159265

    def path(z):
        return vec2(0.5 * math.sin(z), 0.5 * math.sin(z * 0.7))

    def map_func(p):
        px, py, pz = p
        cx, cy = path(pz)
        return -math.sqrt((px - cx)**2 + (py - cy)**2) + 1.2 + 0.3 * math.sin(pz * 0.4)

    def normal(p):
        d = map_func(p)
        e = 0.01
        nx = d - map_func((p[0] - e, p[1], p[2]))
        ny = d - map_func((p[0], p[1] - e, p[2]))
        nz = d - map_func((p[0], p[1], p[2] - e))
        len_n = math.sqrt(nx**2 + ny**2 + nz**2)
        return (nx / len_n, ny / len_n, nz / len_n)

    def sMax(a, b, k):
        return math.log(math.exp(k * a) + math.exp(k * b)) / k

    def bumpFunction(p):
        px, py, pz = p
        c = path(pz)
        cx, cy = c
        id = math.floor(pz * 4 - 0.25)
        h = 0.5 + 0.5 * math.sin(math.atan2(py - cy, px - cx) * 20 + 1.5 * (2 * (id % 2) - 1) + iTime.val * 5)
        h = sMax(h, 0.5 + 0.5 * math.sin(pz * 8 * PI), 16)
        h *= h
        h *= h * h
        return 1 - h

    def bumpNormal(p, n, bumpFactor):
        e = 0.01
        f = bumpFunction(p)
        fx1 = bumpFunction((p[0] - e, p[1], p[2]))
        fy1 = bumpFunction((p[0], p[1] - e, p[2]))
        fz1 = bumpFunction((p[0], p[1], p[2] - e))
        fx2 = bumpFunction((p[0] + e, p[1], p[2]))
        fy2 = bumpFunction((p[0], p[1] + e, p[2]))
        fz2 = bumpFunction((p[0], p[1], p[2] + e))
        grad_x = (fx1 - fx2) / (2 * e)
        grad_y = (fy1 - fy2) / (2 * e)
        grad_z = (fz1 - fz2) / (2 * e)
        nx, ny, nz = n
        dot_ng = nx * grad_x + ny * grad_y + nz * grad_z
        grad_x -= nx * dot_ng
        grad_y -= ny * dot_ng
        grad_z -= nz * dot_ng
        gx, gy, gz = grad_x * bumpFactor, grad_y * bumpFactor, grad_z * bumpFactor
        nx += gx
        ny += gy
        nz += gz
        len_n = math.sqrt(nx**2 + ny**2 + nz**2)
        return (nx / len_n, ny / len_n, nz / len_n)

    @always_comb
    def logic():
        import math  # For sim only
        fragCoord_x = frag_x.val + 0.5
        fragCoord_y = frag_y.val + 0.5
        uv_x = (fragCoord_x * 2 - resolution_x.val) / resolution_y.val
        uv_y = (fragCoord_y * 2 - resolution_y.val) / resolution_y.val

        vel = iTime.val * 1.5
        ro_path = path(vel - 1)
        ro = vec3(ro_path[0], ro_path[1], vel - 1)
        ta_path = path(vel)
        ta = vec3(ta_path[0], ta_path[1], vel)
        fwd_x = ta[0] - ro[0]
        fwd_y = ta[1] - ro[1]
        fwd_z = ta[2] - ro[2]
        len_fwd = math.sqrt(fwd_x**2 + fwd_y**2 + fwd_z**2)
        fwd = (fwd_x / len_fwd, fwd_y / len_fwd, fwd_z / len_fwd)
        upv = vec3(0, 1, 0)
        right = (fwd[1]*upv[2] - fwd[2]*upv[1], fwd[2]*upv[0] - fwd[0]*upv[2], fwd[0]*upv[1] - fwd[1]*upv[0])
        upv = (right[1]*fwd[2] - right[2]*fwd[1], right[2]*fwd[0] - right[0]*fwd[2], right[0]*fwd[1] - right[1]*fwd[0])
        fl = 1.2
        rd_add_x = fl * (uv_x * right[0] + uv_y * upv[0])
        rd_add_y = fl * (uv_x * right[1] + uv_y * upv[1])
        rd_add_z = fl * (uv_x * right[2] + uv_y * upv[2])
        rd_x = fwd[0] + rd_add_x
        rd_y = fwd[1] + rd_add_y
        rd_z = fwd[2] + rd_add_z
        len_rd = math.sqrt(rd_x**2 + rd_y**2 + rd_z**2)
        rd = (rd_x / len_rd, rd_y / len_rd, rd_z / len_rd)

        glow = 0.0
        glowCol = vec3(9, 7, 4)
        t = 0.0
        col = vec3(0, 0, 0)

        for i in range(125):
            p = (ro[0] + t * rd[0], ro[1] + t * rd[1], ro[2] + t * rd[2])
            d = map_func(p)
            glow += math.exp(-d * 8) * 0.005
            if d < 0.01:
                n = normal(p)
                lightDir = n
                n = bumpNormal(p, n, 0.02)
                c = path(p[2])
                id_x = math.floor(p[2] * 4 - 0.25)
                id_y = math.atan2(p[1] - c[1], p[0] - c[0])
                tileCol = vec3(0.7, 0.7, 0.7)
                tileCol = (tileCol[0] + 0.4 * math.sin(id_x), tileCol[1] + 0.4 * math.cos(id_x), tileCol[2])
                sin_val = 0.3 * math.sin(id_x * 0.5 + id_y * 6 - iTime.val * 4)
                tileCol = (tileCol[0] + sin_val, tileCol[1] + sin_val, tileCol[2] + sin_val)
                tileGray = vec3(0.5, 0.5, 0.5)
                height = bumpFunction(p)
                baseCol_x = tileGray[0] + (tileCol[0] - tileGray[0]) * height
                baseCol_y = tileGray[1] + (tileCol[1] - tileGray[1]) * height
                baseCol_z = tileGray[2] + (tileCol[2] - tileGray[2]) * height
                baseCol = (baseCol_x, baseCol_y, baseCol_z)
                diffuseL = max(n[0] * lightDir[0] + n[1] * lightDir[1] + n[2] * lightDir[2], 0)
                col = (col[0] + diffuseL * baseCol[0], col[1] + diffuseL * baseCol[1], col[2] + diffuseL * baseCol[2])
                h_x = (lightDir[0] - rd[0])
                h_y = (lightDir[1] - rd[1])
                h_z = (lightDir[2] - rd[2])
                len_h = math.sqrt(h_x**2 + h_y**2 + h_z**2)
                h = (h_x / len_h, h_y / len_h, h_z / len_h)
                specL = math.pow(max(n[0] * h[0] + n[1] * h[1] + n[2] * h[2], 0), 64)
                col = (col[0] + specL * 0.3, col[1] + specL * 0.3, col[2] + specL * 0.3)
                r = reflect(rd, n)  # Need to define reflect similar to above
                reflCol = vec3(0.5, 0.5, 0.5)  # Placeholder
                col = (col[0] + (reflCol[0] - col[0]) * 0.3, col[1] + (reflCol[1] - col[1]) * 0.3, col[2] + (reflCol[2] - col[2]) * 0.3)
                break
            t += d

        col = (col[0] + glowCol[0] * glow, col[1] + glowCol[1] * glow, col[2] + glowCol[2] * glow)
        col = (math.pow(col[0], 2.2), math.pow(col[1], 2.2), math.pow(col[2], 2.2))
        col_r.next = col[0]
        col_g.next = col[1]
        col_b.next = col[2]
        col_a.next = 1.0

    return logic

# Example usage for simulation (not synthesis)
from myhdl import Simulation, delay
import math  # Required for math functions

@block
def testbench():
    frag_x = Signal(0.0)
    frag_y = Signal(0.0)
    iTime = Signal(0.0)
    resolution_x = Signal(800.0)
    resolution_y = Signal(600.0)
    col_r = Signal(0.0)
    col_g = Signal(0.0)
    col_b = Signal(0.0)
    col_a = Signal(0.0)

    dut = Shader(frag_x, frag_y, iTime, resolution_x, resolution_y, col_r, col_g, col_b, col_a)

    @instance
    def stimulus():
        frag_x.next = 400
        frag_y.next = 300
        iTime.next = 0.0
        yield delay(10)
        print("Color:", col_r, col_g, col_b, col_a)

    return dut, stimulus

tb = testbench()
sim = Simulation(tb)
sim.run()