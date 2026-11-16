#include <cassert>
#include <iostream>
#include "BootSlots.h"
#include "SerialConsole.h"
#include "FanController.h"

int main()
{
    assert(BootSlots::updaterAvailable() && !BootSlots::updaterSelected());
    fakeSlotPresent[1] = false;
    assert(!BootSlots::updaterAvailable());
    assert(BootSlots::selectUpdater() == ESP_ERR_NOT_FOUND && fakeBootWrites == 0);
    fakeSlotPresent[1] = true;
    fakeProjects[1] = "fanmaster"; // A second main image is not the updater.
    assert(BootSlots::selectUpdater() == ESP_ERR_NOT_FOUND && fakeBootWrites == 0);
    fakeProjects[1] = "ResourceUploader";
    fakeSlotValid[1] = false;
    assert(BootSlots::selectUpdater() == ESP_ERR_NOT_FOUND);
    fakeSlotValid[1] = true;
    fakeBootWriteResult = -123;
    assert(BootSlots::selectUpdater() == -123 && !BootSlots::updaterSelected());
    fakeBootWriteResult = ESP_OK;
    FanController fan;
    assert(fan.begin());
    SerialConsole console;
    const char command[] = "boot updater\r\n";
    Serial.incoming.assign(command, command + sizeof(command) - 1);
    console.update(fan);
    assert(BootSlots::updaterSelected() && fakeRestartCount == 0);
    assert(Serial.output.find("Restart manually") != std::string::npos);
    assert(!fan.snapshot().powerEnabled); // Selection works while the fan is off.
    assert(BootSlots::selectMain() == ESP_OK && !BootSlots::updaterSelected());
    fakeProjects[0] = "ResourceUploader";
    assert(BootSlots::selectMain() == ESP_ERR_NOT_FOUND);
    assert(fakeRestartCount == 0);
    std::cout << "Boot slot identity, missing/invalid images, selection errors and serial scheduling tests passed\n";
}
