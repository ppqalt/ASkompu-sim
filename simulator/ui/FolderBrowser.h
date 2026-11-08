#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace simulator::desktop {
// Filesystem navigation kept separate from the GUI so failed paths cannot leave
// an old folder selectable and permission errors stay recoverable.
class FolderBrowser {
 public:
  bool open(const std::filesystem::path& path);
  void showHidden(bool value);
  bool showingHidden() const { return hidden_; }
  bool valid() const { return !path_.empty(); }
  bool hasCore() const;
  const std::filesystem::path& path() const { return path_; }
  const std::vector<std::filesystem::path>& folders() const { return folders_; }
  const std::string& error() const { return error_; }
 private:
  std::filesystem::path path_;
  std::vector<std::filesystem::path> folders_;
  std::string error_;
  bool hidden_ = false;
};
}  // namespace simulator::desktop
