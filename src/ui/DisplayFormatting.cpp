#include "DisplayFormatting.h"

#include <inttypes.h>
#include <cstdio>

namespace ui {

void formatTrip(char* text, size_t size, int64_t distanceMm) {
  const bool negative = distanceMm < 0;
  const uint64_t magnitude = negative
                                 ? static_cast<uint64_t>(-(distanceMm + 1)) + 1
                                 : static_cast<uint64_t>(distanceMm);
  const uint64_t meters = magnitude / 1000ULL;
  if (meters < 10000ULL) {
    std::snprintf(text, size, "%s%" PRIu64 ".%03" PRIu64,
                  negative ? "-" : "",
                  static_cast<uint64_t>(meters / 1000ULL),
                  static_cast<uint64_t>(meters % 1000ULL));
  } else {
    const uint64_t tenMeters = meters / 10ULL;
    std::snprintf(text, size, "%s%" PRIu64 ".%02" PRIu64,
                  negative ? "-" : "",
                  static_cast<uint64_t>(tenMeters / 100ULL),
                  static_cast<uint64_t>(tenMeters % 100ULL));
  }
}

void formatDelta(char* text, size_t size, int64_t seconds) {
  if (seconds > 0)
    std::snprintf(text, size, "+%" PRId64, seconds);
  else
    std::snprintf(text, size, "%" PRId64, seconds);
}

void formatSegmentValue(char* text, size_t size,
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
