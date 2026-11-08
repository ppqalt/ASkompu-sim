#include "CoreVersionView.h"
#include "BuildInformation.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace simulator::desktop {
namespace {
namespace fs=std::filesystem;
const ImVec4 accent{.32f,.84f,.69f,1},muted{.54f,.62f,.66f,1},amber{1,.69f,.35f,1},red{1,.43f,.43f,1};
float u(){return ImGui::GetFontSize()/17.f;}
void gap(float height=8){ImGui::Dummy({0,height*u()});}
std::string decoded(const std::string& input){
  std::string result;
  for(size_t i=0;i<input.size();++i){
    if(input[i]=='%'&&i+2<input.size()){
      const auto hex=input.substr(i+1,2);char* end=nullptr;const auto value=std::strtol(hex.c_str(),&end,16);
      if(end==hex.c_str()+2){result+=static_cast<char>(value);i+=2;continue;}
    }
    result+=input[i];
  }
  return result;
}
std::vector<std::vector<std::string>> rows(const fs::path& path){
  std::ifstream file(path);std::vector<std::vector<std::string>> result;std::string line;
  while(std::getline(file,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    std::vector<std::string> values;size_t start=0;
    for(;;){const auto end=line.find('\t',start);values.push_back(decoded(line.substr(start,end-start)));if(end==std::string::npos)break;start=end+1;}
    result.push_back(std::move(values));
  }
  return result;
}
std::string tail(const fs::path& path){
  std::ifstream stream(path,std::ios::binary);if(!stream)return {};
  stream.seekg(0,std::ios::end);const auto size=stream.tellg();
  stream.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-48000));
  std::ostringstream text;text<<stream.rdbuf();return text.str();
}
void text(const std::string& value){ImGui::TextWrapped("%s",value.c_str());}
void caption(const char* value){ImGui::TextColored(muted,"%s",value);}
std::string brief(const std::string& sha){return sha.substr(0,10);}
void shaRow(const std::string& sha){
  ImGui::TextColored(muted,"%s",sha.empty()?"SHA ei vielä tiedossa":sha.c_str());
  if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);ImGui::TextUnformatted("Napsauta kopioidaksesi koko SHA:n");ImGui::PopTextWrapPos();ImGui::EndTooltip();}
  if(ImGui::IsItemClicked()&&!sha.empty())ImGui::SetClipboardText(sha.c_str());
}
#ifdef _WIN32
std::wstring wide(const std::string& value){
  const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0);
  if(length==0)return {};
  std::wstring result(static_cast<size_t>(length),L'\0');
  MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,result.data(),length);result.pop_back();return result;
}
std::wstring quoted(const std::string& value){
  std::wstring out=L"\"";unsigned slashes=0;
  for(wchar_t c:wide(value)){
    if(c==L'\\'){++slashes;continue;}
    out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;
  }
  out.append(slashes*2,L'\\');return out+L'"';
}
#endif
}

