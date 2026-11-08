#include "FolderBrowser.h"

#include <algorithm>
#include <system_error>

namespace simulator::desktop {
namespace fs=std::filesystem;

bool FolderBrowser::open(const fs::path& requested) {
  path_.clear();folders_.clear();error_.clear();
  std::error_code ec;
  if(requested.empty()) {error_="Anna kansion polku.";return false;}
  const auto absolute=fs::absolute(requested,ec).lexically_normal();
  if(ec||!fs::is_directory(absolute,ec)||ec) {
    error_="Kansiota ei löydy tai siihen ei ole lukuoikeutta.";
    return false;
  }
  fs::directory_iterator it(absolute,ec),end;
  if(ec){error_="Kansion sisältöä ei voi lukea: "+ec.message();return false;}
  for(;it!=end;it.increment(ec)) {
    if(ec)break;
    std::error_code typeError;
    const auto name=it->path().filename().u8string();
    if(it->is_directory(typeError)&&!name.empty()&&(hidden_||name.front()!='.'))
      folders_.push_back(it->path());
  }
  if(ec){folders_.clear();error_="Kansion sisältöä ei voi lukea: "+ec.message();return false;}
  std::sort(folders_.begin(),folders_.end());path_=absolute;
  return true;
}

void FolderBrowser::showHidden(bool value) {
  if(hidden_==value)return;
  hidden_=value;
  if(valid()){const auto current=path_;open(current);}
}

bool FolderBrowser::hasCore() const {
  if(!valid())return false;
  std::error_code ec;
  return fs::is_regular_file(path_/"src/core/ApplicationCore.cpp",ec)&&!ec&&
      fs::is_directory(path_/"include",ec)&&!ec;
}
}  // namespace simulator::desktop
