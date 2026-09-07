#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "ui/AppController.h"
#include "ui/Finnish.h"
#include "DisplayFormatting.h"

using simulator::desktop::AppController;
using core::ButtonId;
namespace {
static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<AppController&>().engine())>>,
              "Käyttöliittymä saa moottorista vain lukuviittauksen");
void check(bool ok, int line) {
  if (!ok) throw std::runtime_error("Tarkistus epäonnistui rivillä " + std::to_string(line));
}
#define CHECK(v) check((v), __LINE__)
void acceptTime(AppController& c) { c.press(ButtonId::Right); c.press(ButtonId::Right); }
void pauseResume() {
  AppController c;
  c.hostFrame(100000000); CHECK(c.engine().monotonicMicroseconds() == 0);
  c.resume(); c.hostFrame(100000000); CHECK(c.engine().monotonicMicroseconds() == 100000);
  c.pause(); c.hostFrame(1000000000); CHECK(c.engine().monotonicMicroseconds() == 100000);
  c.resume(); c.hostFrame(100000000); CHECK(c.engine().monotonicMicroseconds() == 200000);
}
void multipliers() {
  for (unsigned rate : {1U, 10U, 100U}) {
    AppController c; c.setMultiplier(rate); c.resume(); c.hostFrame(123456789);
    CHECK(c.engine().monotonicMicroseconds() == 123456789ULL * rate / 1000);
  }
}
void partition() {
  AppController big, small;
  acceptTime(big); acceptTime(small);
  for (auto* c : {&big, &small}) { c->setSpeed(73.123); c->setMultiplier(100); c->resume(); }
  big.hostFrame(249999999);
  for (unsigned i=0; i<249; ++i) small.hostFrame(1000000);
  small.hostFrame(999999);
  CHECK(big.engine().monotonicMicroseconds() == small.engine().monotonicMicroseconds());
  CHECK(big.engine().generatedPulseCount() == small.engine().generatedPulseCount());
  CHECK(big.engine().lastPulseMicroseconds() == small.engine().lastPulseMicroseconds());
  CHECK(big.engine().displayModel().speedKmh == small.engine().displayModel().speedKmh);
}
void fractionsAndPause() {
  AppController c; c.resume(); c.hostFrame(999); c.pause(); c.hostFrame(999999);
  CHECK(c.engine().monotonicMicroseconds() == 0);
  c.resume(); c.hostFrame(1); CHECK(c.engine().monotonicMicroseconds() == 1);
}
void stalledFrame() {
  AppController c; c.resume(); c.setMultiplier(100);
  c.hostFrame(std::numeric_limits<uint64_t>::max());
  CHECK(c.engine().monotonicMicroseconds() == 25000000);
  c.hostFrame(std::numeric_limits<uint64_t>::max());
  CHECK(c.discardedHostNanoseconds() == std::numeric_limits<uint64_t>::max());
}
void exactStep() {
  AppController c; c.resume(); c.setMultiplier(100); c.step();
  CHECK(c.paused()); CHECK(c.engine().monotonicMicroseconds() == 100000);
  c.step(); CHECK(c.engine().monotonicMicroseconds() == 200000);
}
void driving() {
  AppController c; acceptTime(c); c.setSpeed(36); c.resume(); c.hostFrame(200000000);
  CHECK(c.engine().generatedPulseCount() == 2);
  CHECK(c.engine().application().trip1DistanceMillimeters() == 2000);
  c.setReverse(true); c.hostFrame(100000000);
  CHECK(c.engine().reverseActive()); CHECK(c.engine().application().trip1DistanceMillimeters() == 1000);
  c.stopVehicle(); c.hostFrame(200000000); CHECK(c.engine().generatedPulseCount() == 3);
}
void buttonsAndHold() {
  AppController c; c.press(ButtonId::Up);
  CHECK(c.engine().displayModel().timeEntry.hour == 1);
  acceptTime(c); c.press(ButtonId::Down);
  CHECK(c.engine().displayModel().screen == core::Screen::Menu);
  c.longPress(ButtonId::Left); CHECK(c.engine().displayModel().screen == core::Screen::BasicView);
  c.setSpeed(36); c.holdReset(ButtonId::Trip1Reset,true); c.step();
  CHECK(c.engine().application().trip1DistanceMillimeters() == 0);
  c.holdReset(ButtonId::Trip1Reset,false); c.step();
  CHECK(c.engine().application().trip1DistanceMillimeters() == 1000);
}
void resetConfirmation() {
  AppController c; acceptTime(c); c.setSpeed(36); c.setReverse(true); c.resume();
  c.hostFrame(100000000); c.requestReset();
  CHECK(c.resetPending() && c.paused());
  c.hostFrame(100000000); c.press(ButtonId::Up); c.setSpeed(60); c.step();
  CHECK(c.engine().monotonicMicroseconds() == 100000);
  CHECK(c.engine().targetSpeedKmh() == 36);
  c.cancelReset(); CHECK(!c.paused()); CHECK(c.engine().generatedPulseCount() == 1);
  c.requestReset(); c.confirmReset();
  CHECK(c.paused() && !c.resetPending()); CHECK(c.multiplier() == 1);
  CHECK(c.engine().monotonicMicroseconds() == 0 && c.engine().generatedPulseCount() == 0);
  CHECK(!c.engine().reverseActive() && !c.engine().clock().isSet());
  CHECK(c.engine().application().screen() == core::Screen::StartupTimeEntry);
  CHECK(c.engine().application().eventRepository().count() == 0 && c.log().size() == 1);
}
void boundedLog() {
  AppController c; acceptTime(c); c.press(ButtonId::At); c.press(ButtonId::At);
  size_t domainEntries=0;
  for (const auto& entry:c.log()) if (entry.applicationEvent) ++domainEntries;
  CHECK(domainEntries == 2);
  const auto& event=*c.engine().application().eventRepository().at(0);
  CHECK(event.cancelled);
  CHECK(simulator::desktop::eventDescription(event).find("peruttu") != std::string::npos);
  for (unsigned i=0; i<1000; ++i) c.setSpeed(i%2 ? 36:60);
  CHECK(c.log().size() == AppController::logCapacity);
}
void formatting() {
  char text[64];
  ui::formatTrip(text,sizeof(text),9999000); CHECK(std::string(text)=="9.999");
  ui::formatTrip(text,sizeof(text),10000000); CHECK(std::string(text)=="10.00");
  ui::formatTrip(text,sizeof(text),-1000); CHECK(std::string(text)=="-0.001");
  ui::formatTrip(text,sizeof(text),std::numeric_limits<int64_t>::min());
  CHECK(std::string(text)=="-9223372036854.77");
  ui::formatDelta(text,sizeof(text),1); CHECK(std::string(text)=="+1");
}
void productionReadBoundary() {
  simulator::InitialState initial;
  initial.millimetersPerPulse=997;
  AppController c(initial);
  CHECK(c.millimetersPerPulse()==997);
  acceptTime(c);c.setSpeed(36);c.step();c.press(ButtonId::At);
  CHECK(c.displayModel().trip1.distanceMillimeters==c.engine().application().trip1DistanceMillimeters());
  CHECK(c.applicationEventCount()==c.engine().application().eventRepository().count());
  CHECK(c.applicationEvent(0)==c.engine().application().eventRepository().at(0));
  bool found=false;
  for(const auto& row:c.diagnostics())
    if(row.first=="Pulssimäärä") {found=true;CHECK(row.second==std::to_string(c.engine().generatedPulseCount()));}
  CHECK(found);
  c.requestReset();c.confirmReset();
  CHECK(c.applicationEventCount()==0&&c.applicationEvent(0)==nullptr&&c.millimetersPerPulse()==997);
}
void invalidInput() {
  AppController c;
  bool caught=false;
  try { c.setMultiplier(0); } catch(const std::invalid_argument&) { caught=true; }
  CHECK(caught && c.multiplier()==1);
  caught=false;
  try { c.setSpeed(std::numeric_limits<double>::quiet_NaN()); } catch(const std::invalid_argument&) { caught=true; }
  CHECK(caught && c.engine().targetSpeedKmh()==0);
}
}
int main() {
  const std::vector<std::pair<const char*,std::function<void()>>> tests={
    {"Keskeytys ja jatkaminen",pauseResume},{"Simulointinopeudet",multipliers},
    {"Ruutuajan pilkkominen",partition},{"Ajan jakojäännös ja tauko",fractionsAndPause},
    {"Pitkän ruutuviiveen rajaaminen",stalledFrame},{"Askel on aina 0,1 s",exactStep},
    {"Nopeus, peruutus ja pysähdys",driving},{"Painikkeet ja pidetty nollaus",buttonsAndHold},
    {"Vahvistettu aloitus alusta",resetConfirmation},{"Rajattu loki ja aidot tapahtumat",boundedLog},
    {"Tuotannon yhteinen lukumuotoilu",formatting},{"Virheelliset ohjaussyötteet",invalidInput},{"Tuotantotilan lukuraja",productionReadBoundary}};
  unsigned failures=0;
  for(const auto& test:tests) {
    try {test.second(); std::cout<<"Hyväksytty: "<<test.first<<'\n';}
    catch(const std::exception& error){++failures;std::cerr<<"Virhe: "<<test.first<<": "<<error.what()<<'\n';}
  }
  std::cout<<tests.size()<<" testiä, "<<failures<<" epäonnistui\n";
  return failures ? 1:0;
}
