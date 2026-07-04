// freed_simulator — synthetic FreeD D1 stream generator for desk testing without a
// tracker (plan Phase 3 Task 3). Plain stdout is the product here (CLI tool).
//
// Profiles (all in FreeD space: Z up, X/Y horizontal; see docs/freed-protocol.md):
//   static    fixed pose at (radius, 0, height) facing the origin
//   orbit     circles the origin at --radius/--height, always facing it (default)
//   handheld  static pose + layered-sinusoid jitter (~cm position, ~0.3 deg angles)

#include "tracking/freed.hpp"
#include "tracking/udp_listener.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8001;
  double rate_hz = 50.0;
  std::string profile = "orbit";
  double radius_m = 3.0;
  double height_m = 1.5;
  double period_s = 12.0;  // orbit revolution time
  int camera_id = 1;
  std::uint32_t zoom = 0x080000;   // mid-range raw values
  std::uint32_t focus = 0x080000;
  double duration_s = 0.0;  // 0 = run until interrupted
};

void print_usage() {
  std::fprintf(
      stderr,
      "usage: freed_simulator [options]\n"
      "  --host IP        destination address        (default 127.0.0.1)\n"
      "  --port N         destination UDP port       (default 8001)\n"
      "  --rate HZ        packets per second         (default 50)\n"
      "  --profile P      static | orbit | handheld  (default orbit)\n"
      "  --radius M       orbit radius, meters       (default 3.0)\n"
      "  --height M       camera height, meters      (default 1.5)\n"
      "  --period S       seconds per revolution     (default 12)\n"
      "  --camera-id N    FreeD camera id            (default 1)\n"
      "  --zoom N         raw zoom counts            (default 524288)\n"
      "  --focus N        raw focus counts           (default 524288)\n"
      "  --duration S     stop after S seconds       (default: run forever)\n");
}

// Pose on the orbit at time t. Pan convention per docs/freed-protocol.md: rotation about
// +Z, positive clockwise seen from above, pan 0 = facing +X.
gsr::tracking::FreedPose pose_at(const Options& opt, double t) {
  gsr::tracking::FreedPose pose;
  pose.camera_id = static_cast<std::uint8_t>(opt.camera_id);
  pose.zoom_raw = opt.zoom;
  pose.focus_raw = opt.focus;

  const double theta = opt.profile == "orbit" ? 2.0 * kPi * t / opt.period_s : 0.0;
  pose.x_m = static_cast<float>(opt.radius_m * std::cos(theta));
  pose.y_m = static_cast<float>(opt.radius_m * std::sin(theta));
  pose.z_m = static_cast<float>(opt.height_m);
  // Face the origin: forward(pan) = (cos pan, -sin pan) must equal (-cos t, -sin t).
  pose.pan_rad = static_cast<float>(kPi - theta);
  // Look down at the floor point below the origin from our height.
  pose.tilt_rad = static_cast<float>(-std::atan2(opt.height_m, opt.radius_m));
  pose.roll_rad = 0.0f;

  if (opt.profile == "handheld") {
    // Layered sinusoids: breathing sway + fine tremor. Deterministic (fixed phases).
    const auto sway = [t](double f, double phase) { return std::sin(2.0 * kPi * f * t + phase); };
    pose.x_m += static_cast<float>(0.015 * sway(0.4, 0.0) + 0.004 * sway(2.8, 1.1));
    pose.y_m += static_cast<float>(0.012 * sway(0.33, 2.0) + 0.004 * sway(3.4, 0.4));
    pose.z_m += static_cast<float>(0.010 * sway(0.5, 4.0) + 0.003 * sway(3.1, 2.2));
    const double deg = kPi / 180.0;
    pose.pan_rad += static_cast<float>(0.3 * deg * sway(0.45, 0.7) + 0.06 * deg * sway(4.2, 1.9));
    pose.tilt_rad += static_cast<float>(0.25 * deg * sway(0.38, 3.1) + 0.06 * deg * sway(3.8, 0.2));
    pose.roll_rad += static_cast<float>(0.15 * deg * sway(0.3, 5.0));
  }
  return pose;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const auto value = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: %s needs a value\n", flag);
        return nullptr;
      }
      return argv[++i];
    };
    const char* v = nullptr;
    if (std::strcmp(argv[i], "--host") == 0 && (v = value("--host"))) opt.host = v;
    else if (std::strcmp(argv[i], "--port") == 0 && (v = value("--port"))) opt.port = static_cast<std::uint16_t>(std::stoi(v));
    else if (std::strcmp(argv[i], "--rate") == 0 && (v = value("--rate"))) opt.rate_hz = std::stod(v);
    else if (std::strcmp(argv[i], "--profile") == 0 && (v = value("--profile"))) opt.profile = v;
    else if (std::strcmp(argv[i], "--radius") == 0 && (v = value("--radius"))) opt.radius_m = std::stod(v);
    else if (std::strcmp(argv[i], "--height") == 0 && (v = value("--height"))) opt.height_m = std::stod(v);
    else if (std::strcmp(argv[i], "--period") == 0 && (v = value("--period"))) opt.period_s = std::stod(v);
    else if (std::strcmp(argv[i], "--camera-id") == 0 && (v = value("--camera-id"))) opt.camera_id = std::stoi(v);
    else if (std::strcmp(argv[i], "--zoom") == 0 && (v = value("--zoom"))) opt.zoom = static_cast<std::uint32_t>(std::stoul(v));
    else if (std::strcmp(argv[i], "--focus") == 0 && (v = value("--focus"))) opt.focus = static_cast<std::uint32_t>(std::stoul(v));
    else if (std::strcmp(argv[i], "--duration") == 0 && (v = value("--duration"))) opt.duration_s = std::stod(v);
    else {
      print_usage();
      return 2;
    }
  }
  if (opt.profile != "static" && opt.profile != "orbit" && opt.profile != "handheld") {
    std::fprintf(stderr, "error: unknown profile '%s'\n", opt.profile.c_str());
    print_usage();
    return 2;
  }
  if (opt.rate_hz <= 0.0 || opt.rate_hz > 1000.0) {
    std::fprintf(stderr, "error: --rate must be in (0, 1000]\n");
    return 2;
  }

  try {
    gsr::tracking::UdpSender sender(opt.host, opt.port);
    std::printf("freed_simulator: %s profile -> %s:%u at %.0f Hz%s\n", opt.profile.c_str(),
                opt.host.c_str(), opt.port, opt.rate_hz,
                opt.duration_s > 0 ? "" : " (Ctrl+C to stop)");

    const auto start = std::chrono::steady_clock::now();
    const auto interval =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / opt.rate_hz));
    auto next = start;
    std::uint64_t sent = 0, failed = 0, last_report = 0;

    while (true) {
      const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      if (opt.duration_s > 0.0 && t >= opt.duration_s) break;

      const auto packet = gsr::tracking::serialize_freed_d1(pose_at(opt, t));
      if (sender.send(packet)) ++sent; else ++failed;

      if (sent / static_cast<std::uint64_t>(opt.rate_hz) > last_report) {
        last_report = sent / static_cast<std::uint64_t>(opt.rate_hz);
        std::printf("sent %llu packets (%llu failed), t=%.1fs\n",
                    static_cast<unsigned long long>(sent),
                    static_cast<unsigned long long>(failed), t);
        std::fflush(stdout);
      }
      next += interval;
      std::this_thread::sleep_until(next);
    }
    std::printf("done: %llu packets sent, %llu failed\n",
                static_cast<unsigned long long>(sent),
                static_cast<unsigned long long>(failed));
    return failed == 0 ? 0 : 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}
