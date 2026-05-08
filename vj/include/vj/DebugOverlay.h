#pragma once

#include <string>

#include "MidiController.h"
#include "PrimitiveInterceptor.h"

namespace vj {

std::string buildDebugText(const PrimitiveInterceptor& interceptor,
                           const MidiController& midi);

}
