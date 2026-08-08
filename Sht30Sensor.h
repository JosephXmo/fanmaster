#pragma once

#include <SHTSensor.h>

class Sht30Sensor {
public:
    Sht30Sensor();

    bool begin();
    bool isReady() const;
    SHTSensor &device();

private:
    SHTSensor sensor_;
    bool ready_ = false;
};
