#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>
#include "ClimateReading.h"

int main()
{
    ClimateReading climate;
    std::atomic<bool> done{false};
    std::thread producer([&]() {
        for (int i = 1; i <= 50000; ++i) {
            const float value = static_cast<float>(i);
            climate.publish({i % 2 != 0, value, value * 2});
        }
        done = true;
    });
    do {
        const auto sample = climate.snapshot();
        // Validity and both numbers must come from the same sensor sample.
        assert(sample.humidityPercent == sample.temperatureC * 2);
        assert(sample.valid == (static_cast<int>(sample.temperatureC) % 2 != 0));
    } while (!done);
    producer.join();
    assert(climate.snapshot().temperatureC == 50000);
    std::cout << "Concurrent climate publication and snapshot tests passed\n";
}
