#include "media_source.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#if defined(_WIN32)
#define F8_POPEN _popen
#define F8_PCLOSE _pclose
#else
#define F8_POPEN popen
#define F8_PCLOSE pclose
#endif
namespace f8::implayer {
using json = nlohmann::json;
std::string trim_copy(std::string s) {
  auto is_ws = [](unsigned char ch) {
    return std::isspace(ch) != 0;
  };
  while (!s.empty() && is_ws(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && is_ws(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

bool looks_like_url_without_scheme(const std::string& s) {
  if (s.empty())
    return false;
  if (s.find("://") != std::string::npos)
    return false;

  const char first = s.front();
  if (first == '/' || first == '\\' || first == '.' || first == '~')
    return false;
  if (s.find('\\') != std::string::npos)
    return false;

  // Keep local Windows paths as local files (e.g. C:/xx or D:\xx).
  if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) != 0 && s[1] == ':')
    return false;

  const std::size_t host_end = s.find_first_of("/?#");
  const std::string host = s.substr(0, host_end);
  if (host.empty())
    return false;

  bool has_dot = false;
  bool has_alpha = false;
  for (char ch : host) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isalpha(uch) != 0) {
      has_alpha = true;
      continue;
    }
    if (std::isdigit(uch) != 0 || ch == '-' || ch == ':' || ch == '[' || ch == ']')
      continue;
    if (ch == '.') {
      has_dot = true;
      continue;
    }
    return false;
  }
  return has_dot && has_alpha;
}

std::string normalize_url(std::string s) {
  s = trim_copy(std::move(s));
  while (s.size() >= 2) {
    const char a = s.front();
    const char b = s.back();
    const bool match_double = (a == '"') && (b == '"');
    const bool match_single = (a == '\'') && (b == '\'');
    if (!match_double && !match_single)
      break;
    s = trim_copy(s.substr(1, s.size() - 2));
  }
  if (looks_like_url_without_scheme(s)) {
    s = "https://" + s;
  }
  return s;
}

std::vector<std::string> split_drop_payload(const std::string& raw) {
  std::vector<std::string> out;
  std::string cur;
  cur.reserve(raw.size());
  for (char ch : raw) {
    if (ch == '\r')
      continue;
    if (ch == '\n') {
      auto t = trim_copy(cur);
      if (!t.empty())
        out.emplace_back(std::move(t));
      cur.clear();
      continue;
    }
    cur.push_back(ch);
  }
  auto t = trim_copy(cur);
  if (!t.empty())
    out.emplace_back(std::move(t));
  return out;
}

std::string lowercase_ascii(std::string s) {
  for (char& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

bool starts_with_http_scheme(const std::string& s) {
  const std::string lower = lowercase_ascii(s);
  return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0;
}

std::string url_path_without_query_or_fragment(const std::string& s) {
  const std::size_t query_pos = s.find_first_of("?#");
  if (query_pos == std::string::npos) {
    return s;
  }
  return s.substr(0, query_pos);
}

bool has_any_suffix(const std::string& s, const std::vector<std::string>& suffixes) {
  for (const std::string& suffix : suffixes) {
    if (s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0) {
      return true;
    }
  }
  return false;
}

bool looks_like_direct_media_url(const std::string& s) {
  const std::string path = lowercase_ascii(url_path_without_query_or_fragment(s));
  static const std::vector<std::string> kMediaSuffixes = {
      ".mp4", ".m4v", ".mkv", ".webm", ".mov", ".avi", ".m3u8", ".mpd", ".mp3", ".m4a", ".aac", ".ogg", ".opus",
      ".flac", ".wav"};
  return has_any_suffix(path, kMediaSuffixes);
}

bool should_resolve_with_ytdlp(const std::string& s) {
  if (!starts_with_http_scheme(s)) {
    return false;
  }
  return !looks_like_direct_media_url(s);
}

std::string shell_quote(const std::string& value) {
#if defined(_WIN32)
  std::string out = "\"";
  for (char ch : value) {
    if (ch == '"') {
      out += "\"\"";
    } else {
      out.push_back(ch);
    }
  }
  out.push_back('"');
  return out;
#else
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    } else {
      out.push_back(ch);
    }
  }
  out.push_back('\'');
  return out;
#endif
}

std::string ytdlp_executable() {
  std::vector<std::filesystem::path> candidates;
  const char* env_path = std::getenv("F8_IMPLAYER_YTDLP");
  if (env_path != nullptr && env_path[0] != '\0') {
    candidates.emplace_back(env_path);
  }

  const char* base_path_raw = SDL_GetBasePath();
  if (base_path_raw != nullptr) {
    const std::filesystem::path base_path(base_path_raw);
    std::filesystem::path current = base_path;
    while (!current.empty()) {
#if defined(_WIN32)
      candidates.push_back(current / ".pixi" / "envs" / "cpp" / "Scripts" / "yt-dlp.exe");
      candidates.push_back(current / ".pixi" / "envs" / "cpp" / "bin" / "yt-dlp.exe");
#else
      candidates.push_back(current / ".pixi" / "envs" / "cpp" / "bin" / "yt-dlp");
#endif
      const std::filesystem::path parent = current.parent_path();
      if (parent == current) {
        break;
      }
      current = parent;
    }
#if defined(_WIN32)
    candidates.push_back(base_path / "yt-dlp.exe");
#else
    candidates.push_back(base_path / "yt-dlp");
#endif
  }
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) {
      return candidate.string();
    }
  }
