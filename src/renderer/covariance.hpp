#pragma once

// Gaussian covariance math, host/device-shared (CPU-tested in tests/test_covariance.cpp,
// executed per splat by the CUDA preprocess kernel).
//
// Frame convention for the EWA projection: this file works in the COLMAP-style projection
// frame — x right, y DOWN, z forward with z > 0 in front of the camera — because that is
// the frame the reference 3DGS math (and our pixel coordinates, image y down) live in.
// The renderer's view space is right-handed Y-up -Z forward; the named adapter
// projection_frame_from_view() below performs the (y,z) sign flip. Per CLAUDE.md rule 4,
// no other code may inline that flip.

#include <cmath>

#ifdef __CUDACC__
#define GSR_HD __host__ __device__
#else
#define GSR_HD
#endif

namespace gsr::renderer {

// 3D covariance Σ = R S Sᵀ Rᵀ from linear scale s and unit quaternion q = [w x y z]
// (SplatData conventions). Output: upper triangle [Σ00 Σ01 Σ02 Σ11 Σ12 Σ22].
GSR_HD inline void covariance_3d(const float s[3], const float q[4], float cov6[6]) {
  const float w = q[0], x = q[1], y = q[2], z = q[3];
  // Rotation matrix rows from the quaternion.
  const float r00 = 1.0f - 2.0f * (y * y + z * z);
  const float r01 = 2.0f * (x * y - w * z);
  const float r02 = 2.0f * (x * z + w * y);
  const float r10 = 2.0f * (x * y + w * z);
  const float r11 = 1.0f - 2.0f * (x * x + z * z);
  const float r12 = 2.0f * (y * z - w * x);
  const float r20 = 2.0f * (x * z - w * y);
  const float r21 = 2.0f * (y * z + w * x);
  const float r22 = 1.0f - 2.0f * (x * x + y * y);

  // M = R * diag(s); Σ = M Mᵀ.
  const float m00 = r00 * s[0], m01 = r01 * s[1], m02 = r02 * s[2];
  const float m10 = r10 * s[0], m11 = r11 * s[1], m12 = r12 * s[2];
  const float m20 = r20 * s[0], m21 = r21 * s[1], m22 = r22 * s[2];

  cov6[0] = m00 * m00 + m01 * m01 + m02 * m02;
  cov6[1] = m00 * m10 + m01 * m11 + m02 * m12;
  cov6[2] = m00 * m20 + m01 * m21 + m02 * m22;
  cov6[3] = m10 * m10 + m11 * m11 + m12 * m12;
  cov6[4] = m10 * m20 + m11 * m21 + m12 * m22;
  cov6[5] = m20 * m20 + m21 * m21 + m22 * m22;
}

// Named convention adapter (CLAUDE.md rule 2/4): renderer view space (y up, -z forward)
// -> projection frame (y down, +z forward). Applies to points and to rotation-matrix rows.
GSR_HD inline void projection_frame_from_view(float vx, float vy, float vz, float out[3]) {
  out[0] = vx;
  out[1] = -vy;
  out[2] = -vz;
}

// EWA splatting: project Σ (world) to the 2D image-plane covariance.
//   t          splat center in the PROJECTION frame (z = depth > 0)
//   fx, fy     focal lengths in pixels
//   tan_fovx/y frustum half-tangents used to clamp t off-axis (reference: 1.3x clamp)
//   w_rows     world->projection-frame rotation, 9 floats row-major
// Output cov2d = [a b c] for the symmetric 2x2 [[a, b], [b, c]], with the reference +0.3
// pixel dilation on the diagonal.
GSR_HD inline void project_covariance_ewa(const float cov6[6], float tx, float ty, float tz,
                                          float fx, float fy, float tan_fovx, float tan_fovy,
                                          const float w_rows[9], float cov2d[3]) {
  // Clamp the point to 1.3x the frustum so the Jacobian stays sane at the edges.
  const float limx = 1.3f * tan_fovx * tz;
  const float limy = 1.3f * tan_fovy * tz;
  tx = tx < -limx ? -limx : (tx > limx ? limx : tx);
  ty = ty < -limy ? -limy : (ty > limy ? limy : ty);

  // Jacobian of (u,v) = (fx*x/z, fy*y/z) at t. J is 2x3.
  const float inv_z = 1.0f / tz;
  const float j00 = fx * inv_z, j02 = -fx * tx * inv_z * inv_z;
  const float j11 = fy * inv_z, j12 = -fy * ty * inv_z * inv_z;

  // T = J * W (2x3).
  const float t00 = j00 * w_rows[0] + j02 * w_rows[6];
  const float t01 = j00 * w_rows[1] + j02 * w_rows[7];
  const float t02 = j00 * w_rows[2] + j02 * w_rows[8];
  const float t10 = j11 * w_rows[3] + j12 * w_rows[6];
  const float t11 = j11 * w_rows[4] + j12 * w_rows[7];
  const float t12 = j11 * w_rows[5] + j12 * w_rows[8];

  // cov2d = T Σ Tᵀ, with Σ from its upper triangle.
  const float s00 = cov6[0], s01 = cov6[1], s02 = cov6[2];
  const float s11 = cov6[3], s12 = cov6[4], s22 = cov6[5];

  const float a0 = t00 * s00 + t01 * s01 + t02 * s02;
  const float a1 = t00 * s01 + t01 * s11 + t02 * s12;
  const float a2 = t00 * s02 + t01 * s12 + t02 * s22;
  const float b0 = t10 * s00 + t11 * s01 + t12 * s02;
  const float b1 = t10 * s01 + t11 * s11 + t12 * s12;
  const float b2 = t10 * s02 + t11 * s12 + t12 * s22;

  cov2d[0] = a0 * t00 + a1 * t01 + a2 * t02 + 0.3f;
  cov2d[1] = a0 * t10 + a1 * t11 + a2 * t12;
  cov2d[2] = b0 * t10 + b1 * t11 + b2 * t12 + 0.3f;
}

// Minimum blendable alpha: the blend kernel skips contributions below 1/255, so any
// pixel where opacity * exp(power) < 1/255 can be excluded from binning too.
inline constexpr float kMinAlpha = 1.0f / 255.0f;

// Cutoff |power| for a splat of peak opacity alpha: alpha * exp(-cutoff) == 1/255.
GSR_HD inline float power_cutoff(float opacity) { return logf(255.0f * opacity); }

// Invert the 2D covariance into the conic used by the blend kernel; returns false when
// degenerate OR the splat can never reach 1/255 alpha (skip it). Writes the screen-space
// binning radius: opacity-aware alpha-cutoff extent sqrt(2*ln(255*alpha)) sigma —
// ~3.33 sigma for opaque splats, much tighter for translucent ones — rounded up.
GSR_HD inline bool conic_from_cov2d(const float cov2d[3], float opacity, float conic[3],
                                    float* radius_px) {
  if (!(opacity >= kMinAlpha)) return false;  // also rejects NaN opacity
  const float det = cov2d[0] * cov2d[2] - cov2d[1] * cov2d[1];
  if (det <= 0.0f) return false;
  const float inv_det = 1.0f / det;
  conic[0] = cov2d[2] * inv_det;
  conic[1] = -cov2d[1] * inv_det;
  conic[2] = cov2d[0] * inv_det;

  // Largest eigenvalue of the 2x2 (= sigma_max^2), then the alpha-cutoff radius.
  const float mid = 0.5f * (cov2d[0] + cov2d[2]);
  float disc = mid * mid - det;
  disc = disc > 0.1f ? disc : 0.1f;
  const float lambda_max = mid + sqrtf(disc);
  const float r = sqrtf(2.0f * power_cutoff(opacity) * lambda_max);
  const int ri = static_cast<int>(r);
  *radius_px = static_cast<float>(static_cast<float>(ri) < r ? ri + 1 : ri);
  return true;
}

namespace detail {
// Minimum of Q(x,y) = 0.5*(a x^2 + c y^2) + b x y along the segment x = x_fixed,
// y in [ly, uy] (closed form: clamp the 1D vertex).
GSR_HD inline float edge_min_x_fixed(float x, float ly, float uy, float a, float b,
                                     float c) {
  float y = c > 0.0f ? -b * x / c : ly;
  y = y < ly ? ly : (y > uy ? uy : y);
  return 0.5f * (a * x * x + c * y * y) + b * x * y;
}
GSR_HD inline float edge_min_y_fixed(float y, float lx, float ux, float a, float b,
                                     float c) {
  float x = a > 0.0f ? -b * y / a : lx;
  x = x < lx ? lx : (x > ux ? ux : x);
  return 0.5f * (a * x * x + c * y * y) + b * x * y;
}
}  // namespace detail

// Exact tile/splat overlap: does any PIXEL CENTER in the tile receive alpha >= 1/255?
// Evaluates the minimum Gaussian power over the tile's pixel rect (convex quadratic:
// interior if the center is inside, else the exact minimum over the four edges) and
// compares against power_cutoff(opacity). Replaces bounding-square binning, which claims
// every tile in the radius square — ~21%+ waste for isotropic splats, far more for
// elongated ones. MUST be the single source of truth for binning: the preprocess count
// and the key-emission loop both call this, or their counts diverge and the offset
// buffer is corrupted.
GSR_HD inline bool tile_overlaps_splat(int tile_x, int tile_y, int tile_size, float u,
                                       float v, const float conic[3], float cutoff) {
  const float lx = static_cast<float>(tile_x * tile_size) - u;
  const float ux = lx + static_cast<float>(tile_size - 1);
  const float ly = static_cast<float>(tile_y * tile_size) - v;
  const float uy = ly + static_cast<float>(tile_size - 1);
  if (lx <= 0.0f && 0.0f <= ux && ly <= 0.0f && 0.0f <= uy) return true;  // center inside

  const float a = conic[0], b = conic[1], c = conic[2];
  float m = detail::edge_min_x_fixed(lx, ly, uy, a, b, c);
  m = fminf(m, detail::edge_min_x_fixed(ux, ly, uy, a, b, c));
  m = fminf(m, detail::edge_min_y_fixed(ly, lx, ux, a, b, c));
  m = fminf(m, detail::edge_min_y_fixed(uy, lx, ux, a, b, c));
  return m <= cutoff;
}

}  // namespace gsr::renderer
