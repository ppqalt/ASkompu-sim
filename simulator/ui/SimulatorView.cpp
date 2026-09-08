#include "SimulatorView.h"

#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <string>
#include "DeviceDisplay.h"
#include "Finnish.h"
#include "BuildInformation.h"

namespace simulator::desktop {
namespace {
const ImVec4 accent{.32f,.84f,.69f,1};
const ImVec4 muted{.54f,.62f,.66f,1};
const ImVec4 amber{1,.69f,.35f,1};
float unit() { return ImGui::GetFontSize()/17.0f; }
void gap(float value=8) { ImGui::Dummy({0,value*unit()}); }
void label(const char* text) { ImGui::TextColored(muted,"%s",text); }
void hint(const char* text) {
  if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);
    ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();ImGui::EndTooltip();
  }
}
void section(const char* title) { ImGui::TextUnformatted(title);gap(6); }
void row(const char* title,const std::string& value) {
  label(title);ImGui::SameLine(ImGui::GetContentRegionAvail().x*.48f);
  ImGui::TextUnformatted(value.c_str());
}
const char* guide(core::Screen s) {
  switch(s) {
    case core::Screen::StartupTimeEntry:return "Aseta ensin kellonaika: YLÖS/ALAS muuttaa arvoa, OIKEA jatkaa ja hyväksyy.";
    case core::Screen::BasicView:return "YLÖS/ALAS avaa valikon. Ajomääräys luodaan ASkompun omassa valikossa.";
    case core::Screen::Menu:return "YLÖS/ALAS selaa, OIKEA avaa ja VASEN palaa. Himmeä valinta ei ole käytettävissä.";
    default:return "Käytä ASkompun painikkeita. Näytön alareuna kertoo tämän näkymän toiminnot.";
  }
}
}  // namespace

void applyDesktopStyle(float scale) {
  ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
  s.WindowPadding={24,22};s.FramePadding={12,10};s.ItemSpacing={10,10};
  s.ItemInnerSpacing={8,8};s.WindowRounding=0;s.ChildRounding=12;s.FrameRounding=6;
  s.PopupRounding=10;s.ScrollbarRounding=8;s.GrabRounding=5;s.WindowBorderSize=0;
  s.ChildBorderSize=0;s.FrameBorderSize=0;s.ScrollbarSize=10;
  s.Colors[ImGuiCol_WindowBg]={.045f,.063f,.075f,1};
  s.Colors[ImGuiCol_ChildBg]={.07f,.094f,.11f,1};
  s.Colors[ImGuiCol_PopupBg]={.085f,.11f,.125f,1};
  s.Colors[ImGuiCol_Text]={.91f,.95f,.95f,1};s.Colors[ImGuiCol_TextDisabled]=muted;
  s.Colors[ImGuiCol_FrameBg]={.11f,.15f,.17f,1};
  s.Colors[ImGuiCol_FrameBgHovered]={.15f,.22f,.24f,1};
  s.Colors[ImGuiCol_FrameBgActive]={.19f,.29f,.3f,1};
  s.Colors[ImGuiCol_Button]={.14f,.19f,.21f,1};
  s.Colors[ImGuiCol_ButtonHovered]={.20f,.28f,.30f,1};
  s.Colors[ImGuiCol_ButtonActive]={.25f,.36f,.36f,1};
  s.Colors[ImGuiCol_Header]={.10f,.15f,.17f,1};
  s.Colors[ImGuiCol_HeaderHovered]={.17f,.31f,.32f,1};
  s.Colors[ImGuiCol_HeaderActive]={.22f,.36f,.35f,1};
  s.Colors[ImGuiCol_PlotHistogram]=accent;
  s.Colors[ImGuiCol_CheckMark]=accent;s.Colors[ImGuiCol_SliderGrab]=accent;
  s.Colors[ImGuiCol_SliderGrabActive]={.49f,.96f,.79f,1};
  s.Colors[ImGuiCol_Separator]={.14f,.19f,.21f,1};
  s.Colors[ImGuiCol_NavCursor]=accent;s.ScaleAllSizes(scale);
}

