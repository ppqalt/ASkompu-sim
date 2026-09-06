#include "DeviceDisplay.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include "Finnish.h"
#include "ui/DisplayFormatting.h"
#include "ui/DisplayLayout.h"
#include "ui/RouteOrderFormatting.h"

namespace simulator::desktop {
namespace {
struct Canvas {
  ImDrawList* draw;
  ImFont* font;
  ImVec2 origin;
  float scale;
  ImU32 color;
  ImVec2 point(float x,float y) const { return {origin.x+x*scale,origin.y+y*scale}; }
  void rectangle(float x,float y,float w,float h,ImU32 ink,float rounding=0) {
    draw->AddRectFilled(point(x,y),point(x+w,y+h),ink,rounding*scale);
  }
  void text(const std::string& text,float x,float y,float w,float h,float size,
            bool centered=true,ImU32 ink=0) {
    auto measured=font->CalcTextSizeA(size,FLT_MAX,0,text.c_str());
    if(measured.x>w-4) {size*=(w-4)/measured.x;measured=font->CalcTextSizeA(size,FLT_MAX,0,text.c_str());}
    draw->AddText(font,size*scale,point(x+(centered?(w-measured.x)/2:2),y+(h-measured.y)/2),
                  ink?ink:color,text.c_str());
  }
  void title(const std::string& value) {text(value,10,6,460,36,22);}
  void main(const std::string& value,float size=52) {text(value,12,73,456,139,size);}
  void sub(const std::string& value) {text(value,12,216,456,40,19);}
  void footer(const std::string& value) {text(value,8,288,464,27,12);}
};
std::string trip(int64_t mm) {char b[48];ui::formatTrip(b,sizeof(b),mm);return b;}
std::string delta(int64_t s) {char b[48];ui::formatDelta(b,sizeof(b),s);return b;}
std::string segmentValue(const domain::SegmentDefinition& s) {
  char b[48];ui::formatSegmentValue(b,sizeof(b),s);return b;
}
std::string range(const domain::SegmentDefinition& s) {
  char b[32];ui::formatDriveSegmentRange(b,sizeof(b),s);return b;
}
template<class T> std::string timeText(T t) {return clockText({t.hour,t.minute,t.second});}

void basic(Canvas& c,const core::DisplayModel& m) {
  if(m.finishResult.visible) {
    c.title("MAALI");c.main(timeText(m.finishResult.finishClockTime));
    c.sub("YHTEENSÄ "+std::to_string(m.finishResult.totalPoints)+" PISTETTÄ");
    c.footer("VASEN / OIKEA: PALAA");return;
  }
  const auto layout=ui::layoutFor(480,320);
  const auto& t=layout.trip1;
  if(m.tripDisplayMode==domain::TripDisplayMode::BOTH) {
    c.text(trip(m.trip1.distanceMillimeters),t.x,t.y,t.width/2.0f,t.height,54);
    c.text(trip(m.trip2.distanceMillimeters),t.x+t.width/2.0f,t.y,t.width/2.0f,t.height,54);
  } else {
    c.text(trip(m.tripDisplayMode==domain::TripDisplayMode::TRIP_2?
                m.trip2.distanceMillimeters:m.trip1.distanceMillimeters),t.x,t.y,t.width,t.height,76);
  }
  const auto& clock=layout.clock;
  c.text(timeText(m.clock),clock.x,clock.y,clock.width,clock.height,23);
  if(m.competition.state!=domain::CompetitionState::IDLE) {
    const auto& d=layout.delta;
    c.text(m.atOverlay.visible?timeText(m.atOverlay.clockTime):delta(m.competition.deltaSeconds),
           d.x,d.y,d.width,d.height,m.atOverlay.visible?36:68);
    const auto renderSegment=[&](const domain::SegmentDefinition& s,const ui::WidgetRect& r) {
      c.text(ui::driveSegmentLabel(s),r.x,r.y,r.width,24,18);
      c.text(range(s),r.x,r.y+r.height/3.0f-15,r.width,32,23);
      c.text(segmentValue(s),r.x,r.y+r.height*2/3.0f-24,r.width,52,38);
    };
    if(m.competition.hasCurrentSegment)renderSegment(m.competition.currentSegment,layout.currentSegment);
    if(m.competition.timeAdjustmentEditing) {
      const auto& r=layout.nextSegment;
      c.text("AIKA +/−",r.x,r.y,r.width,32,18);
      c.text(delta(m.competition.editedTimeAdjustmentSeconds)+" s",r.x,r.y+55,r.width,90,30);
    } else if(m.competition.hasNextSegment)renderSegment(m.competition.nextSegment,layout.nextSegment);
  }
  if(m.showSpeed) {
    char text[32];std::snprintf(text,sizeof(text),"%.0f km/h",m.speedKmh);
    c.text(text,4,288,140,32,21,false);
  }
  if(m.competition.undoPromptVisible)c.text("VASEN: PERU",160,288,150,32,16);
  if(m.competition.manualSubtractActive) {
    c.rectangle(0,230,480,90,IM_COL32(176,37,50,255));
    c.text("MIINUSTUS",0,230,480,90,50,true,IM_COL32_WHITE);
  }
}

void order(Canvas& c,const core::OrderDisplayModel& m) {
  const auto& e=m.editor;
  std::string title=ui::routeOrderEditorTitle(e),value,detail;
  char text[80];
  switch(e.phase) {
    case route::EditorPhase::COMPETITION_TYPE:value=ui::competitionTypeLabel(e.competitionType);break;
    case route::EditorPhase::START_TIME:
      std::snprintf(text,sizeof(text),e.startTimeField==0?"[%02u]:%02u":"%02u:[%02u]",e.startHour,e.startMinute);
      value=text;break;
    case route::EditorPhase::SEGMENT_TYPE:
      title=e.editingExisting?"MUOKKAA PISTEVÄLIÄ":"MÄÄRÄYSTYYPPI";
      value=e.segmentType==domain::SegmentType::TIME?"AIKA":e.segmentType==domain::SegmentType::SPEED?"NOPEUS":"MITTIS";break;
    case route::EditorPhase::VALUE:
      title=e.segmentType==domain::SegmentType::TIME?"AIKA":e.segmentType==domain::SegmentType::SPEED?
        "NOPEUS":e.enteringMittisTime?"MITTIS AIKA":"MITTIS METRIÄ";
      ui::formatEditableRouteOrderValue(text,sizeof(text),e);value=text;break;
    case route::EditorPhase::CONTINUATION:
      title="JATKOTOIMINTO";value=e.continuation==0?"SEURAAVA":e.continuation==1?"JAT":"MAALI";break;
    case route::EditorPhase::JAT_TYPE:title="JAT-TYYPPI";value=jatName(e.jatType);break;
    case route::EditorPhase::JAT_OFFSET:title="JAT LISÄAIKA";value=delta(e.jatOffsetMinutes)+" min";break;
    case route::EditorPhase::BROWSE:
      if(m.showsStartTime) {std::snprintf(text,sizeof(text),"LÄHTÖ %02u:%02u",e.startHour,e.startMinute);value=text;}
      else if(m.hasSelectedSegment) {
        const auto& s=m.selectedSegment;value=range(s);
        detail=s.segmentType==domain::SegmentType::TIME?"AIKA "+segmentValue(s):
          s.segmentType==domain::SegmentType::SPEED?"NOPEUS "+segmentValue(s)+" km/h":
          "MITTIS "+std::to_string(s.value)+" m · "+std::to_string(s.mittisDurationSeconds/60)+":"+
          (s.mittisDurationSeconds%60<10?"0":"")+std::to_string(s.mittisDurationSeconds%60);
      }
      break;
    case route::EditorPhase::CANCEL_PROMPT:title="KESKEYTETÄÄNKÖ?";value="VASEN: EI   OIKEA: KYLLÄ";break;
    case route::EditorPhase::SAVE_PENDING:title="TALLENNETAAN";value="ODOTA";break;
  }
  c.title(title);c.main(value,48);c.sub(e.saveFailed?"TALLENNUSVIRHE":detail);
  c.footer(e.phase==route::EditorPhase::BROWSE?"VASEN: PALAA   YLÖS/ALAS: SELAA   OIKEA: MUOKKAA":
    e.phase==route::EditorPhase::CANCEL_PROMPT?"VASEN: EI   OIKEA: KYLLÄ":
    "VASEN: TAKAISIN   YLÖS/ALAS: MUUTA   OIKEA: JATKA");
}
}  // namespace

void drawDeviceDisplay(const core::DisplayModel& m,ImVec2 origin,ImVec2 size,ImFont* font) {
  const float brightness=0.4f+0.6f*m.backlightPercent/100.0f;
  ImVec4 color=m.textColor==domain::TextColor::RED?ImVec4(1,.22f,.22f,1):
    m.textColor==domain::TextColor::GREEN?ImVec4(.18f,1,.32f,1):ImVec4(.95f,.97f,.96f,1);
  color.x*=brightness;color.y*=brightness;color.z*=brightness;
  Canvas c{ImGui::GetWindowDrawList(),font,origin,size.x/480.0f,ImGui::ColorConvertFloat4ToU32(color)};
  c.draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(2,5,6,255),8);
  c.draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
  switch(m.screen) {
    case core::Screen::BasicView:basic(c,m);break;
    case core::Screen::StartupTimeEntry:
    case core::Screen::TimeEdit: {
      const auto& t=m.timeEntry;
      c.title(t.startup?"ASETA KELLONAIKA":"MUUTA KELLONAIKAA");
      char text[32];std::snprintf(text,sizeof(text),t.activeField==core::TimeField::Hour?"[%02u]:%02u":"%02u:[%02u]",t.hour,t.minute);
      c.main(text,66);c.sub(t.activeField==core::TimeField::Hour?"TUNNIT":"MINUUTIT");
      c.footer("VASEN: TAKAISIN   YLÖS/ALAS: MUUTA   OIKEA: JATKA");break;
    }
    case core::Screen::Menu: {
      const auto& menu=m.menu;c.title(menu.title?menu.title:"");
      const float fontSize=20+static_cast<unsigned>(m.menuFontSize)*4;
      ImVec4 disabled=color;disabled.x*=.4f;disabled.y*=.4f;disabled.z*=.4f;
      for(unsigned i=0;i<menu.visibleRowCount;++i) {
        if(i==menu.selectedVisibleRow)c.rectangle(7,68+i*76,4,48,c.color,2);
        c.text(menu.rows[i].label?menu.rows[i].label:"",20,55+i*76,435,72,fontSize,false,
               menu.rows[i].enabled?c.color:ImGui::ColorConvertFloat4ToU32(disabled));
        if(!menu.rows[i].enabled)c.text("—",448,55+i*76,24,72,20);
      }
      if(menu.scrollOffset>0)c.text("+",452,36,18,20,15);
      if(menu.scrollOffset+menu.visibleRowCount<menu.totalRows)c.text("+",452,294,18,20,15);
      break;
    }
    case core::Screen::CalibrationEdit:
      c.title("MITTARIKERROIN");c.main(std::to_string(m.calibration.editedMillimetersPerPulse),66);
      c.sub(m.calibration.saveFailed?"TALLENNUSVIRHE":"mm/pulssi");
      c.footer("VASEN: PERU   YLÖS/ALAS: MUUTA   OIKEA: HYVÄKSY");break;
    case core::Screen::MittisProposal:
      c.title("MITTIS KERROIN");
      c.main(std::to_string(m.mittis.oldMillimetersPerPulse)+" → "+std::to_string(m.mittis.proposedMillimetersPerPulse));
      c.sub(m.mittis.saveFailed?"TALLENNUSVIRHE — VANHA KERROIN":m.mittis.valid?"mm/pulssi":"MITATTU MATKA EI KELPAA");
      c.footer("VASEN: HYLKÄÄ   OIKEA: HYVÄKSY");break;
    case core::Screen::JatResult:
      c.title("JAT TULOAIKA");c.main(timeText(m.jatResult.arrivalClockTime));
      c.sub(delta(m.jatResult.finalDeltaSeconds));c.footer("OIKEA: JATKA");break;
    case core::Screen::StartTimeEdit:
      c.title("SEURAAVA LÄHTÖ");
      if(m.startTimeEdit.valueVisible)c.main(timeText(m.startTimeEdit.proposedClockTime));
      c.sub(jatName(m.startTimeEdit.jatType));
      c.footer(m.startTimeEdit.acceptsWithPoint?"YLÖS/ALAS: MUUTA   PISTE: HYVÄKSY":"YLÖS/ALAS: MUUTA   OIKEA: HYVÄKSY");break;
    case core::Screen::OverrideMenu:
      c.title("KILPAILUMUUTOS");c.main(m.overrideMenu.selectedAction==core::OverrideMenuAction::AdditionalOrder?"LISÄMÄÄRÄYS":"TIEKATKO");
      c.footer("VASEN: PERU   YLÖS/ALAS: VAIHDA   OIKEA: AVAA");break;
    case core::Screen::OverrideEdit: {
      const auto& o=m.overrideEdit;c.title(o.action==core::OverrideMenuAction::AdditionalOrder?"LISÄMÄÄRÄYS":"TIEKATKO");
      if(o.phase==core::OverrideEditPhase::StartSegment)c.main("ALKU "+std::to_string(o.startSegmentIndex)+"–"+std::to_string(o.startSegmentIndex+1));
      else if(o.phase==core::OverrideEditPhase::EndPoint)c.main("LOPPU PISTE "+std::to_string(o.endPointIndex));
      else {char text[32];std::snprintf(text,sizeof(text),"AIKA %u:%02u",o.durationSeconds/60,o.durationSeconds%60);c.main(text);}
      if(o.invalid)c.sub("VALINTA EI KELPAA");
      c.footer("VASEN: PALAA   YLÖS/ALAS: MUUTA   OIKEA: JATKA");break;
    }
    case core::Screen::ResultView: {
      const auto& r=m.resultView;
      if(r.type==core::ResultViewType::StageResults) {
        c.title("JAKSOJEN PISTEET");c.main(r.hasStageResult?"JAKSO "+std::to_string(r.stageResult.stageIndex+1)+" · "+std::to_string(r.stageResult.points):"EI TULOKSIA");
        c.sub("YHTEENSÄ "+std::to_string(r.totalPoints));
      } else if(r.type==core::ResultViewType::TotalPoints) {c.title("KOKONAISPISTEET");c.main(std::to_string(r.totalPoints));}
      else {c.title("TAPAHTUMAT");c.main(r.hasEvent?eventDescription(r.event):"EI TAPAHTUMIA",32);
        if(r.hasEvent)c.sub(timeText(r.event.clockTime));}
      c.footer("VASEN: PALAA   YLÖS/ALAS: SELAA");break;
    }
    case core::Screen::OrderAccessPrompt:
      c.title("AJOMÄÄRÄYS");c.main(m.orderAccess.selectedAction==core::OrderAccessAction::Edit?"MUOKKAA AJOMÄÄRÄYSTÄ":"UUSI AJOMÄÄRÄYS",36);
      c.footer("VASEN: PALAA   YLÖS/ALAS: VALITSE   OIKEA: JATKA");break;
    case core::Screen::OrderEdit:order(c,m.order);break;
    case core::Screen::Diagnostics: {
      const auto& d=m.diagnostics;c.title("DIAGNOSTIIKKA");
      const std::string lines[]={"Pulssit "+std::to_string(d.totalPulseCount),
        "Trip 1  "+distanceText(d.trip1DistanceMillimeters),"Trip 2  "+distanceText(d.trip2DistanceMillimeters),
        "Kalibrointi "+std::to_string(d.millimetersPerPulse)+" mm/pulssi",
        "Pulssin ikä "+std::to_string(d.lastPulseAgeMilliseconds)+" ms",
        std::string("Peruutus ")+(d.reverseActive?"päällä":"pois"),"Kello "+timeText(d.clock)};
      for(unsigned i=0;i<7;++i)c.text(lines[i],18,52+i*31,444,30,21,false);
      c.footer("VASEN: PALAA");break;
    }
  }
  c.draw->PopClipRect();
}
}  // namespace simulator::desktop
