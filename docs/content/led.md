# Status LED

The onboard addressable RGB LED is a WS2812 device and is driven through the
FastLED Arduino library. `Config::StatusLed::INDICATION_MODE` selects one of
three indication modes:

- `IndicationMode::FLASH` flashes at a response-dependent rate using the fixed
  `FLASH_COLOR_RGB` color, encoded as `0xRRGGBB` and defaulting to red.
- `IndicationMode::SPECTRUM` is the default. It remains lit and changes smoothly
  from violet through the visible spectrum to red as the response rises.
- `IndicationMode::FLASH_AND_SPECTRUM` combines the variable flash rate and
  variable spectrum color.

The response source is selected in `src/fanmaster/Config.h`:

```cpp
constexpr ResponseSource RESPONSE_SOURCE = ResponseSource::TACH;
constexpr IndicationMode INDICATION_MODE = IndicationMode::SPECTRUM;
constexpr uint32_t FLASH_COLOR_RGB = 0xFF0000;
```

- `ResponseSource::DUTY` maps applied duty from 1% through 100%.
- `ResponseSource::TACH` maps measured speed from 550 through 1700 RPM.

For modes that flash, the low endpoint uses a 1200 ms interval and the high
endpoint a 150 ms interval. Spectrum modes map the low endpoint to violet and
the high endpoint to red. The LED remains off when the response is zero.

Function (button 4) + button 3 toggles the indication at runtime. Disabling it keeps the FastLED
controller active but immediately writes black to the RGB LED.

LED timing must remain non-blocking so fan control, TACH sampling, serial
handling, button debounce, and Web Panel requests continue to run while the
LED is flashing.