void SimulatorView::remember(const char* title) {
  hitRegions_[title]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
}
bool SimulatorView::button(const char* title,ImVec2 size,bool primary) {
  if(primary) {
    ImGui::PushStyleColor(ImGuiCol_Button,accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.46f,.94f,.78f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.24f,.70f,.56f,1));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.02f,.12f,.10f,1));
  }
  const bool clicked=ImGui::Button(title,size);remember(title);
  if(primary)ImGui::PopStyleColor(4);
  return clicked;
}

void SimulatorView::transport() {
  const float u=unit();
  const bool singleRow=ImGui::GetContentRegionAvail().x>=850*u;
  if(button(controller_.paused()?"Jatka":"Keskeytä",{124*u,40*u},true)) {
    if(controller_.paused())controller_.resume();else controller_.pause();
  }
  hint("Välilyönti: keskeytä tai jatka simuloitua aikaa. Ajoneuvon nopeus säilyy tauolla.");
  ImGui::SameLine();if(button("Askel",{82*u,40*u}))controller_.step();
  hint("Keskeyttää jatkuvan ajon ja etenee täsmälleen 0,1 sekuntia. Pikanäppäin: N.");
  ImGui::SameLine();if(button("Aloita alusta",{142*u,40*u}))controller_.requestReset();
  if(singleRow)ImGui::SameLine(0,28*u);
  else gap(4);
  ImGui::BeginGroup();
  for(unsigned speed:{1U,10U,100U}) {
    if(speed!=1)ImGui::SameLine();
    const std::string name=std::to_string(speed)+"×";
    if(button(name.c_str(),{64*u,40*u},controller_.multiplier()==speed))controller_.setMultiplier(speed);
  }
  ImGui::SameLine();label("Simulointinopeus");ImGui::EndGroup();
}

void SimulatorView::instrument() {
  const float u=unit();
  const auto model=controller_.displayModel();

  const auto start=ImGui::GetCursorScreenPos();
  const float width=ImGui::GetContentRegionAvail().x;
  const float inset=18*u;
  const float screenHeightBudget=std::clamp(ImGui::GetIO().DisplaySize.y*.36f,220*u,360*u);
  const float screenWidth=std::min(width-2*inset,screenHeightBudget*1.5f);
  const float bezelWidth=screenWidth+2*inset;
  const float offset=(width-bezelWidth)/2;
  const float height=screenWidth*320/480+60*u;
  auto* draw=ImGui::GetWindowDrawList();
  draw->AddRectFilled({start.x+offset,start.y},{start.x+offset+bezelWidth,start.y+height},IM_COL32(16,23,27,255),14*u);
  draw->AddRect({start.x+offset,start.y},{start.x+offset+bezelWidth,start.y+height},IM_COL32(42,56,62,255),14*u,0,1*u);
  draw->AddText({start.x+offset+inset,start.y+12*u},IM_COL32(146,168,177,255),"ASkompu");
  draw->AddCircleFilled({start.x+offset+bezelWidth-inset,start.y+21*u},3*u,
    controller_.paused()?IM_COL32(224,170,95,255):IM_COL32(82,214,176,255));
  drawDeviceDisplay(model,{start.x+offset+inset,start.y+42*u},{screenWidth,screenWidth*320/480},displayFont_);
  ImGui::Dummy({width,height});
  hint(guide(model.screen));
}

