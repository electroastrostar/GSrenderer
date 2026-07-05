#include "app/config.hpp"

#include <toml++/toml.hpp>

#include <stdexcept>

namespace gsr::app {

void apply_config_file(const std::string& path, RunConfig* config) {
  toml::table table;
  try {
    table = toml::parse_file(path);
  } catch (const toml::parse_error& e) {
    throw std::runtime_error("config " + path + ": " + std::string(e.description()));
  }

  const auto get = [&](const toml::table* section, const char* key, auto* out) {
    if (section == nullptr) return;
    using T = std::remove_pointer_t<decltype(out)>;
    if (const auto node = section->get(key)) {
      if constexpr (std::is_same_v<T, std::string>) {
        if (const auto v = node->value<std::string>()) { *out = *v; return; }
      } else if constexpr (std::is_same_v<T, bool>) {
        if (const auto v = node->value<bool>()) { *out = *v; return; }
      } else if constexpr (std::is_same_v<T, int>) {
        if (const auto v = node->value<std::int64_t>()) { *out = static_cast<int>(*v); return; }
      } else {  // float
        if (const auto v = node->value<double>()) { *out = static_cast<float>(*v); return; }
      }
      throw std::runtime_error("config " + path + ": wrong type for '" + key + "'");
    }
  };

  get(&table, "asset", &config->asset);

  const auto* output = table["output"].as_table();
  get(output, "width", &config->width);
  get(output, "height", &config->height);
  get(output, "fov_deg", &config->fov_deg);
  get(output, "vsync", &config->vsync);
  get(output, "overscan_pct", &config->overscan_pct);

  const auto* render = table["render"].as_table();
  get(render, "sh_clamp", &config->sh_clamp);
  get(render, "flip_scene", &config->flip_scene);

  const auto* tracking = table["tracking"].as_table();
  get(tracking, "freed_port", &config->freed_port);
  get(tracking, "latency_ms", &config->latency_ms);

  const auto* lens = table["lens"].as_table();
  get(lens, "file", &config->lens_file);
  get(lens, "sensor_height_mm", &config->sensor_height_mm);

  const auto* ndi = table["ndi"].as_table();
  get(ndi, "name", &config->ndi_name);
  get(ndi, "fps", &config->ndi_fps);

  const auto* stage = table["stage"].as_table();
  get(stage, "yaw_deg", &config->stage_yaw_deg);
  if (stage != nullptr) {
    if (const auto* offset = stage->get_as<toml::array>("offset_m")) {
      if (offset->size() != 3) {
        throw std::runtime_error("config " + path + ": stage.offset_m needs 3 values");
      }
      for (int i = 0; i < 3; ++i) {
        const auto v = (*offset)[static_cast<std::size_t>(i)].value<double>();
        if (!v) throw std::runtime_error("config " + path + ": stage.offset_m non-numeric");
        config->stage_offset_m[i] = static_cast<float>(*v);
      }
    }
  }
}

}  // namespace gsr::app
