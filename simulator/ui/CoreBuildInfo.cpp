#include "CoreBuildInfo.h"

namespace simulator::desktop {
namespace {
std::string value(const std::map<std::string,std::string>& fields,const std::string& key){
  const auto found=fields.find(key);return found==fields.end()?std::string{}:found->second;
}
}
CoreBuildInfo readCoreBuildInfo(const std::map<std::string,std::string>& fields,bool doctor){
  const std::string prefix=doctor?"ready_":"";
  CoreBuildInfo result;
  result.artifact=value(fields,"artifact");
  result.kind=value(fields,doctor?"ready_source_kind":"source_kind");
  result.path=value(fields,prefix+"local_path");result.sha=value(fields,prefix+"sha");
  result.fingerprint=value(fields,prefix+"fingerprint");
  result.profile=value(fields,prefix+"profile");result.date=value(fields,prefix+"date");
  result.executable=value(fields,prefix+"executable");
  const auto tag=value(fields,prefix+"tag");
  result.label=result.kind=="local"?"Paikallinen ASkompu":
      tag.empty()?result.sha.substr(0,10):tag;
  return result;
}
bool matchesLocalBuild(const CoreBuildInfo& built,const std::string& path,
                       const std::string& fingerprint,const std::string& profile){
  return built.present()&&built.kind=="local"&&!path.empty()&&!fingerprint.empty()&&
      built.path==path&&built.fingerprint==fingerprint&&built.profile==profile;
}
bool matchesGitBuild(const CoreBuildInfo& built,const std::string& sha,const std::string& profile){
  return built.present()&&built.kind=="github"&&!sha.empty()&&built.sha==sha&&built.profile==profile;
}
int buildStageIndex(const std::string& phase){
  if(phase=="ready")return static_cast<int>(buildStages.size());
  if(phase=="listing")return 0;
  for(size_t index=0;index<buildStages.size();++index)
    if(phase==buildStages[index].phase)return static_cast<int>(index);
  return -1;
}
}  // namespace simulator::desktop
