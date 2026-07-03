#pragma once

// Spherical-harmonics color evaluation (degrees 0-3), INRIA 3DGS convention.
// Header-only and host/device-shared: the exact code the CUDA kernels run is what the CPU
// unit tests exercise (tests/test_sh.cpp). Coefficient layout matches SplatData
// (docs/splat-memory-layout.md): rest is channel-major, R coeffs then G then B.

#ifdef __CUDACC__
#define GSR_HD __host__ __device__
#else
#define GSR_HD
#endif

namespace gsr::renderer {

// Real SH basis constants (Y_l^m up to l=3), matching the reference 3DGS rasterizer.
// NOTE: the array tables live INSIDE the functions — nvcc cannot reference namespace-scope
// constexpr arrays from device code (even with --expt-relaxed-constexpr).
inline constexpr float kSH0 = 0.28209479177387814f;
inline constexpr float kSH1 = 0.4886025119029199f;

// Non-DC basis functions for unit direction (x,y,z), written to basis[0..rest-1] in the
// f_rest coefficient order (rest = (degree+1)^2 - 1; degree must be 0..3).
GSR_HD inline void eval_sh_basis(int degree, float x, float y, float z, float* basis) {
  constexpr float kSH2[5] = {1.0925484305920792f, -1.0925484305920792f,
                             0.31539156525252005f, -1.0925484305920792f,
                             0.5462742152960396f};
  constexpr float kSH3[7] = {-0.5900435899266435f, 2.890611442640554f,
                             -0.4570457994644658f, 0.3731763325901154f,
                             -0.4570457994644658f, 1.445305721320277f,
                             -0.5900435899266435f};
  if (degree >= 1) {
    basis[0] = -kSH1 * y;
    basis[1] = kSH1 * z;
    basis[2] = -kSH1 * x;
  }
  if (degree >= 2) {
    const float xx = x * x, yy = y * y, zz = z * z;
    basis[3] = kSH2[0] * x * y;
    basis[4] = kSH2[1] * y * z;
    basis[5] = kSH2[2] * (2.0f * zz - xx - yy);
    basis[6] = kSH2[3] * x * z;
    basis[7] = kSH2[4] * (xx - yy);
  }
  if (degree >= 3) {
    const float xx = x * x, yy = y * y, zz = z * z;
    basis[8] = kSH3[0] * y * (3.0f * xx - yy);
    basis[9] = kSH3[1] * x * y * z;
    basis[10] = kSH3[2] * y * (4.0f * zz - xx - yy);
    basis[11] = kSH3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy);
    basis[12] = kSH3[4] * x * (4.0f * zz - xx - yy);
    basis[13] = kSH3[5] * z * (xx - yy);
    basis[14] = kSH3[6] * x * (xx - 3.0f * yy);
  }
}

// Full RGB evaluation for one splat. dc points at 3 floats [R G B]; rest points at the
// splat's 3*R channel-major coefficients (R = (degree+1)^2 - 1); (x,y,z) is the UNIT
// direction from the physical camera position to the splat center. Negative channel
// values are clamped to 0 (reference behavior). Writes rgb[0..2].
GSR_HD inline void eval_sh_color(int degree, const float* dc, const float* rest, float x,
                                 float y, float z, float* rgb) {
  float basis[15];
  eval_sh_basis(degree, x, y, z, basis);
  const int n_rest = (degree + 1) * (degree + 1) - 1;
  for (int channel = 0; channel < 3; ++channel) {
    float v = 0.5f + kSH0 * dc[channel];
    const float* coeffs = rest + channel * n_rest;
    for (int k = 0; k < n_rest; ++k) {
      v += basis[k] * coeffs[k];
    }
    rgb[channel] = v > 0.0f ? v : 0.0f;
  }
}

}  // namespace gsr::renderer
