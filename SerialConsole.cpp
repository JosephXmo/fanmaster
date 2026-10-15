#include "SerialConsole.h"

#include <stdlib.h>
#include <string.h>

#include "FanController.h"

void SerialConsole::update(FanController &fan)
{
    while (Serial.available() > 0) {
        const char received = static_cast<char>(Serial.read());

        if (received == '\r' || received == '\n') {
            // CR+LF produces a second empty line, which is intentionally ignored.
            if (inputLength_ > 0 || inputOverflow_) {
                processCommand(fan);
            }

            inputLength_ = 0;
            inputOverflow_ = false;
            continue;
        }

        if (inputLength_ < Config::SerialConsole::INPUT_BUFFER_SIZE - 1) {
            inputBuffer_[inputLength_++] = received;
        } else {
            inputOverflow_ = true;
        }
    }
}

void SerialConsole::processCommand(FanController &fan)
{
    if (inputOverflow_) {
        Serial.println("Ignored: input is too long; enter an integer from 0 to 100.");
        return;
    }

    inputBuffer_[inputLength_] = '\0';
    char *input = trim(inputBuffer_);

    if (*input == '\0') {
        return;
    }

    if (strcmp(input, "mode duty") == 0 || strcmp(input, "mode rpm") == 0) {
        const bool changed = fan.setMode(strcmp(input, "mode rpm") == 0 ? FanMode::Rpm : FanMode::Duty);
        Serial.println(changed ? "Control mode updated." : "Ignored: exit THERMAL first.");
        return;
    }
    if (strncmp(input, "rpm ", 4) == 0) {
        char *end = nullptr;
        const char *number = trim(input + 4);
        const unsigned long rpm = strtoul(number, &end, 10);
        if (*number < '0' || *number > '9' || *end ||
            rpm > Config::RpmControl::MAX_RPM || rpm < Config::RpmControl::MIN_RPM) {
            Serial.println("Ignored: RPM must be a whole number within Config limits.");
        } else if (!fan.setTargetRpm(static_cast<uint32_t>(rpm))) {
            Serial.println("Ignored: exit THERMAL first.");
        } else {
            Serial.println("RPM target updated; RPM control active.");
        }
        return;
    }

    char *parseEnd = nullptr;
    const long requestedDuty = strtol(input, &parseEnd, 10);

    if (parseEnd == input || *parseEnd != '\0') {
        Serial.println("Ignored: please enter an integer from 0 to 100.");
        return;
    }

    if (requestedDuty < 0 || requestedDuty > 100) {
        Serial.println("Ignored: duty cycle must be between 0 and 100.");
        return;
    }

    if (!fan.setDutyPercent(static_cast<uint8_t>(requestedDuty))) {
        Serial.println("Ignored: exit THERMAL first.");
        return;
    }

    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.println("Duty cycle updated.");
        Serial.print(fan.dutyPercent());
        Serial.println('%');
    }
}

char *SerialConsole::trim(char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    char *end = text;
    while (*end != '\0') {
        end++;
    }

    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }

    *end = '\0';
    return text;
}