#if defined(_WIN32)
  return "yt-dlp.exe";
#else
  return "yt-dlp";
#endif
}

bool run_command_output(const std::string& command, std::string& output) {
  FILE* pipe = F8_POPEN(command.c_str(), "r");
  if (pipe == nullptr) {
    return false;
  }

  std::array<char, 4096> buffer{};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output.append(buffer.data());
  }
  const int rc = F8_PCLOSE(pipe);
  if (rc != 0) {
    return false;
  }
  return true;
}

std::string ytdlp_browser_cookie_value(const YtdlpAuthOptions& auth) {
  if (auth.browser_profile.empty()) {
    return auth.browser;
  }
  return auth.browser + ":" + auth.browser_profile;
}

void append_ytdlp_headers(const json& headers, std::vector<std::string>& out) {
  if (!headers.is_object()) {
    return;
  }
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    if (it.key().empty() || !it.value().is_string()) {
      continue;
    }
    const std::string value = it.value().get<std::string>();
    if (!value.empty()) {
      out.push_back(it.key() + ": " + value);
    }
  }
}

bool contains_header_name(const std::vector<std::string>& headers, const std::string& wanted_name) {
  const std::string wanted = lowercase_ascii(wanted_name);
  for (const std::string& header : headers) {
    const std::size_t colon = header.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    if (lowercase_ascii(header.substr(0, colon)) == wanted) {
      return true;
    }
  }
  return false;
}

bool load_resolved_from_json(const json& payload, ResolvedMediaSource& resolved, std::string& err) {
  if (payload.contains("format_id") && payload["format_id"].is_string()) {
    resolved.format_id = payload["format_id"].get<std::string>();
  }
  if (payload.contains("protocol") && payload["protocol"].is_string()) {
    resolved.protocol = payload["protocol"].get<std::string>();
  }
  if (payload.contains("extractor") && payload["extractor"].is_string()) {
    resolved.extractor = payload["extractor"].get<std::string>();
  }

  const json* first_format = nullptr;
  const json* second_format = nullptr;
  if (payload.contains("requested_formats") && payload["requested_formats"].is_array()) {
    const json& requested_formats = payload["requested_formats"];
    if (!requested_formats.empty() && requested_formats[0].is_object()) {
      first_format = &requested_formats[0];
    }
    if (requested_formats.size() >= 2 && requested_formats[1].is_object()) {
      second_format = &requested_formats[1];
    }
  }

  if (first_format != nullptr && first_format->contains("url") && (*first_format)["url"].is_string()) {
    resolved.video_url = (*first_format)["url"].get<std::string>();
    if (first_format->contains("http_headers")) {
      append_ytdlp_headers((*first_format)["http_headers"], resolved.http_headers);
    }
  } else if (payload.contains("url") && payload["url"].is_string()) {
    resolved.video_url = payload["url"].get<std::string>();
    if (payload.contains("http_headers")) {
      append_ytdlp_headers(payload["http_headers"], resolved.http_headers);
    }
  }

  if (second_format != nullptr && second_format->contains("url") && (*second_format)["url"].is_string()) {
    resolved.audio_url = (*second_format)["url"].get<std::string>();
    if (second_format->contains("http_headers") && resolved.http_headers.empty()) {
      append_ytdlp_headers((*second_format)["http_headers"], resolved.http_headers);
    }
  }
  if (payload.contains("webpage_url") && payload["webpage_url"].is_string() &&
      !contains_header_name(resolved.http_headers, "referer")) {
    resolved.http_headers.push_back("Referer: " + payload["webpage_url"].get<std::string>());
  }

  if (resolved.video_url.empty()) {
    err = "yt-dlp resolved no playable media URL";
    return false;
  }
  return true;
}

