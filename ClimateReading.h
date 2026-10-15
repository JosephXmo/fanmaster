#pragma once

// One cached sensor sample shared by the application, OLED and HTTP status.
struct ClimateReading {
    bool valid = false;
    float temperatureC = 0.0f;
    float humidityPercent = 0.0f;
};
