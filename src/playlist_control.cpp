#include "implayer_service.h"
#include "media_source.h"
#include <mutex>
#include <unordered_set>
#include <utility>
namespace f8::implayer {
void ImPlayerService::playlist_add(const std::vector<std::string>& items, bool play_if_idle) {
  if (items.empty())
    return;

  std::string url_to_open;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    const std::string current_url = normalize_url(media_url_);

    std::unordered_set<std::string> seen;
    std::vector<std::string> next_playlist;
    next_playlist.reserve(playlist_.size() + items.size());

    for (const auto& existing : playlist_) {
      const std::string u = normalize_url(existing);
      if (u.empty())
        continue;
      if (!seen.insert(u).second)
        continue;
      next_playlist.push_back(u);
    }

    const bool empty_before = next_playlist.empty();
    for (const auto& raw : items) {
      const std::string u = normalize_url(raw);
      if (u.empty())
        continue;
      if (!seen.insert(u).second)
        continue;
      next_playlist.push_back(u);
    }

    playlist_ = std::move(next_playlist);
    if (playlist_.empty())
      return;

    if (!current_url.empty()) {
      for (std::size_t i = 0; i < playlist_.size(); ++i) {
        if (playlist_[i] == current_url) {
          playlist_index_ = static_cast<int>(i);
          break;
        }
      }
    }
    if (empty_before) {
      playlist_index_ = 0;
      url_to_open = playlist_[0];
    } else if (play_if_idle && playlist_index_ < 0) {
      playlist_index_ = 0;
      url_to_open = playlist_[0];
    } else if (play_if_idle && playlist_index_ >= static_cast<int>(playlist_.size())) {
      playlist_index_ = 0;
      url_to_open = playlist_[0];
    }
  }

  if (!url_to_open.empty()) {
    std::string err;
    if (play_if_idle) {
      set_playback_intent(PlaybackIntent::Playing);
    }
    (void)open_media_internal(url_to_open, true, err);
  }
}

void ImPlayerService::playlist_play_index(int index) {
  std::string url;
  int previous_index = -1;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    if (index < 0 || index >= static_cast<int>(playlist_.size()))
      return;
    previous_index = playlist_index_;
    playlist_index_ = index;
    url = playlist_[static_cast<std::size_t>(playlist_index_)];
  }
  std::string err;
  if (!open_media_internal(url, true, err)) {
    std::lock_guard<std::mutex> lock(state_mu_);
    playlist_index_ = previous_index;
    last_error_ = "Playlist: " + err;
  }
}

void ImPlayerService::playlist_remove_index(int index) {
  std::lock_guard<std::mutex> lock(state_mu_);
  if (index < 0 || index >= static_cast<int>(playlist_.size())) {
    return;
  }

  const std::size_t i = static_cast<std::size_t>(index);
  playlist_.erase(playlist_.begin() + static_cast<std::ptrdiff_t>(i));
  if (playlist_.empty()) {
    playlist_index_ = -1;
    return;
  }

  if (playlist_index_ == index) {
    // The currently-highlighted item was removed. Keep playback state unchanged and
    // force the selection to "none" to avoid highlighting the wrong item.
    playlist_index_ = -1;
    return;
  }
  if (playlist_index_ > index) {
    playlist_index_ -= 1;
  }
}

void ImPlayerService::playlist_clear() {
  std::lock_guard<std::mutex> lock(state_mu_);
  playlist_.clear();
  playlist_index_ = -1;
}

void ImPlayerService::playlist_next() {
  std::string url;
  bool loop = false;
  int previous_index = -1;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    if (playlist_.empty())
      return;
    previous_index = playlist_index_;
    const int next = playlist_index_ < 0 ? 0 : playlist_index_ + 1;
    loop = loop_;
    if (next >= static_cast<int>(playlist_.size())) {
      if (!loop)
        return;
      playlist_index_ = 0;
      url = playlist_[0];
    } else {
      playlist_index_ = next;
      url = playlist_[static_cast<std::size_t>(playlist_index_)];
    }
  }
  std::string err;
  if (!open_media_internal(url, true, err)) {
    std::lock_guard<std::mutex> lock(state_mu_);
    playlist_index_ = previous_index;
    last_error_ = "Next: " + err;
  }
}

void ImPlayerService::playlist_prev() {
  std::string url;
  int previous_index = -1;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    if (playlist_.empty())
      return;
    if (playlist_index_ <= 0)
      return;
    previous_index = playlist_index_;
    playlist_index_ -= 1;
    url = playlist_[static_cast<std::size_t>(playlist_index_)];
  }
  std::string err;
  if (!open_media_internal(url, true, err)) {
    std::lock_guard<std::mutex> lock(state_mu_);
    playlist_index_ = previous_index;
    last_error_ = "Previous: " + err;
  }
}


}  // namespace f8::implayer
