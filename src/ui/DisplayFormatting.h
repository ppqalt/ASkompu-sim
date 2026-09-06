#pragma once

#include <cstdint>
#include <cstdio>
#include <inttypes.h>

#include "domain/RouteOrder.h"

namespace ui {

// Laitenäytön ja desktop-näytön yhteinen lukumuotoilu.
inline void formatTrip(char* text, size_t size, int64_t distanceMm) {
  const bool negative = distanceMm < 0;
  const uint64_t magnitude = negative
                                 ? static_cast<uint64_t>(-(distanceMm + 1)) + 1
                                 : static_cast<uint64_t>(distanceMm);
  const uint64_t meters = magnitude / 1000ULL;
  if (meters < 10000ULL) {
    std::snprintf(text, size, "%s%" PRIu64 ".%03" PRIu64,
                  negative ? "-" : "",
                  meters / 1000ULL, meters % 1000ULL);
  } else {
    const uint64_t tenMeters = meters / 10ULL;
    std::snprintf(text, size, "%s%" PRIu64 ".%02" PRIu64,
                  negative ? "-" : "",
                  tenMeters / 100ULL, tenMeters % 100ULL);
  }
}

inline void formatDelta(char* text, size_t size, int64_t seconds) {
  if (seconds > 0)
    std::snprintf(text, size, "+%" PRId64, seconds);
  else
    std::snprintf(text, size, "%" PRId64, seconds);
}

inline void formatSegmentValue(char* text, size_t size,
                        const domain::SegmentDefinition& segment) {
  if (segment.segmentType == domain::SegmentType::TIME) {
    std::snprintf(text, size, "%lu:%02lu",
                  static_cast<unsigned long>(segment.value / 60),
                  static_cast<unsigned long>(segment.value % 60));
  } else if (segment.segmentType == domain::SegmentType::SPEED) {
    std::snprintf(text, size, "%lu",
                  static_cast<unsigned long>(segment.value));
  } else {
    text[0] = '\0';
  }
}

}  // namespace ui
