#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "EmbeddedFont.h"
#include "BuildInformation.h"
#include "AppController.h"
#include "GuiSmokeCheck.h"
#include "SimulatorView.h"

namespace {
struct Desktop {
  SDL_Window* window=nullptr;
  SDL_Renderer* renderer=nullptr;
  bool imguiContext=false,platformBackend=false,rendererBackend=false;
  ~Desktop() {
    if(rendererBackend)ImGui_ImplSDLRenderer2_Shutdown();
    if(platformBackend)ImGui_ImplSDL2_Shutdown();
    if(imguiContext)ImGui::DestroyContext();
    if(renderer)SDL_DestroyRenderer(renderer);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();
  }
};
}
int main(int argc,char** argv) {
  bool check=false;
  std::string screenshotDirectory;
  for(int i=1;i<argc;++i) {
    if(std::strcmp(argv[i],"--versio")==0) {
      namespace build=simulator::desktop::build;
      std::cout<<"ASkompu-simulaattori "<<build::version<<'\n'
        <<"Lähderevisio: "<<build::revision<<'\n'<<"Työpuu: "<<build::worktree<<'\n'
        <<"Yhteinen upstream-pohja: "<<build::upstreamBase<<'\n'
        <<"ASkompu-lähteiden SHA-256: "<<build::coreFingerprint<<'\n'
        <<"Simulaattorilähteiden SHA-256: "<<build::simulatorFingerprint<<'\n';
      return 0;
    }
    if(std::strcmp(argv[i],"--ohje")==0) {
      std::cout<<"ASkompu-simulaattori\nKäynnistys: askompu-simulaattori\n"
        "  --versio               Näytä version ja lähteiden tunnisteet\n"
        "  --ohje                 Näytä tämä ohje\n"
        "  --tarkista             Suorita näkyvä käyttöliittymän käynnistystesti ja sulje\n"
        "  --tarkistuskuvat POLKU  Tallenna käynnistystestin tarkistuskuvat olemassa olevaan hakemistoon\n";
      return 0;
    } else if(std::strcmp(argv[i],"--tarkista")==0)check=true;
    else if(std::strcmp(argv[i],"--tarkistuskuvat")==0&&i+1<argc)screenshotDirectory=argv[++i];
    else {std::cerr<<"Virhe: tuntematon tai puutteellinen valitsin. Katso --ohje.\n";return 1;}
  }
  if(!screenshotDirectory.empty()&&!check) {
    std::cerr<<"Virhe: tarkistuskuvat edellyttävät --tarkista-valitsinta.\n";return 1;
  }
  Desktop desktop;
  try {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_IME_SHOW_UI,"1");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)
      throw std::runtime_error("Näyttöjärjestelmää ei voitu alustaa");
    float dpi=96;
    if(SDL_GetDisplayDPI(0,&dpi,nullptr,nullptr)!=0)dpi=96;
    const float scale=std::clamp(dpi/96.0f,1.0f,1.75f);
    desktop.window=SDL_CreateWindow("ASkompu-simulaattori",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
      static_cast<int>(1380*scale),static_cast<int>(960*scale),SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!desktop.window)throw std::runtime_error("Simulaattorin ikkunaa ei voitu avata");
    SDL_SetWindowMinimumSize(desktop.window,740,580);
    desktop.renderer=SDL_CreateRenderer(desktop.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!desktop.renderer)desktop.renderer=SDL_CreateRenderer(desktop.window,-1,SDL_RENDERER_SOFTWARE);
    if(!desktop.renderer)throw std::runtime_error("Näytön piirtäjää ei voitu luoda");
    IMGUI_CHECKVERSION();ImGui::CreateContext();desktop.imguiContext=true;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    // Tab ottaa UI-kohdistuksen käyttöön. Muulloin nuolet kuuluvat ASkompulle.
    ImFontConfig font; font.FontDataOwnedByAtlas=false;
    static const ImWchar glyphs[]={0x20,0xFF,0x2010,0x203A,0x2190,0x2193,0x2212,0x2212,0};
    io.FontDefault=io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(embeddedFont),static_cast<int>(sizeof(embeddedFont)),17*scale,&font,glyphs);
    ImFont* displayFont=io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(embeddedFont),static_cast<int>(sizeof(embeddedFont)),144*scale,&font,glyphs);
    if(!io.FontDefault||!displayFont)throw std::runtime_error("Simulaattorin fonttia ei voitu ladata");
    simulator::desktop::applyDesktopStyle(scale);
    if(!ImGui_ImplSDL2_InitForSDLRenderer(desktop.window,desktop.renderer))
      throw std::runtime_error("Ikkunan ohjausta ei voitu alustaa");
    desktop.platformBackend=true;
    if(!ImGui_ImplSDLRenderer2_Init(desktop.renderer))throw std::runtime_error("Käyttöliittymän piirtoa ei voitu alustaa");
    desktop.rendererBackend=true;
    simulator::desktop::AppController controller;
    simulator::desktop::SimulatorView view(controller,displayFont);
    std::unique_ptr<simulator::desktop::GuiSmokeCheck> smoke;
    if(check)smoke=std::make_unique<simulator::desktop::GuiSmokeCheck>(screenshotDirectory);
    auto previous=std::chrono::steady_clock::now();
    bool done=false;
    while(!done) {
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        if(event.type==SDL_KEYDOWN&&event.key.keysym.scancode==SDL_SCANCODE_TAB)
          io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        if(event.type==SDL_MOUSEBUTTONDOWN)io.ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;
        ImGui_ImplSDL2_ProcessEvent(&event);
        if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE&&
            event.window.windowID==SDL_GetWindowID(desktop.window)))done=true;
      }
      if(done)break;
      const auto now=std::chrono::steady_clock::now();
      const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(now-previous).count();
      previous=now;
      controller.hostFrame(check?16666666:static_cast<uint64_t>(std::max<int64_t>(elapsed,0)));
      ImGui_ImplSDLRenderer2_NewFrame();ImGui_ImplSDL2_NewFrame();
      if(smoke)smoke->beforeFrame(view,controller,desktop.window);
      ImGui::NewFrame();view.render();ImGui::Render();
      SDL_RenderSetScale(desktop.renderer,io.DisplayFramebufferScale.x,io.DisplayFramebufferScale.y);
      SDL_SetRenderDrawColor(desktop.renderer,11,16,19,255);SDL_RenderClear(desktop.renderer);
      ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),desktop.renderer);
      if(smoke)smoke->afterRender(desktop.renderer);
      SDL_RenderPresent(desktop.renderer);
      if(smoke&&smoke->finished())done=true;
      // Vain UI:n joutokäynti: odotus ei koskaan määrää simulaation tuloksia.
      SDL_Delay((SDL_GetWindowFlags(desktop.window)&SDL_WINDOW_MINIMIZED)?20:1);
    }
    return 0;
  } catch(const std::exception& error) {
    std::cerr<<"Virhe: "<<error.what()<<'\n';
    if(!check)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ASkompu-simulaattori · Virhe",error.what(),desktop.window);
    return 1;
  }
}
