#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/RouteOrder.h"

namespace ui {

void formatTrip(char* text, size_t size, int64_t distanceMm);
void formatDelta(char* text, size_t size, int64_t seconds);
void formatSegmentValue(char* text, size_t size,
                        const domain::SegmentDefinition& segment);

}  // namespace ui
