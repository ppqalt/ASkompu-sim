#pragma once

#include <string>
#include "core/AppEvent.h"
#include "core/DisplayModel.h"

namespace simulator::desktop {
const char* buttonName(core::ButtonId id);
const char* screenName(core::Screen screen);
const char* competitionName(domain::CompetitionState state);
const char* eventName(domain::DomainEventType type);
const char* jatName(domain::JatType type);
std::string durationText(uint64_t microseconds);
std::string distanceText(int64_t millimeters);
std::string clockText(core::ClockTime clock);
std::string eventDescription(const domain::EventRecord& event);
}  // namespace simulator::desktop
