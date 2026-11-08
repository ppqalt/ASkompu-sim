#pragma once
#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "imgui.h"
#include "CoreBuildInfo.h"
#include "FolderBrowser.h"

namespace simulator::desktop {
struct CoreVersion { std::string kind,label,sha,date,subject,body,tag; };
// Tämä asiakas käynnistää yhden backend-prosessin ja lukee sen tilan.
// Git-, CMake-, testaus- tai restart-logiikkaa ei toteuteta uudelleen GUI:ssa.
class CoreVersionView {
 public:
  CoreVersionView(std::string home = {}, std::string upstream = {}, std::string local = {});
  ~CoreVersionView();
  CoreVersionView(const CoreVersionView&) = delete;
  CoreVersionView& operator=(const CoreVersionView&) = delete;
  void render();
  void poll();
  bool closeRequested() const { return closeRequested_; }
  bool busy() const { return running_; }
  void cancel();
  const std::map<std::string,std::pair<ImVec2,ImVec2>>& hitRegions() const { return hits_; }
  const std::string& phase() const { return phase_; }
  const std::string& operation() const { return operation_; }
  bool localSourceReady() const { return localChecked_; }
  const std::string& localSourcePath() const { return checkedLocalPath_; }
  bool folderSourceReady() const { return !folderPathEdited_&&browser_.hasCore(); }
 private:
  void start(const std::string& operation, const std::vector<std::string>& extra = {});
  bool spawn(const std::vector<std::string>& arguments);
  void readResult();
  bool button(const char* name, bool primary = false, ImVec2 size = {0,0});
  void activeCard();
  void selectionCard();
  void progressCard();
  float versionList();
  void requirementsCard();
  void localCard();
  void folderPicker();
  void navigateFolder(const std::filesystem::path& path);
  void saveLocalPreference();
  bool readyMatchesSelection() const;
  std::filesystem::path home_,script_,bundle_,executable_,job_;
  std::string upstream_,operation_,phase_="idle",message_,error_,log_,previous_,readyWarning_;
  CoreBuildInfo ready_;
  std::vector<CoreVersion> versions_;
  std::vector<std::pair<std::string,bool>> requirements_;
  CoreVersion selected_;
  std::map<std::string,std::string> state_;
  std::map<std::string,std::pair<ImVec2,ImVec2>> hits_;
  char exactSha_[96] = {};
  char filter_[160] = {};
  char localPath_[2048] = {};
  char folderPath_[2048] = {};
  std::string checkedLocalPath_,localSubject_,localSha_,localFingerprint_;
  FolderBrowser browser_;
  std::filesystem::path folderHome_;
  int category_ = 0;
  int sourceMode_ = 1, profile_ = 1;
  bool localChecked_ = false,localDirty_ = false,localHasGit_ = false,folderOpen_ = false;
  bool folderPathEdited_ = false;
  bool scrollTop_ = false;
  bool initialized_ = false, running_ = false, requirementsOk_ = false, closeRequested_ = false;
  bool restartPending_ = false, rollbackPending_ = false;
  bool readyExpanded_ = true;
  bool readyIsActive_ = false;
  std::chrono::steady_clock::time_point began_,lastRead_;
#ifdef _WIN32
  void* process_ = nullptr;
#else
  int process_ = -1;
#endif
};
} // namespace simulator::desktop
