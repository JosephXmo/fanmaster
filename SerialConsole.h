#pragma once

#include <Arduino.h>

#include "Config.h"

class FanController;

class SerialConsole {
public:
    void update(FanController &fan);

private:
    void processCommand(FanController &fan);
    static char *trim(char *text);

    char inputBuffer_[Config::SerialConsole::INPUT_BUFFER_SIZE] = {};
    size_t inputLength_ = 0;
    bool inputOverflow_ = false;
};