bool resolve_ytdlp_media(const std::string& url, const YtdlpAuthOptions& auth, ResolvedMediaSource& resolved,
                         std::string& err) {
  const std::string exe = ytdlp_executable();
  const std::string format = "bestvideo[height<=2160]+bestaudio/bestvideo+bestaudio/best";
  const std::string stderr_sink =
#if defined(_WIN32)
      " 2>NUL";
#else
      " 2>/dev/null";
#endif
  std::string command = shell_quote(exe) + " --no-playlist --no-warnings -f " + shell_quote(format) + " -J ";
  if (auth.mode == "browser") {
    command += "--cookies-from-browser " + shell_quote(ytdlp_browser_cookie_value(auth)) + " ";
  } else if (auth.mode == "cookiesFile") {
    command += "--cookies " + shell_quote(auth.cookies_file) + " ";
  }
  command += shell_quote(url) + stderr_sink;
  std::string output;
  if (!run_command_output(command, output) || output.empty()) {
    err = "yt-dlp failed to resolve media URL";
    return false;
  }
  try {
    const json payload = json::parse(output);
    if (!load_resolved_from_json(payload, resolved, err)) {
      return false;
    }
    spdlog::info("yt-dlp resolved media extractor={} format={} protocol={} audio={} headers={} exe={}",
                 resolved.extractor.empty() ? "unknown" : resolved.extractor,
                 resolved.format_id.empty() ? "unknown" : resolved.format_id,
                 resolved.protocol.empty() ? "unknown" : resolved.protocol, resolved.audio_url.empty() ? "no" : "yes",
                 resolved.http_headers.size(), exe);
    return true;
  } catch (const json::exception& exc) {
    err = std::string("yt-dlp returned invalid JSON: ") + exc.what();
    return false;
  }
}

bool parse_auth_mode(const std::string& raw, std::string& normalized_mode) {
  const std::string mode = lowercase_ascii(trim_copy(raw));
  if (mode == "none") {
    normalized_mode = "none";
    return true;
  }
  if (mode == "browser") {
    normalized_mode = "browser";
    return true;
  }
  if (mode == "cookiesfile") {
    normalized_mode = "cookiesFile";
    return true;
  }
  return false;
}

bool is_supported_auth_mode(const std::string& mode) {
  std::string normalized;
  return parse_auth_mode(mode, normalized);
}

bool is_supported_auth_browser(const std::string& browser) {
  return browser == "chrome" || browser == "chromium" || browser == "edge" || browser == "firefox" ||
         browser == "safari";
}

bool is_sensitive_auth_field(const std::string& field) {
  return field == "authBrowserProfile" || field == "authCookiesFile";
}

bool is_profile_value_safe(const std::string& profile) {
  // ytdl-raw-options uses comma as option separator; disallow commas/newlines to
  // keep parsing deterministic.
  return profile.find(',') == std::string::npos && profile.find('\n') == std::string::npos &&
         profile.find('\r') == std::string::npos;
}


}  // namespace f8::implayer