void SimulatorView::vehicle() {
  const float u=unit();section("Ajoneuvo");
  label("Tavoitenopeus");
  char speed[32];std::snprintf(speed,sizeof(speed),"%.1f",controller_.engine().targetSpeedKmh());
  ImGui::PushFont(displayFont_);
  // Suuren fontin koko skaalataan vain tämän lukeman ajaksi.
  ImGui::SetWindowFontScale(.30f);
  ImGui::TextColored(accent,"%s",speed);
  ImGui::SetWindowFontScale(1);ImGui::PopFont();
  ImGui::SameLine();label("km/h");
  float value=static_cast<float>(controller_.engine().targetSpeedKmh());
  ImGui::SetNextItemWidth(-1);
  if(ImGui::SliderFloat("##Nopeus",&value,0,200,"%.1f km/h",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))controller_.setSpeed(value);
  remember("Nopeus");hint("Aseta ajoneuvon tavoitenopeus. Muutos kulkee simulaattorin pulssigeneraattorin kautta.");
  ImGui::SetNextItemWidth(150*u);
  if(!ImGui::IsAnyItemActive())speedInput_=static_cast<float>(controller_.engine().targetSpeedKmh());
  if(ImGui::InputFloat("km/h##syotto",&speedInput_,0,0,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue))
    controller_.setSpeed(std::clamp(speedInput_,0.0f,200.0f));
  remember("Nopeuden syöttö");hint("Kirjoita nopeus ja hyväksy Enterillä. Sallitut arvot 0–200 km/h.");
  gap(4);
  const float presetWidth=(ImGui::GetContentRegionAvail().x-30*u)/4;
  for(int preset:{0,36,60,120}) {
    if(preset)ImGui::SameLine();
    if(button(std::to_string(preset).c_str(),{presetWidth,34*u}))
      controller_.setSpeed(preset);
  }
  gap(5);
  if(button("Pysäytä",{-1,40*u}))controller_.stopVehicle();
  hint("Asettaa ajoneuvon nopeudeksi 0 km/h. Simuloitu aika jatkuu. Pikanäppäin: S.");
  bool reverse=controller_.engine().reverseActive();
  if(ImGui::Checkbox("Peruutus",&reverse))controller_.setReverse(reverse);
  remember("Peruutus");hint("Peruutussignaali toimitetaan oikealle ASkompu-ytimelle. Pikanäppäin: R.");
  ImGui::SameLine();ImGui::TextColored(reverse?amber:muted,"%s",reverse?"Peruutustilassa":"Eteenpäin");
}

void SimulatorView::controls() {
  const float u=unit();
  const float available=ImGui::GetContentRegionAvail().x;
  const float budget=std::clamp(ImGui::GetIO().DisplaySize.y*.36f,220*u,360*u);
  const float bezel=std::min(available-36*u,budget*1.5f)+36*u;
  const float width=std::min(420*u,bezel-36*u);
  const float left=ImGui::GetCursorPosX()+(available+bezel)/2-width-18*u;
  const float spacing=6*u,w=(width-3*spacing)/4,h=58*u;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(spacing,spacing));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,5*u);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1*u);
  auto key=[&](const char* text,const char* identity,core::ButtonId id,int direction=-1) {
    if(button(text,{w,h}))controller_.press(id);
    remember(identity);
    if(direction>=0) {
      const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
      const ImVec2 c{(lo.x+hi.x)/2,(lo.y+hi.y)/2};
      const ImVec2 d=direction==0?ImVec2{-1,0}:direction==1?ImVec2{0,-1}:direction==2?ImVec2{1,0}:ImVec2{0,1};
      auto point=[&](float along,float across){return ImVec2{c.x+(d.x*along-d.y*across)*u,c.y+(d.y*along+d.x*across)*u};};
      auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(ImGuiCol_Text);
      draw->AddLine(point(-12,0),point(1,0),color,7*u);
      draw->AddTriangleFilled(point(13,0),point(0,-11),point(0,11),color);
    }
  };
  ImGui::SetCursorPosX(left);
  key("VP","PISTE",core::ButtonId::Point);
  hint("VP / PISTE: kirjaa piste. Pikanäppäin: P. Pitkä painallus löytyy lisävalikosta.");
  ImGui::SameLine();key("##VASEN","VASEN",core::ButtonId::Left,0);hint("VASEN — vasen nuolinäppäin.");
  ImGui::SameLine();key("##YLÖS","YLÖS",core::ButtonId::Up,1);hint("YLÖS — nuolinäppäin ylös.");
  ImGui::SameLine();key("##OIKEA","OIKEA",core::ButtonId::Right,2);hint("OIKEA — oikea nuolinäppäin.");
  ImGui::SetCursorPosX(left);
  key("TRIP","TRIP",core::ButtonId::Trip1Reset);
  hint("Sisäinen trip-nollaus. Nollaa tuotannon asetuksen valitseman tripin.");
  ImGui::SameLine();key("AT","AT",core::ButtonId::At);
  hint("Kirjaa AT-aika. Uusi painallus tuotannon perumisikkunassa peruu kirjauksen. Pikanäppäin: A.");
  ImGui::SameLine();key("##ALAS","ALAS",core::ButtonId::Down,3);hint("ALAS — nuolinäppäin alas.");
  ImGui::PopStyleVar(3);
  gap(4);
  if(ImGui::TreeNode("Pitkät painallukset ja nollaus")) {
    if(button("Pitkä VASEN",{-1,0}))controller_.longPress(core::ButtonId::Left);
    hint("Palaa perusnäkymään. Pikanäppäin: Esc, kun tekstikenttää tai valintaikkunaa ei muokata.");
    if(button("Pitkä PISTE",{-1,0}))controller_.longPress(core::ButtonId::Point);
    hint("Avaa tuotannon kilpailumuutosvalikon, kun toiminto on käytettävissä. Ei lyhyttä pistekirjausta.");
    for(auto id:{core::ButtonId::Trip1Reset,core::ButtonId::Trip2Reset,core::ButtonId::FootReset}) {
      if(button(buttonName(id),{-1,0}))controller_.press(id);
      hint("Nollaa tuotannon asetuksen valitseman tripin. Ulkoinen nollaus ja jalkanollaus käyttävät samaa kohdeasetusta.");
    }
    ImGui::TreePop();
  }
}

