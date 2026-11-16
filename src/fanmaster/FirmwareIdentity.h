#pragma once
#include "Config.h"

#ifdef ARDUINO_ARCH_ESP32
#define FANMASTER_APP_IDENTITY __attribute__((section(".rodata_custom_desc"), used))
#else
#define FANMASTER_APP_IDENTITY
#endif

namespace FirmwareIdentity {
// ESP-IDF places .rodata_custom_desc immediately after esp_app_desc_t.
// Arduino's standard project_name describes its prebuilt SDK, not our sketch.
struct Descriptor { char magic[8]; char role[24]; char version[8]; };
static_assert(sizeof(Descriptor) == 40, "Stable on-flash identity layout");
constexpr Descriptor make(const char *role) {
    Descriptor result{{'F','M','B','O','O','T','1',0}, {}, {}};
    for (size_t i = 0; role[i] && i + 1 < sizeof(result.role); ++i) result.role[i] = role[i];
    for (size_t i = 0; Config::VERSION[i] && i + 1 < sizeof(result.version); ++i)
        result.version[i] = Config::VERSION[i];
    return result;
}
}  // namespace FirmwareIdentity
