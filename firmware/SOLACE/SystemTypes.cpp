#include "SystemTypes.h"

const char *stateName(SystemState state) {
  switch (state) {
    case SystemState::LOW_LIGHT: return "LOW_LIGHT";
    case SystemState::TRACKING: return "TRACKING";
    case SystemState::ALIGNED: return "ALIGNED";
    case SystemState::SHADE: return "SHADE";
    case SystemState::STOW_WIND: return "STOW_WIND";
    case SystemState::STOW_RAIN: return "STOW_RAIN";
    case SystemState::FAULT: return "FAULT";
  }
  return "UNKNOWN";
}
