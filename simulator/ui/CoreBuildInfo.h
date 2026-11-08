#pragma once

#include <array>
#include <map>
#include <string>

namespace simulator::desktop {
struct CoreBuildInfo {
  std::string artifact,kind,path,sha,fingerprint,profile,date,label,executable;
  bool present() const { return !artifact.empty(); }
};
CoreBuildInfo readCoreBuildInfo(const std::map<std::string,std::string>& fields,
                              bool doctor);
bool matchesLocalBuild(const CoreBuildInfo& built,const std::string& path,
                       const std::string& fingerprint,const std::string& profile);
bool matchesGitBuild(const CoreBuildInfo& built,const std::string& sha,
                     const std::string& profile);

struct BuildStage { const char* phase;const char* label; };
inline constexpr std::array<BuildStage,6> buildStages{{
  {"downloading","Lähteet"},{"configuring","Valmistelu"},
  {"building","Rakennus"},{"testing","Testit"},
  {"installing","Paketti"},{"validating","Käynnistys"}
}};
int buildStageIndex(const std::string& phase);
}  // namespace simulator::desktop
