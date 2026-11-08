#include "ui/FolderBrowser.h"
#include "ui/CoreBuildInfo.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace simulator::desktop;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
  const auto root=fs::temp_directory_path()/
      ("askompu-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try{
    fs::create_directories(root/"project/src/core");fs::create_directories(root/"project/include");
    std::ofstream(root/"project/src/core/ApplicationCore.cpp")<<"test";
    fs::create_directories(root/".hidden");fs::create_directories(root/fs::u8path("kansio ää"));
    FolderBrowser browser;
    require(browser.open(root/"project")&&browser.hasCore(),"ASkompu-kansiota ei tunnistettu");
    require(!browser.open(root/"missing")&&!browser.valid()&&!browser.hasCore(),
            "Virheellinen polku jätti vanhan projektin valittavaksi");
    require(!browser.error().empty(),"Virheelliseltä polulta puuttui virheviesti");
    require(!browser.open(root/"project/src/core/ApplicationCore.cpp"),"Tiedosto hyväksyttiin kansiona");
    require(!browser.open({})&&!browser.hasCore(),"Tyhjä polku hyväksyttiin nykyisenä kansiona");
    require(browser.open(root)&&browser.folders().size()==2,"Piilokansioiden suodatus epäonnistui");
    browser.showHidden(true);
    require(browser.valid()&&browser.folders().size()==3,"Piilokansion näyttäminen ei toiminut");
    require(browser.open(root/"project/../project")&&browser.hasCore(),"Suhteellisen polun normalisointi epäonnistui");
    std::map<std::string,std::string> doctor{{"artifact","accepted"},{"ready_source_kind","local"},
      {"ready_local_path","/src/ASkompu"},{"ready_sha","sha"},{"ready_fingerprint","old-pixels"},
      {"ready_profile","480x320"},{"ready_date","2026-10-12"}};
    const auto build=readCoreBuildInfo(doctor,true);
    require(matchesLocalBuild(build,"/src/ASkompu","old-pixels","480x320"),"Samaa rakennusta ei tunnistettu");
    require(!matchesLocalBuild(build,"/src/other","old-pixels","480x320"),"Eri kansio tunnistettiin rakennetuksi");
    require(!matchesLocalBuild(build,"/src/ASkompu","new-pixels","480x320"),"Muuttunut lähde tunnistettiin rakennetuksi");
    require(!matchesLocalBuild(build,"/src/ASkompu","old-pixels","320x170"),"Väärä näyttökoko tunnistettiin rakennetuksi");
    require(!readCoreBuildInfo({},true).present(),"Puuttuva rakennus jäi valmiiksi");
    const auto git=readCoreBuildInfo({{"artifact","other"},{"source_kind","github"},
      {"sha","selected-sha"},{"tag","v1"},{"profile","320x170"}},false);
    require(matchesGitBuild(git,"selected-sha","320x170"),"Git-rakennusta ei tunnistettu");
    require(!matchesGitBuild(git,"new-sha","320x170"),"Väärä commit tunnistettiin valmiiksi");
    require(!matchesGitBuild(build,"sha","480x320"),"Paikallinen rakennus tunnistettiin Git-rakennukseksi");
    require(buildStageIndex("installing")==4&&buildStageIndex("validating")==5&&
        buildStageIndex("ready")==6&&buildStageIndex("starting")==-1,"Rakennuksen loppuvaiheet eivät päivity");
    fs::remove_all(root);
    std::cout<<"Hyväksytty: kansiovalitsimen virhetilat, piilokansiot sekä lähteen ja näyttökoon rakennustilat\n";
    return 0;
  }catch(const std::exception& error){
    std::error_code ec;fs::remove_all(root,ec);
    std::cerr<<"Virhe: "<<error.what()<<'\n';return 1;
  }
}
