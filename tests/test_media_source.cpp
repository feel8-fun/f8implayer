#include <gtest/gtest.h>
#include "media_source.h"

using namespace f8::implayer;

TEST(MediaSource, NormalizesWebUrlsAndPreservesLocalPaths) {
  EXPECT_EQ(normalize_url("  \"example.com/watch?v=1\"  "), "https://example.com/watch?v=1");
  EXPECT_EQ(normalize_url("C:/videos/demo.mp4"), "C:/videos/demo.mp4");
  EXPECT_FALSE(should_resolve_with_ytdlp("https://example.com/demo.mp4?token=abc"));
  EXPECT_TRUE(should_resolve_with_ytdlp("https://example.com/watch?v=1"));
}

TEST(MediaSource, ResolvesSeparateTracksAndPreservesHeaders) {
  const nlohmann::json payload = {
    {"requested_formats", {{{"url", "https://media/video"}, {"http_headers", {{"Authorization", "Bearer token"}}}},
                            {{"url", "https://media/audio"}}}},
    {"webpage_url", "https://example.com/watch"}
  };
  ResolvedMediaSource result;
  std::string error;
  ASSERT_TRUE(load_resolved_from_json(payload, result, error));
  EXPECT_EQ(result.video_url, "https://media/video");
  EXPECT_EQ(result.audio_url, "https://media/audio");
  EXPECT_EQ(result.http_headers.size(), 2u);
  EXPECT_EQ(result.http_headers[1], "Referer: https://example.com/watch");
}

TEST(MediaSource, RejectsUnplayableResultsAndInvalidAuthModes) {
  ResolvedMediaSource result;
  std::string error;
  EXPECT_FALSE(load_resolved_from_json(nlohmann::json::object(), result, error));
  EXPECT_FALSE(error.empty());
  std::string mode;
  EXPECT_TRUE(parse_auth_mode(" CookiesFile ", mode));
  EXPECT_EQ(mode, "cookiesFile");
  EXPECT_FALSE(parse_auth_mode("guess", mode));
  EXPECT_FALSE(is_profile_value_safe("profile,injected=value"));
}
