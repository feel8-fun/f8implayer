#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace f8::implayer {
struct ResolvedMediaSource {
  std::string video_url;
  std::string audio_url;
  std::vector<std::string> http_headers;
  std::string format_id;
  std::string protocol;
  std::string extractor;
};

struct YtdlpAuthOptions {
  std::string mode = "none";
  std::string browser;
  std::string browser_profile;
  std::string cookies_file;
};

std::string trim_copy(std::string value);
std::string lowercase_ascii(std::string value);
std::string normalize_url(std::string value);
std::vector<std::string> split_drop_payload(const std::string& raw);
bool should_resolve_with_ytdlp(const std::string& url);
bool load_resolved_from_json(const nlohmann::json& payload, ResolvedMediaSource& resolved, std::string& error);
bool resolve_ytdlp_media(const std::string& url, const YtdlpAuthOptions& auth, ResolvedMediaSource& resolved, std::string& error);
bool parse_auth_mode(const std::string& raw, std::string& normalized_mode);
bool is_supported_auth_mode(const std::string& mode);
bool is_supported_auth_browser(const std::string& browser);
bool is_sensitive_auth_field(const std::string& field);
bool is_profile_value_safe(const std::string& profile);
}  // namespace f8::implayer