CoreVersionView::CoreVersionView(std::string home,std::string upstream,std::string local):upstream_(std::move(upstream)){
  char* base=SDL_GetBasePath();fs::path directory=fs::u8path(base?base:"");SDL_free(base);
  // SDL includes a trailing separator; strip it before finding the install prefix.
  if(directory.filename().empty())directory=directory.parent_path();
#ifdef _WIN32
  executable_=directory/"askompu-simulaattori.exe";
#else
  executable_=directory/"askompu-simulaattori";
#endif
  if(home.empty()){
    char* pref=SDL_GetPrefPath("ASkompu","Simulator");
    if(pref){home_=fs::u8path(pref)/"local-core-workflow";SDL_free(pref);}
  }else home_=fs::u8path(home);
  for(const auto& root:{directory,directory.parent_path(),directory.parent_path()/"share/askompu-simulaattori"}){
    if(fs::is_regular_file(root/"tools/core_versions.py")&&fs::is_regular_file(root/"simulator-source.zip")){
      script_=root/"tools/core_versions.py";bundle_=root/"simulator-source.zip";break;
    }
  }
  std::ifstream preference(home_/"local-source.txt");
  std::string remembered;
  std::getline(preference,remembered);
  profile_=ASKOMPU_DISPLAY_WIDTH==320?0:1;
  int rememberedProfile=profile_;
  if(preference>>rememberedProfile)profile_=rememberedProfile==0?0:1;
  const std::string localSource=!local.empty()?local:
      std::string(build::sourceKind)=="local"?build::sourcePath:remembered;
  if(!localSource.empty())std::snprintf(localPath_,sizeof(localPath_),"%s",localSource.c_str());
  const char* userHome=std::getenv("HOME");
#ifdef _WIN32
  if(!userHome)userHome=std::getenv("USERPROFILE");
#endif
  std::error_code homeError;
  folderHome_=userHome?fs::u8path(userHome):fs::current_path(homeError);
  message_="Valitse paikallinen ASkompu-kansio tai GitHub-versio.";
}
CoreVersionView::~CoreVersionView(){
  if(running_)cancel();
#ifdef _WIN32
  if(process_)CloseHandle(process_);
#endif
}
void CoreVersionView::cancel(){
  if(!running_)return;
  std::ofstream file(job_/"cancel");file<<"cancel\n";
  message_="Peruutetaan toimintoa ja suljetaan sen rakennusprosessit…";
}
bool CoreVersionView::spawn(const std::vector<std::string>& arguments){
#ifdef _WIN32
  std::wstring command;
  for(const auto& argument:arguments){if(!command.empty())command+=L' ';command+=quoted(argument);}
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
  HANDLE logFile=CreateFileW((job_/"backend.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&attributes,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(logFile==INVALID_HANDLE_VALUE||input==INVALID_HANDLE_VALUE){
    if(logFile!=INVALID_HANDLE_VALUE)CloseHandle(logFile);
    if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
    return false;
  }
  STARTUPINFOW info{};info.cb=sizeof(info);info.dwFlags=STARTF_USESTDHANDLES;
  info.hStdOutput=logFile;info.hStdError=logFile;info.hStdInput=input;
  PROCESS_INFORMATION pi{};
  const bool started=CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&info,&pi)!=0;
  CloseHandle(logFile);CloseHandle(input);
  if(!started)return false;
  CloseHandle(pi.hThread);process_=pi.hProcess;return true;
#else
  std::vector<char*> argv;for(const auto& argument:arguments)argv.push_back(const_cast<char*>(argument.c_str()));argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  const auto output=(job_/"backend.log").string();
  posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,output.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
  posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
  pid_t pid;const int result=posix_spawnp(&pid,argv[0],&actions,nullptr,argv.data(),environ);
  posix_spawn_file_actions_destroy(&actions);
  if(result!=0)return false;
  process_=pid;return true;
#endif
}
void CoreVersionView::start(const std::string& operation,const std::vector<std::string>& extra){
  if(running_)return;
  error_.clear();state_.clear();log_.clear();operation_=operation;
  scrollTop_=operation=="build"||operation=="launch"||operation=="rollback";
  if(home_.empty()||script_.empty()){
    phase_="failed";error_="Versionvalitsimen lähdepakettia tai käyttäjän työtilaa ei löydy. Pura koko julkaisu uudelleen.";return;
  }
  std::error_code ec;
  job_=home_/"jobs"/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  fs::create_directories(job_,ec);
  if(ec){phase_="failed";error_="Työtilaan ei voi kirjoittaa: "+ec.message();return;}
  std::vector<std::string> command={script_.u8string(),operation,"--home",home_.u8string(),"--job",job_.u8string(),"--bundle",bundle_.u8string()};
  if(!upstream_.empty()){command.push_back("--upstream");command.push_back(upstream_);}
  command.insert(command.end(),extra.begin(),extra.end());
#ifdef _WIN32
  // Prefer the interpreter on PATH: some installations alias py to python.exe,
  // which does not understand the classic launcher's -3 switch.
  for(const std::vector<std::string>& prefix:std::vector<std::vector<std::string>>{{"python"},{"python3"},{"py","-3"}}){
    auto invocation=prefix;invocation.insert(invocation.end(),command.begin(),command.end());
    if(spawn(invocation)){running_=true;break;}
  }
#else
  command.insert(command.begin(),"python3");running_=spawn(command);
#endif
  if(!running_){phase_="failed";error_="Python 3.10+ ei käynnistynyt. Asenna Python ja lisää se PATHiin; simulointi toimii edelleen.";return;}
  phase_="starting";message_="Valmistellaan toimintoa…";began_=std::chrono::steady_clock::now();lastRead_={};
}
void CoreVersionView::readResult(){
  if(operation_=="doctor"){
    requirements_.clear();for(const auto& row:rows(job_/"requirements.tsv"))if(row.size()==2)requirements_.emplace_back(row[1],row[0]=="1");
    requirementsOk_=state_["requirements_ok"]=="1";previous_=state_["previous"];
    ready_=readCoreBuildInfo(state_,true);
    readyWarning_=state_["ready_warning"];
    if(!state_["previous_warning"].empty())readyWarning_+=(readyWarning_.empty()?"":"\n")+state_["previous_warning"];
  }
  if(operation_=="inspect-local"&&phase_=="done"){
    checkedLocalPath_=state_["local_path"];
    std::snprintf(localPath_,sizeof(localPath_),"%s",checkedLocalPath_.c_str());
    localSha_=state_["sha"];localSubject_=state_["subject"];
    localFingerprint_=state_["fingerprint"];localDirty_=state_["dirty"]=="1";localHasGit_=state_["git"]=="1";
    localChecked_=state_["compatibility_ok"]=="1";
    if(localChecked_)saveLocalPreference();
  }
  if(operation_=="list"&&phase_=="done"){
    versions_.clear();for(const auto& row:rows(job_/"versions.tsv"))if(row.size()==7)versions_.push_back({row[0],row[1],row[2],row[3],row[4],row[5],row[6]});
    if(selected_.sha.empty()&&!versions_.empty())selected_=versions_.front();
  }
  if(operation_=="resolve"&&phase_=="done")selected_={"commit",brief(state_["sha"]),state_["sha"],state_["date"],state_["subject"],state_["body"],state_["tag"]};
  if(operation_=="build"&&phase_=="ready"){
    ready_=readCoreBuildInfo(state_,false);readyWarning_.clear();readyExpanded_=true;
    if(ready_.kind=="local"&&ready_.path==checkedLocalPath_){
      localFingerprint_=ready_.fingerprint;localSha_=ready_.sha;
      localDirty_=state_["dirty"]=="1";localHasGit_=state_["git"]=="1";
    }
  }
  if(operation_=="doctor"||(operation_=="build"&&phase_=="ready")){
    std::error_code ec;
    readyIsActive_=ready_.present()&&!ready_.executable.empty()&&
        fs::equivalent(fs::u8path(ready_.executable),executable_,ec)&&!ec;
  }
  if(phase_=="launched")closeRequested_=true;
  if(operation_=="doctor"&&phase_=="done"&&sourceMode_==1&&*localPath_)
    start("inspect-local",{"--local",localPath_});
}
void CoreVersionView::poll(){
  if(!running_)return;
  const auto now=std::chrono::steady_clock::now();
  if(now-lastRead_<std::chrono::milliseconds(100))return;
  lastRead_=now;
  const auto data=rows(job_/"state.tsv");
  if(!data.empty()){
    for(const auto& row:data)if(row.size()==2)state_[row[0]]=row[1];
    phase_=state_["phase"];message_=state_["message"];
  }
  log_=tail(job_/"operation.log");
  const auto startup=tail(job_/"startup.log");if(!startup.empty())log_+="\nKäynnistys:\n"+startup;
  bool exited=false;int code=0;
#ifdef _WIN32
  DWORD result=STILL_ACTIVE;
  if(process_&&GetExitCodeProcess(process_,&result)&&result!=STILL_ACTIVE){exited=true;code=static_cast<int>(result);CloseHandle(process_);process_=nullptr;}
#else
  int status=0;const auto result=waitpid(process_,&status,WNOHANG);
  if(result==process_){exited=true;code=WIFEXITED(status)?WEXITSTATUS(status):1;process_=-1;}
#endif
  if(exited){
    scrollTop_=operation_=="build"||operation_=="launch"||operation_=="rollback"||phase_=="failed";
    // Lue lopputila vielä prosessin päättymisen jälkeen: ensimmäinen luku voi
    // osua viimeisen tilaviestin atomista vaihtoa edeltävään hetkeen.
    for(const auto& row:rows(job_/"state.tsv"))if(row.size()==2)state_[row[0]]=row[1];
    if(!state_.empty()){phase_=state_["phase"];message_=state_["message"];}
    running_=false;
    if(state_.empty()||(code!=0&&phase_!="failed"&&phase_!="cancelled")||
       (phase_!="done"&&phase_!="ready"&&phase_!="failed"&&phase_!="cancelled"&&phase_!="launched")){
      phase_="failed";error_="Taustatyökalu päättyi ilman lopputulosta ("+std::to_string(code)+"). Tarkista Python 3.10+ ja tekninen loki.";log_+=tail(job_/"backend.log");
    }else if(phase_=="failed")error_=message_;
    readResult();
  }
}
bool CoreVersionView::button(const char* name,bool primary,ImVec2 size){
  if(primary){ImGui::PushStyleColor(ImGuiCol_Button,accent);ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.02f,.12f,.10f,1));}
  const bool clicked=ImGui::Button(name,size);hits_[name]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  if(primary)ImGui::PopStyleColor(2);
  return clicked;
}
void CoreVersionView::activeCard(){
  const bool columns=ImGui::GetContentRegionAvail().x>510*u();
  if(columns){ImGui::BeginTable("Nykyiset versiot",2);ImGui::TableSetupColumn("Ydin",ImGuiTableColumnFlags_WidthStretch,2.f);ImGui::TableSetupColumn("Sovellus",ImGuiTableColumnFlags_WidthStretch,1.f);ImGui::TableNextColumn();}
  caption("KÄYTÖSSÄ");
  if(std::string(build::sourceKind)=="local"){
    ImGui::TextColored(accent,"Paikallinen ASkompu");text(build::sourcePath);
  }
  ImGui::TextColored(accent,"%.10s",build::coreRevision);ImGui::SameLine();
  text(std::string(*build::coreTag?build::coreTag:"Ei tagia")+"  ·  "+std::string(build::coreDate).substr(0,10));
  ImGui::TextColored(std::string(build::coreWorktree)=="puhdas"?muted:amber,"%s",build::coreWorktree);
  ImGui::TextColored(muted,"Näyttö %u × %u",ASKOMPU_DISPLAY_WIDTH,ASKOMPU_DISPLAY_HEIGHT);
  if(ImGui::TreeNode("Version tiedot")){shaRow(build::coreRevision);text(build::coreDate);text("Lähdetiiviste: "+std::string(build::coreFingerprint));ImGui::TreePop();}
  if(columns)ImGui::TableNextColumn();
  caption("SIMULAATTORI");
  ImGui::Text("%s  ·  %.10s",build::version,build::revision);
  ImGui::TextColored(std::string(build::worktree)=="puhdas"?muted:amber,"%s",build::worktree);
  if(columns)ImGui::EndTable();
  if(!previous_.empty()){
    gap(4);ImGui::BeginDisabled(running_);
    if(button("Palaa edelliseen ohjelmaan"))rollbackPending_=true;
    ImGui::EndDisabled();
  }
}
void CoreVersionView::requirementsCard(){
  if(ImGui::CollapsingHeader(requirementsOk_?"Rakennustyökalut · valmiina":"Rakennustyökalut · tarkista vaatimukset",requirementsOk_?0:ImGuiTreeNodeFlags_DefaultOpen)){
    text("Ytimen vaihtaminen kääntää lähdekoodia tällä koneella. Tavallinen simulointi ei tarvitse näitä työkaluja.");
    if(requirements_.empty())text("Automaattinen työkalutarkistus tarvitsee Python 3.10+:n. Puuttuvat työkalut eivät estä tavallista simulointia.");
    for(const auto& item:requirements_){ImGui::TextColored(item.second?accent:amber,"%s",item.second?"OK":"Puuttuu");ImGui::SameLine();text(item.first);}
#ifdef _WIN32
    text("Asenna Python 3.10+, Git for Windows sekä CMake 3.25+ (PATHiin). Visual Studio 2022 / Build Tools tarvitsee C++-työpöytäkehityksen ja Windows SDK:n. Käynnistä simulaattori uudelleen asennuksen jälkeen.");
#else
    text("Tarvitaan Python 3.10+, Git, CMake 3.25+, CTest, GCC C++ ja Make sekä SDL:n X11/Wayland-kehityskirjastot. Ensimmäinen rakennus lataa myös lukitut kirjastolähteet verkosta.");
#endif
    ImGui::BeginDisabled(running_);if(button("Tarkista työkalut uudelleen"))start("doctor");ImGui::EndDisabled();
  }
}
void CoreVersionView::saveLocalPreference(){
  std::error_code ec;fs::create_directories(home_,ec);
  if(!ec){std::ofstream preference(home_/"local-source.txt");preference<<localPath_<<'\n'<<profile_<<'\n';}
}
void CoreVersionView::navigateFolder(const fs::path& path){
  auto requested=path;
  const auto typed=path.u8string();
  if(typed=="~")requested=folderHome_;
  else if(typed.rfind("~/",0)==0||typed.rfind("~\\",0)==0)requested=folderHome_/fs::u8path(typed.substr(2));
  const bool opened=browser_.open(requested);
  std::snprintf(folderPath_,sizeof(folderPath_),"%s",(opened?browser_.path():requested).u8string().c_str());
  folderPathEdited_=false;
}
void CoreVersionView::folderPicker(){
  if(folderOpen_){ImGui::OpenPopup("Valitse ASkompu-kansio");folderOpen_=false;}
  ImGui::SetNextWindowSize({std::min(640*u(),ImGui::GetIO().DisplaySize.x-30*u()),0},ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints({0,0},{FLT_MAX,ImGui::GetIO().DisplaySize.y-30*u()});
  if(!ImGui::BeginPopupModal("Valitse ASkompu-kansio",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
  ImGui::SetNextItemWidth(-1);
  const bool entered=ImGui::InputTextWithHint("##KansiovalitsimenPolku","Polku kansioon",folderPath_,sizeof(folderPath_),ImGuiInputTextFlags_EnterReturnsTrue);
  hits_["Kansion polku"]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  if(ImGui::IsItemEdited())folderPathEdited_=true;
  if(entered)navigateFolder(fs::u8path(folderPath_));
  if(button("Koti"))navigateFolder(folderHome_);
  ImGui::SameLine();ImGui::BeginDisabled(!browser_.valid()||browser_.path()==browser_.path().root_path());
  if(button("Ylempi kansio"))navigateFolder(browser_.path().parent_path());
  ImGui::EndDisabled();ImGui::SameLine();
  if(button("Avaa polku"))navigateFolder(fs::u8path(folderPath_));
#ifdef _WIN32
  if(ImGui::BeginCombo("Asema",browser_.path().root_name().u8string().c_str())){
    for(char drive='A';drive<='Z';++drive){const std::string root=std::string(1,drive)+":\\";std::error_code ec;
      if(fs::is_directory(fs::u8path(root),ec)&&ImGui::Selectable(root.c_str()))navigateFolder(fs::u8path(root));}
    ImGui::EndCombo();
  }
#endif
  bool hidden=browser_.showingHidden();
  if(ImGui::Checkbox("Näytä piilokansiot",&hidden))browser_.showHidden(hidden);
  gap(4);caption("Avaa kansio kaksoisnapsautuksella tai Enterillä");
  fs::path next;
  ImGui::BeginChild("Kansiot",{0,std::min(280*u(),ImGui::GetIO().DisplaySize.y*.32f)},ImGuiChildFlags_Borders);
  if(browser_.folders().empty())caption(browser_.valid()?"Ei alikansioita":"Avaa kansio polulla tai Koti-painikkeella");
  for(const auto& folder:browser_.folders()){
    const auto name=folder.filename().u8string();
    if(ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)&&
        (ImGui::IsMouseDoubleClicked(0)||ImGui::IsKeyPressed(ImGuiKey_Enter,false)))next=folder;
  }
  ImGui::EndChild();
  if(!next.empty())navigateFolder(next);
  if(!browser_.error().empty()){ImGui::PushStyleColor(ImGuiCol_Text,amber);text(browser_.error());ImGui::PopStyleColor();}
  const bool hasCore=!folderPathEdited_&&browser_.hasCore();
  ImGui::TextColored(hasCore?accent:muted,"%s",folderPathEdited_?"Paina Enter tai Avaa polku":
      hasCore?"ASkompu-lähteet löytyivät":"Valitse projektin juuri, jossa on src ja include");
  gap(6);
  if(button("Peruuta kansion valinta"))ImGui::CloseCurrentPopup();
  ImGui::SameLine();ImGui::BeginDisabled(!hasCore);
  if(button("Käytä tätä kansiota",true)){
    std::snprintf(localPath_,sizeof(localPath_),"%s",browser_.path().u8string().c_str());
    localChecked_=false;start("inspect-local",{"--local",localPath_});ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  if(ImGui::IsKeyPressed(ImGuiKey_Escape,false))ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}
void CoreVersionView::localCard(){
  caption("PAIKALLINEN ASKOMPU");gap(4);
  text("Valitse lähdekansio. Myös tallennetut, committoimattomat muutokset tulevat mukaan.");gap(4);
  ImGui::BeginDisabled(running_);
  ImGui::SetNextItemWidth(-1);
  const bool inspectEntered=ImGui::InputTextWithHint("##PaikallinenPolku","ASkompu-projektin kansio",localPath_,sizeof(localPath_),ImGuiInputTextFlags_EnterReturnsTrue);
  if(ImGui::IsItemEdited())localChecked_=false;
  hits_["Paikallinen polku"]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  if(inspectEntered&&*localPath_){localChecked_=false;start("inspect-local",{"--local",localPath_});}
  if(button("Valitse kansio…")){
    navigateFolder(*localPath_?fs::u8path(localPath_):folderHome_);folderOpen_=true;
  }
  ImGui::SameLine();ImGui::BeginDisabled(!*localPath_);
  if(button("Tarkista kansio")){localChecked_=false;start("inspect-local",{"--local",localPath_});}
  ImGui::EndDisabled();ImGui::EndDisabled();
  if(localChecked_){
    gap(8);ImGui::TextColored(accent,"Kansio valmis kokeiltavaksi");
    text(fs::u8path(checkedLocalPath_).filename().u8string());
    ImGui::TextColored(localDirty_?amber:muted,"%s",!localHasGit_?"Lähdekansio ilman Git-tietoja":
        localDirty_?"Sisältää paikallisia muutoksia":"Ei committoimattomia muutoksia");
    if(ImGui::TreeNode("Paikallisen version tiedot")){text(checkedLocalPath_);text(localSubject_);shaRow(localSha_);text("Lähdetiiviste: "+localFingerprint_);ImGui::TreePop();}
  }
  gap(8);ImGui::BeginDisabled(running_);
  caption("NÄYTTÖKOKO");const char* profiles[]={"320 × 170","480 × 320"};
  ImGui::SetNextItemWidth(180*u());if(ImGui::Combo("##PaikallinenNäyttö",&profile_,profiles,2))saveLocalPreference();
  ImGui::EndDisabled();
  gap(6);ImGui::BeginDisabled(running_||!requirementsOk_||!localChecked_);
  if(button("Rakenna ja testaa",true,{-1,44*u()})){
    readyExpanded_=true;saveLocalPreference();
    start("build",{"--local",checkedLocalPath_,"--profile",profile_==0?"320x170":"480x320"});
  }
  ImGui::EndDisabled();
  caption("Rakennus tehdään erikseen. Lähdekansiota ei muuteta.");
  if(!requirementsOk_)caption("Tarkista rakennustyökalut ennen rakentamista.");
}
float CoreVersionView::versionList(){
  caption("VALITSE LÄHDEVERSIO");gap(5);
  const float listTop=ImGui::GetCursorScreenPos().y;
  const char* categories[]={"Kaikki","Main","Tagit","Commitit"};
  ImGui::SetNextItemWidth(130*u());ImGui::Combo("##Rajaus",&category_,categories,4);ImGui::SameLine();
  ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##Haku","Suodata viestiä, tagia tai SHA:ta",filter_,sizeof(filter_));
  ImGui::BeginChild("Versiolista",{0,versions_.empty()?150*u():std::clamp(ImGui::GetIO().DisplaySize.y*.32f,190*u(),360*u())},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
  size_t count=0;
  for(size_t i=0;i<versions_.size();++i){
    const auto& version=versions_[i];
    if(category_!=0&&version.kind!=(category_==1?"main":category_==2?"tag":"commit"))continue;
    if(*filter_&&(version.subject+version.label+version.sha).find(filter_)==std::string::npos)continue;
    ++count;ImGui::PushID(static_cast<int>(i));
    const auto pos=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    const auto messageSize=ImGui::CalcTextSize(version.subject.c_str(),nullptr,false,width-24*u());
    const float height=std::min(messageSize.y,ImGui::GetTextLineHeight()*2)+44*u();
    ImGui::BeginDisabled(running_);
    if(ImGui::Selectable("##versio",selected_.sha==version.sha&&selected_.kind==version.kind,0,{0,height}))selected_=version;
    hits_["Versio "+std::to_string(i)]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);text(version.label);text(version.subject);ImGui::PopTextWrapPos();ImGui::EndTooltip();}
    auto* draw=ImGui::GetWindowDrawList();
    const auto title=(version.kind=="main"?"MAIN  ·  ":version.kind=="tag"?"TAGI  ·  ":"")+version.label+"   ·   "+version.date.substr(0,10);
    draw->PushClipRect(pos,{pos.x+width,pos.y+height},true);
    draw->AddText({pos.x+10*u(),pos.y+5*u()},ImGui::ColorConvertFloat4ToU32(version.kind=="main"?accent:muted),title.c_str());
    draw->PushClipRect({pos.x+10*u(),pos.y+28*u()},{pos.x+width-10*u(),pos.y+height-5*u()},true);
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{pos.x+10*u(),pos.y+28*u()},ImGui::GetColorU32(ImGuiCol_Text),version.subject.c_str(),nullptr,width-24*u());
    draw->PopClipRect();draw->PopClipRect();ImGui::PopID();
  }
  if(count==0){gap(20);caption(versions_.empty()?"Versiolistaa ei ole vielä haettu.":"Hakuehdoilla ei löytynyt versioita.");text(versions_.empty()?"Päivitä lista GitHubista tai ratkaise tarkka commit-SHA.":"Kokeile lyhyempää hakua tai valitse Kaikki.");}
  ImGui::EndChild();
  gap(4);
  const bool exactOpen=ImGui::TreeNodeEx("Tarkka commit",ImGuiTreeNodeFlags_SpanAvailWidth);
  hits_["Tarkka commit"]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  if(exactOpen){
  ImGui::BeginDisabled(running_);
  ImGui::SetNextItemWidth(-130*u());ImGui::InputTextWithHint("##TarkkaSHA","40-merkkinen commit-SHA",exactSha_,sizeof(exactSha_));
  ImGui::SameLine();if(button("Ratkaise SHA",false,{120*u(),0}))start("resolve",{"--ref",exactSha_});
  ImGui::EndDisabled();ImGui::TreePop();
  }
  return listTop;
}
void CoreVersionView::selectionCard(){
  caption("VALITTU VERSIO");gap(4);
  if(selected_.sha.empty())text("Ei valittua versiota");
  else{
    ImGui::PushStyleColor(ImGuiCol_Text,accent);text(selected_.label);ImGui::PopStyleColor();text(selected_.subject);gap(3);
    text(selected_.date.substr(0,10)+"  ·  "+brief(selected_.sha));
    if(ImGui::TreeNode("Version tarkat tiedot")){shaRow(selected_.sha);text(selected_.date);ImGui::TreePop();}
    if(!selected_.body.empty()&&ImGui::TreeNode("Commitin koko viesti")){text(selected_.body);ImGui::TreePop();}
  }
  gap(6);ImGui::BeginDisabled(running_);
  caption("NÄYTTÖKOKO");const char* profiles[]={"320 × 170","480 × 320"};
  ImGui::SetNextItemWidth(180*u());if(ImGui::Combo("##GitHubNäyttö",&profile_,profiles,2))saveLocalPreference();
  ImGui::EndDisabled();
  gap(6);ImGui::BeginDisabled(running_||!requirementsOk_||selected_.sha.empty());
  if(button("Rakenna ja testaa",true,{-1,44*u()})){readyExpanded_=true;start("build",{"--ref",selected_.sha,"--profile",profile_==0?"320x170":"480x320"});}
  ImGui::EndDisabled();
  if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);text("Rakennetaan ja testataan erikseen. Nykyinen ohjelma säilyy käytössä.");ImGui::PopTextWrapPos();ImGui::EndTooltip();}
  if(!requirementsOk_)caption("Tarkista rakennustyökalut ennen rakentamista.");
}
bool CoreVersionView::readyMatchesSelection() const {
  const std::string profile=profile_==0?"320x170":"480x320";
  return sourceMode_==1?localChecked_&&matchesLocalBuild(ready_,checkedLocalPath_,localFingerprint_,profile):
      matchesGitBuild(ready_,selected_.sha,profile);
}
void CoreVersionView::progressCard(){
  if(phase_=="idle")return;
  ImGui::BeginChild("Rakennuksen tila",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);
  const bool failed=phase_=="failed",cancelled=phase_=="cancelled";
  const bool current=readyMatchesSelection(),hasReady=ready_.present();
  if(hasReady&&!running_&&!failed&&!cancelled&&!readyExpanded_&&readyWarning_.empty()){
    ImGui::TextColored(accent,"%s",readyIsActive_?"Tämä versio on käytössä":current?"Valittu versio on rakennettu":"Aiempi testattu rakennus");ImGui::SameLine();
    if(button("Näytä rakennus"))readyExpanded_=true;
    ImGui::EndChild();return;
  }
  ImGui::TextColored(failed?red:hasReady&&!running_?accent:muted,"%s",
      running_?operation_=="build"?"Rakennetaan ASkompu-versiota":"TOIMINTO KÄYNNISSÄ":
      failed?"Toiminto ei valmistunut":cancelled?"Toiminto peruutettu":hasReady?"ASkompu-versio valmis":"TILA");
  if(failed||cancelled||running_)text(error_.empty()?message_:error_);
  if(operation_=="build"&&running_){
    if(state_["source_kind"]=="local")text("Paikallinen ASkompu · "+state_["profile"]);
    else if(!state_["sha"].empty())text("GitHub · "+brief(state_["sha"])+" · "+state_["profile"]);
  }
  if(running_&&operation_=="build"){
    const int stage=buildStageIndex(phase_);
    gap(6);const size_t columns=ImGui::GetContentRegionAvail().x>720*u()?6:3;
    for(size_t i=0;i<buildStages.size();++i){
      if(i%columns)ImGui::SameLine(0,12*u());
      ImGui::TextColored(static_cast<int>(i)<=stage?accent:muted,"%s %s",
          static_cast<int>(i)<stage?"+":static_cast<int>(i)==stage?">":"·",buildStages[i].label);
    }
    gap(4);const float fraction=stage<0?0.f:std::min(1.f,(static_cast<float>(stage)+.35f)/buildStages.size());
    ImGui::ProgressBar(fraction,{-1,6*u()},"");
  }
  if(running_){
    const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-began_).count();
    ImGui::TextColored(muted,"%s  %lld:%02lld",(static_cast<int>(ImGui::GetTime()*3)%2)?"·":" ",static_cast<long long>(seconds/60),static_cast<long long>(seconds%60));
    if(button("Peruuta toiminto"))cancel();
  }
  if(hasReady&&!running_){
    gap(6);caption(current&&!failed&&!cancelled?"VALMIS RAKENNUS":"AIEMPI TESTATTU RAKENNUS");
    ImGui::TextColored(accent,"%s",ready_.label.c_str());
    if(!ready_.path.empty())text(ready_.path);
    ImGui::Text("Näyttö %s",ready_.profile.c_str());
    if(!current)caption("Tämä rakennus ei vastaa nykyistä lähde- ja näyttökokovalintaa.");
    if(ImGui::TreeNode("Rakennetun version tiedot")){
      shaRow(ready_.sha);if(!ready_.fingerprint.empty())text("Lähdetiiviste: "+ready_.fingerprint);
      if(!ready_.date.empty())text("Lähderevision päiväys: "+ready_.date);
      ImGui::TreePop();
    }
    ImGui::BeginDisabled(readyIsActive_);
    if(button(readyIsActive_?"Tämä versio on käytössä":"Avaa rakennettu versio",true))restartPending_=true;
    ImGui::EndDisabled();
    ImGui::SameLine();if(button("Myöhemmin"))readyExpanded_=false;
  }
  if(!readyWarning_.empty()){gap(4);ImGui::PushStyleColor(ImGuiCol_Text,amber);text(readyWarning_);ImGui::PopStyleColor();}
  gap(8);
  if(ImGui::CollapsingHeader("Tekninen loki")){
    text("Lokihakemisto: "+job_.u8string());
    if(button("Kopioi lokin polku"))ImGui::SetClipboardText(job_.u8string().c_str());
    ImGui::BeginChild("Rakennusloki",{0,180*u()},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(log_.empty()?"Odotetaan taustatyökalun tulostetta…":log_.c_str());ImGui::EndChild();
  }
  ImGui::EndChild();
}
void CoreVersionView::render(){
  hits_.clear();
  if(scrollTop_){ImGui::SetScrollY(0);scrollTop_=false;}
  if(!initialized_){initialized_=true;start("doctor");}
  ImGui::TextUnformatted("ASkompu-versio");
  gap(6);
  const bool wideLayout=ImGui::GetContentRegionAvail().x>940*u();
  ImGui::BeginChild("Käytössä oleva ydin",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);activeCard();ImGui::EndChild();
  if(running_||phase_=="failed"||phase_=="cancelled"||ready_.present()||!readyWarning_.empty())progressCard();
  gap(10);ImGui::BeginDisabled(running_);
  if(button("Paikallinen kansio",sourceMode_==1))sourceMode_=1;
  ImGui::SameLine();if(button("GitHub",sourceMode_==0))sourceMode_=0;
  ImGui::EndDisabled();gap(8);
  if(sourceMode_==1){
    ImGui::BeginChild("Paikallinen lähde",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);
    localCard();ImGui::EndChild();
  }else{
    ImGui::BeginDisabled(running_);
    if(button("Päivitä versiolista"))start("list");
    ImGui::EndDisabled();
    if(ImGui::GetContentRegionAvail().x>580*u())ImGui::SameLine();
    caption("Mikky100/ASkompu");
    if(!upstream_.empty()&&ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);text("Testilähde: "+upstream_);ImGui::PopTextWrapPos();ImGui::EndTooltip();}
    gap(10);
    if(wideLayout){
      ImGui::BeginTable("Version valinta",2,ImGuiTableFlags_SizingStretchProp);
      ImGui::TableSetupColumn("Versiot",ImGuiTableColumnFlags_WidthStretch,1.4f);ImGui::TableSetupColumn("Valinta",ImGuiTableColumnFlags_WidthStretch,1.f);
      ImGui::TableNextColumn();const float listTop=versionList();ImGui::TableNextColumn();
      ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x,listTop});
      ImGui::BeginChild("Valittu versio",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);selectionCard();ImGui::EndChild();ImGui::EndTable();
    }else{versionList();gap(10);selectionCard();}
  }
  gap(10);requirementsCard();
  folderPicker();
  if(restartPending_||rollbackPending_)ImGui::OpenPopup("Aloitetaanko uusi sessio?");
  ImGui::SetNextWindowSize({std::min(500*u(),ImGui::GetIO().DisplaySize.x-30*u()),0},ImGuiCond_Always);
  if(ImGui::BeginPopupModal("Aloitetaanko uusi sessio?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
    text(rollbackPending_?"Käynnistetään edellinen toimiva ohjelma. Nykyisen ajon muistissa olevat tiedot eivät siirry.":"Uusi ASkompu-versio on rakennettu ja testattu. Nykyisen ajon muistissa olevat tiedot eivät siirry uuteen ohjelmaan.");
    if(!rollbackPending_){text(ready_.label+" · "+ready_.profile);if(!ready_.path.empty())text(ready_.path);}
    gap(8);text("Nykyinen ikkuna suljetaan vasta, kun uusi ikkuna on avautunut. Epäonnistuminen jättää nykyisen ohjelman käyttöön.");gap(8);
    if(button("Peruuta käynnistys")){restartPending_=rollbackPending_=false;ImGui::CloseCurrentPopup();}
    ImGui::SameLine();if(button("Aloita uusi sessio",true)){
      start(rollbackPending_?"rollback":"launch",{"--artifact",ready_.artifact,"--current",executable_.u8string()});
      restartPending_=rollbackPending_=false;ImGui::CloseCurrentPopup();
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){restartPending_=rollbackPending_=false;ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
  }
}
} // namespace simulator::desktop
