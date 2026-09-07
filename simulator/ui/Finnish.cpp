#include "Finnish.h"

#include <cstdio>
#include "DisplayFormatting.h"

namespace simulator::desktop {
const char* buttonName(core::ButtonId id) {
  switch (id) {
    case core::ButtonId::Left: return "VASEN";
    case core::ButtonId::Up: return "YLÖS";
    case core::ButtonId::Down: return "ALAS";
    case core::ButtonId::Right: return "OIKEA";
    case core::ButtonId::Point: return "PISTE";
    case core::ButtonId::At: return "AT";
    case core::ButtonId::Trip1Reset: return "Sisäinen nollaus";
    case core::ButtonId::Trip2Reset: return "Ulkoinen nollaus";
    case core::ButtonId::FootReset: return "Jalkanollaus";
  }
  return "Painike";
}
const char* screenName(core::Screen s) {
  switch (s) {
    case core::Screen::StartupTimeEntry: return "Kellon alustus";
    case core::Screen::BasicView: return "Ajonäkymä";
    case core::Screen::Menu: return "Valikko";
    case core::Screen::TimeEdit: return "Kellon asetus";
    case core::Screen::CalibrationEdit: return "Kalibrointi";
    case core::Screen::MittisProposal: return "MITTIS-ehdotus";
    case core::Screen::JatResult: return "JAT-tulos";
    case core::Screen::StartTimeEdit: return "Seuraava lähtö";
    case core::Screen::OverrideMenu: return "Kilpailumuutos";
    case core::Screen::OverrideEdit: return "Muutoksen asetus";
    case core::Screen::ResultView: return "Tulokset";
    case core::Screen::OrderAccessPrompt: return "Ajomääräyksen valinta";
    case core::Screen::OrderEdit: return "Ajomääräyksen muokkaus";
    case core::Screen::Diagnostics: return "Diagnostiikka";
  }
  return "Näyttö";
}
const char* competitionName(domain::CompetitionState s) {
  switch (s) {
    case domain::CompetitionState::IDLE: return "Ei kilpailua";
    case domain::CompetitionState::WAIT_START: return "Odottaa lähtöä";
    case domain::CompetitionState::RUNNING: return "Kilpailu käynnissä";
    case domain::CompetitionState::JAT_RESULT: return "JAT-tulos";
    case domain::CompetitionState::EDIT_START_TIME: return "Lähtöajan asetus";
    case domain::CompetitionState::FINISHED: return "Maalissa";
  }
  return "Kilpailu";
}
const char* eventName(domain::DomainEventType t) {
  switch (t) {
    case domain::DomainEventType::NORMAL_POINT: return "Reittipiste kirjattu";
    case domain::DomainEventType::POINT_UNDO: return "Reittipiste peruttu";
    case domain::DomainEventType::AT: return "AT-aika kirjattu";
    case domain::DomainEventType::AT_CANCELLED: return "AT-aika peruttu";
    case domain::DomainEventType::JAT: return "Saapuminen JAT-pisteelle";
    case domain::DomainEventType::FINISH: return "Saapuminen maaliin";
    case domain::DomainEventType::START_TIME_PROPOSED: return "Lähtöaikaa ehdotettu";
    case domain::DomainEventType::START_TIME_ACCEPTED: return "Lähtöaika hyväksytty";
    case domain::DomainEventType::START_TIME_CORRECTED: return "Lähtöaika korjattu";
    case domain::DomainEventType::MITTIS_PROPOSED: return "MITTIS-kalibrointia ehdotettu";
    case domain::DomainEventType::MITTIS_ACCEPTED: return "MITTIS-kalibrointi hyväksytty";
    case domain::DomainEventType::MITTIS_REJECTED: return "MITTIS-kalibrointi hylätty";
    case domain::DomainEventType::ADDITIONAL_ORDER: return "Lisämääräys hyväksytty";
    case domain::DomainEventType::ROAD_BREAK: return "Tiekatko hyväksytty";
    case domain::DomainEventType::REVERSE_CHANGED: return "Peruutustila muuttui";
    case domain::DomainEventType::TRIP_RESET: return "Matkamittari nollattu";
    case domain::DomainEventType::MANUAL_SUBTRACTION_CHANGED: return "Miinustustila muuttui";
    case domain::DomainEventType::TIME_ADJUSTMENT: return "Aikakorjaus hyväksytty";
  }
  return "ASkompu-tapahtuma";
}
const char* jatName(domain::JatType t) {
  switch (t) {
    case domain::JatType::MANNED_JAT: return "MIEHITETTY JAT";
    case domain::JatType::EMIT_JAT_OFFSET: return "JAT + AIKA";
    case domain::JatType::EMIT_MLA: return "JAT + MLA";
    case domain::JatType::EMIT_ULA: return "JAT + ULA";
  }
  return "JAT";
}
std::string durationText(uint64_t us) {
  char text[48];
  const uint64_t ms = us / 1000;
  std::snprintf(text, sizeof(text), "%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64,
                ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
  return text;
}
std::string distanceText(int64_t mm) {
  char text[48];
  ui::formatTrip(text, sizeof(text), mm);
  return std::string(text) + " km";
}
std::string clockText(core::ClockTime c) {
  char text[16];
  std::snprintf(text, sizeof(text), "%02u:%02u:%02u", c.hour, c.minute, c.second);
  return text;
}
std::string eventDescription(const domain::EventRecord& e) {
  std::string text = eventName(e.eventType);
  if (e.eventType == domain::DomainEventType::REVERSE_CHANGED)
    text += e.payload.reverseActive ? ": päällä" : ": pois";
  if (e.eventType == domain::DomainEventType::TRIP_RESET)
    text += e.payload.tripChannel == domain::TripChannel::TRIP_1 ? ": Trip 1" : ": Trip 2";
  if (e.payload.hasStageResult)
    text += " · " + std::to_string(e.payload.stageResult.points) + " pistettä";
  if (e.eventType == domain::DomainEventType::MITTIS_ACCEPTED ||
      e.eventType == domain::DomainEventType::MITTIS_PROPOSED)
    text += " · " + std::to_string(e.payload.proposedCalibration) + " mm/pulssi";
  if (e.cancelled) text += " (peruttu)";
  return text;
}
}  // namespace simulator::desktop