void SimulatorView::telemetry() {
  const auto model=controller_.displayModel();
  if(ImGui::BeginTable("Matkat",3,ImGuiTableFlags_SizingStretchSame)) {
    for(int i=0;i<3;++i) {
      ImGui::TableNextColumn();label(i==0?"Trip 1":i==1?"Trip 2":"Kalibrointi");
      ImGui::TextUnformatted((i==0?distanceText(model.trip1.distanceMillimeters):
        i==1?distanceText(model.trip2.distanceMillimeters):std::to_string(controller_.millimetersPerPulse())+" mm/pulssi").c_str());
    }
    ImGui::EndTable();
  }
  gap(6);
  if(ImGui::CollapsingHeader("Sisäinen tila"))
    for(const auto& item:controller_.diagnostics())row(item.first.c_str(),item.second);
}

void SimulatorView::eventLog() {
  const float u=unit();
  if(!ImGui::CollapsingHeader("Tapahtumaloki"))return;
  ImGui::Checkbox("Seuraa uusinta",&followLog_);ImGui::SameLine();
  ImGui::Checkbox("Vain ASkompu",&showApplicationOnly_);
  hint("Lokissa säilyvät viimeisimmät 512 havaintoa.");
  ImGui::BeginChild("Lokirivit",{0,190*u},ImGuiChildFlags_AlwaysUseWindowPadding);
  if(ImGui::IsWindowHovered()&&ImGui::GetIO().MouseWheel>0)followLog_=false;
  if(ImGui::BeginTable("Tapahtumat",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Havaittu aika",ImGuiTableColumnFlags_WidthFixed,132*u);
    ImGui::TableSetupColumn("Lähde",ImGuiTableColumnFlags_WidthFixed,100*u);
    ImGui::TableSetupColumn("Tapahtuma");ImGui::TableHeadersRow();
    for(const auto& item:controller_.log()) {
      if(showApplicationOnly_&&!item.applicationEvent)continue;
      ImGui::TableNextRow();ImGui::TableNextColumn();
      ImGui::TextColored(muted,"%s",durationText(item.observedAtUs).c_str());
      ImGui::TableNextColumn();ImGui::TextColored(item.applicationEvent?accent:muted,"%s",item.applicationEvent?"ASkompu":"Simulaattori");
      ImGui::TableNextColumn();
      const auto* event=item.applicationEvent?controller_.applicationEvent(item.repositoryIndex):nullptr;
      const auto text=event?eventDescription(*event):item.text;
      ImGui::TextWrapped("%s",text.c_str());
      if(event&&ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("ASkompun kello %s",clockText({event->clockTime.hour,event->clockTime.minute,event->clockTime.second}).c_str());
        ImGui::Text("Tapahtuma %llu · pisteväli %u",static_cast<unsigned long long>(event->eventId),event->segmentIndex+1);
        ImGui::Text("Trip 1: %s",distanceText(event->trip1DistanceMillimeters).c_str());
        ImGui::EndTooltip();
      }
    }
    ImGui::EndTable();
  }
  if(showApplicationOnly_&&controller_.applicationEventCount()==0)
    ImGui::TextColored(muted,"Ei vielä ASkompu-tapahtumia.");
  if(followLog_)ImGui::SetScrollHereY(1);
  ImGui::EndChild();
}

void SimulatorView::shortcuts() {
  if(ImGui::GetIO().WantTextInput||ImGui::GetIO().NavVisible||ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return;
  const auto key=[&](ImGuiKey k){return ImGui::IsKeyPressed(k,false);};
  if(key(ImGuiKey_Space)){if(controller_.paused())controller_.resume();else controller_.pause();}
  if(key(ImGuiKey_N))controller_.step();
  if(key(ImGuiKey_S))controller_.stopVehicle();
  if(key(ImGuiKey_R))controller_.setReverse(!controller_.engine().reverseActive());
  if(key(ImGuiKey_P))controller_.press(core::ButtonId::Point);
  if(key(ImGuiKey_A))controller_.press(core::ButtonId::At);
  if(key(ImGuiKey_Escape))controller_.longPress(core::ButtonId::Left);
  for(const auto& p:{std::pair<ImGuiKey,core::ButtonId>{ImGuiKey_LeftArrow,core::ButtonId::Left},
      {ImGuiKey_RightArrow,core::ButtonId::Right},{ImGuiKey_UpArrow,core::ButtonId::Up},{ImGuiKey_DownArrow,core::ButtonId::Down}})
    if(key(p.first))controller_.press(p.second);
}

void SimulatorView::dialogs() {
  const float u=unit();
  if(controller_.resetPending()&&!ImGui::IsPopupOpen("Aloitetaanko alusta?"))ImGui::OpenPopup("Aloitetaanko alusta?");
  ImGui::SetNextWindowSize({std::min(460*u,ImGui::GetIO().DisplaySize.x-30*u),0},ImGuiCond_Always);
  if(ImGui::BeginPopupModal("Aloitetaanko alusta?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Nykyinen ajo, tapahtumat ja ajon aikana tehdyt asetukset poistuvat. ASkompu palaa kellon alustukseen.");
    gap(10);
    if(button("Peruuta",{140*u,40*u})) {controller_.cancelReset();ImGui::CloseCurrentPopup();}
    ImGui::SameLine();if(button("Aloita alusta##vahvista",{180*u,40*u},true)){controller_.confirmReset();ImGui::CloseCurrentPopup();}
    if(ImGui::IsKeyPressed(ImGuiKey_Escape)){controller_.cancelReset();ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
  }
  if(helpOpen_){ImGui::OpenPopup("Käyttöohje");helpOpen_=false;}
  ImGui::SetNextWindowSize({std::min(590*u,ImGui::GetIO().DisplaySize.x-30*u),0},ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints({0,0},{FLT_MAX,ImGui::GetIO().DisplaySize.y-30*u});
  if(ImGui::BeginPopupModal("Käyttöohje",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Aloita asettamalla kellonaika ASkompun painikkeilla. OIKEA siirtyy tunneista minuutteihin ja hyväksyy ajan. Avaa sen jälkeen ajomääräys YLÖS/ALAS-painikkeella avautuvasta valikosta.");
    gap();ImGui::TextWrapped("Nopeus ohjaa ajoneuvoa. Pysäytä asettaa nopeudeksi nollan; Keskeytä pysäyttää simuloidun ajan. Askel etenee aina 0,1 sekuntia. Aloita alusta poistaa nykyisen ajon myös muistista.");
    gap();ImGui::TextWrapped("Näppäimistö: nuolet = suuntapainikkeet, P = PISTE, A = AT, R = peruutus, S = pysäytä, N = askel, välilyönti = keskeytä/jatka, Esc = pitkä VASEN. Pikanäppäimet eivät toimi syöttökenttää muokattaessa.");
    gap();ImGui::TextWrapped("Tab ottaa käyttöön säätimien näppäimistökohdistuksen ja varaa nuolet siihen. Hiiren napsautus palauttaa ASkompun pikanäppäimet. Pienessä ikkunassa paneeleita voi vierittää.");
    gap();ImGui::TextWrapped("Tapahtumalokin aika kertoo, milloin käyttöliittymä havaitsi tapahtuman. ASkompun oman tapahtuma-ajan näet viemällä osoittimen tapahtuman päälle. Asetukset ja ajomääräys säilyvät vain tämän ajon muistissa.");
    gap();
    const bool showAbout=ImGui::TreeNode("Tietoja simulaattorista");
    remember("Tietoja simulaattorista");
    if(showAbout) {
      ImGui::Text("Versio: %s · %s",build::version,build::configuration());
      ImGui::TextWrapped("Lähderevisio: %s",build::revision);
      ImGui::TextWrapped("ASkompu-ydin: %s",build::coreRevision);
      ImGui::TextWrapped("Työpuu: %s",build::worktree);
      ImGui::TextWrapped("Yhteinen upstream-pohja: %s",build::upstreamBase);
      ImGui::TextWrapped("ASkompu-lähteiden tiiviste: %.16s",build::coreFingerprint);
      ImGui::TextWrapped("Simulaattorin tiiviste: %.16s",build::simulatorFingerprint);
      ImGui::TreePop();
    }
    gap();if(button("Sulje",{140*u,40*u},true))ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void SimulatorView::render() {
  hitRegions_.clear();
  coreVersions_.poll();
  ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::Begin("ASkompu-simulaattori",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
  const float u=unit();
  ImGui::TextColored(accent,"ASkompu");ImGui::SameLine();label("/  SIMULAATTORI");
  if(ImGui::GetContentRegionAvail().x>=640*u)ImGui::SameLine(250*u);else gap(4);
  if(button("Simulaattori",{140*u,36*u},!corePage_))corePage_=false;
  ImGui::SameLine();
  if(button("Ytimen versio",{150*u,36*u},corePage_)){corePage_=true;controller_.pause();}
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-82*u);
  if(button("Ohje",{82*u,36*u}))helpOpen_=true;
  if(corePage_) {
    gap(12);coreVersions_.render();
    for(const auto& hit:coreVersions_.hitRegions())hitRegions_[hit.first]={hit.second.first,hit.second.second};
    dialogs();ImGui::End();return;
  }
  gap(6);
  ImGui::TextColored(controller_.paused()?amber:accent,"%s",controller_.paused()?"KESKEYTETTY":"KÄYNNISSÄ");
  ImGui::SameLine();label("·");ImGui::SameLine();
  ImGui::TextUnformatted(durationText(controller_.engine().monotonicMicroseconds()).c_str());
  hint("Simuloitu aika");
  if(controller_.engine().reverseActive()){ImGui::SameLine();ImGui::TextColored(amber,"  PERUUTUS");}
  gap(4);transport();gap(8);
  const bool wide=ImGui::GetContentRegionAvail().x>=1040*u;
  if(wide) {
    ImGui::BeginTable("Päänäkymä",2,ImGuiTableFlags_SizingStretchProp);
    ImGui::TableSetupColumn("Näyttö",ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Ohjaimet",ImGuiTableColumnFlags_WidthFixed,330*u);
    ImGui::TableNextColumn();ImGui::BeginChild("Laite",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);
    instrument();gap(8);controls();ImGui::EndChild();gap(12);
    ImGui::BeginChild("Mittarit",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);telemetry();ImGui::EndChild();
    ImGui::TableNextColumn();
    ImGui::BeginChild("Ajoneuvopaneeli",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);vehicle();ImGui::EndChild();
    ImGui::EndTable();
  } else {
    ImGui::BeginChild("Laite",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);
    instrument();gap(8);controls();ImGui::EndChild();gap(12);
    ImGui::BeginChild("Mittarit",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);telemetry();ImGui::EndChild();gap(18);
    ImGui::BeginChild("Ajoneuvopaneeli",{0,0},ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize|ImGuiChildFlags_AlwaysUseWindowPadding);vehicle();ImGui::EndChild();
  }
  gap(16);eventLog();
  shortcuts();dialogs();ImGui::End();
}
}  // namespace simulator::desktop
