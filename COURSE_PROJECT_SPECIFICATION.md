# Course Project Specification

## Problem Description

Four-wire PWM fans are widely used in computers, instruments, enclosures, and
small automation systems, but a complete controller must coordinate several
different concerns. It must generate a stable high-frequency PWM control
signal, count asynchronous TACH pulses without losing data, calculate a useful
speed measurement, remain responsive to user input, and expose the same state
through both local and network interfaces. Optional sensors and displays add
further timing, resource, and usability constraints to a small embedded
platform.

In this project, you'll need to develop an ESP32-based smart fan controller.
The system controls the duty cycle of a four-wire fan,
measures its rotational speed, and presents live telemetry and controls through
a Wi-Fi Web Panel. The project emphasizes non-blocking embedded design,
interrupt-safe data handling, input validation, modular C++ organization, and
clear behavior when a network or optional peripheral is unavailable.

The basic system is not a closed-loop RPM regulator. A user requests a PWM duty
cycle from 0% through 100%, and the measured RPM is reported as feedback. More
advanced work may add local display and button interaction, environmental
sensing, or a captive Wi-Fi provisioning workflow.

You are also encouraged to leverage other sensor modalities, signal processing
techniques, or machine learning approaches, whether through additional hardware
modules or improved software algorithms, to enhance measurement accuracy,
robustness, or user experience.

## Basic Features (30%)

1. **Configurable hardware mapping (6%)**
   - Keep fan, LED, I2C, OLED reset, and button pins in a centralized
     configuration module.
   - Keep fan constants, timing intervals, I2C addresses, Wi-Fi credentials,
     and HTTP settings configurable without editing device drivers.

2. **Four-wire fan control (4%)**
   - Generate an approximately 25 kHz PWM signal suitable for a standard
     four-wire PC fan.
   - Accept an integer duty-cycle setting in the inclusive range `[0, 100]`.
   - Support optional software inversion for an external transistor driver.
   - Apply a full-duty startup period before switching to the configured
     default duty cycle.

3. **TACH measurement (6%)**
   - Count TACH pulses accurately enough to produce stable, observable RPM
     measurements while the rest of the system is operating.
   - The counting technique is unrestricted. Hardware interrupts are a common
     solution, but polling, peripheral counters, RTOS tasks, or any other
     implementation can receive full credit if it satisfies the functional
     requirements.
   - Calculate and cache RPM once per second using a configurable
     pulses-per-revolution value.
   - Keep the TACH path independent from HTTP request timing.

4. **Local serial control (4%)**
   - Accept complete base-10 integer duty commands through a non-blocking serial
     input parser.
   - Reject malformed, fractional, and out-of-range values without changing
     the active duty cycle.
   - Report RPM and duty over serial.

5. **Wi-Fi station connectivity (2%)**
   - Connect to a configured Wi-Fi access point using hard-coded SSID and
     password strings.
   - After a successful station connection, print the assigned IP address to
     the serial console.
   - Keep fan control and TACH measurement operating while Wi-Fi is unavailable.
   - Retry failed or lost connections with non-blocking, rollover-safe timing.
   - Do not create a fallback access point as part of the basic implementation.

6. **Web Panel (2%)**
   - Serve a responsive browser interface directly from the ESP32 without
     relying on external page assets.
   - Display the most recent RPM sample and currently applied duty cycle.
   - Refresh telemetry once per second without accumulating overlapping HTTP
     requests.
   - Provide an integer spinner constrained to `[0, 100]` and a separate submit
     button for changing fan duty.
   - Give the user clear connection, validation, and submission feedback.
   - Reserve visible temperature and humidity fields using unavailable-value
     placeholders until the corresponding advanced feature is implemented.

7. **Entrypoint API (2%)**
   - Provide `GET /api/status` for the cached RPM, applied duty, Wi-Fi state,
     and reserved temperature/humidity values.
   - Provide `POST /api/fan/duty` for validated duty-cycle changes.
   - Return explicit JSON content types, appropriate HTTP status codes, and
     machine-readable error messages.
   - Ensure Web Panel and serial commands update the same fan controller state.

8. **Cooperative main loop or task dispatch (4%)**
   - This is a flexible scoring item rather than a prescribed architecture. A
     static, monolithic loop, a cooperative scheduler, RTOS tasks, or another
     dispatch model may be used.
   - The implementation is graded by how smoothly all required features run
     together. Long pauses, missed updates, or one feature noticeably delaying
     another reduce the score.
   - An implementation that approaches non-blocking behavior receives more
     credit; a fully non-blocking implementation that keeps all features
     responsive receives full credit.
   - Fan control and measurement should continue if optional I2C devices are
     absent or fail to initialize.

## Advanced Features (10%)

1. **OLED status display (4%)**
   - Use the Adafruit SSD1306 library and the configurable display geometry,
     address, I2C pins, and optional reset pin.
   - Display Wi-Fi connection state and the assigned station IP address.
   - Display live RPM.
   - Handle an OLED reset configuration of `-1` as no dedicated reset pin.

2. **Onboard LED speed indication (2%)**

    Use the onboard RGB LED to provide an obviously observable indication that
    changes at different fan speeds. The indication method is unrestricted. For
    example, students may vary flash rate, flash pattern, brightness, color, or a
    combination of these properties. No particular RGB LED API or mapping formula
    is required, provided that a viewer can clearly distinguish different speed
    conditions from the emitted light. This requirement contributes **2%** of the
    course project grade.

3. **Temperature and humidity monitoring (2%)**
   - Use the Sensirion `arduino-sht` library with an SHT30 sensor.
   - Sample at a sensor-appropriate non-blocking interval.
   - Display temperature and humidity on the OLED.
   - Replace the Web Panel placeholders and API `null` values with live samples.
   - Represent missing, stale, or failed measurements explicitly.

4. **Four-button local control (4%)**
   - Read four active-low GPIO buttons with software debounce.
   - Assign clear navigation and control roles to the buttons.
   - Increase or decrease fan duty in 5% steps while clamping the result to
     `[0, 100]`.
   - Keep button, serial, and Web Panel changes synchronized through one shared
     fan state.

5. **Three-point automatic temperature control (4%)**
   - Provide a Web Panel editor for three temperature setpoints ordered from
     low to high and three corresponding fan duty percentages also ordered
     from low to high.
   - Validate that all temperatures are ordered correctly and that all duty
     values are integers in the inclusive range `[0, 100]` before applying the
     profile.
   - Use piecewise linear interpolation on the ESP32 to calculate the fan duty
     for temperatures between adjacent setpoints.
   - Use the lowest configured duty at or below the lowest temperature and the
     highest configured duty at or above the highest temperature.
   - Display the active automatic-control state, current temperature, and
     calculated duty in the Web Panel.
   - This feature requires, at minimum, correct configuration and operation of
     a digital temperature/humidity sensor module. A simulated or permanently
     hard-coded temperature input does not satisfy this prerequisite.

6. **Fixed-RPM PID control (6%)**
   - Provide a Web Panel mode that accepts a requested fan speed in RPM.
   - Implement the PID feedback algorithm on the ESP32 using TACH-derived RPM
     as the process measurement and PWM duty as the control output.
   - Limit the PID output to the valid fan duty range `[0, 100]`. A requested
     speed above or below the fan's achievable range must settle at the
     corresponding maximum or minimum operating speed rather than producing an
     invalid output.
   - Update the controller at a documented, stable interval and avoid integral
     accumulation causing prolonged incorrect output after saturation.
   - Display requested RPM, measured RPM, applied duty, and controller mode in
     the Web Panel.
   - Manual duty control, three-point automatic temperature control, and
     fixed-RPM control must be mutually exclusive operating modes with a clear
     transition between them.

7. **Optional bonus: Captive Portal Wi-Fi provisioning (6%)**
   - After a bounded failure period for configured station credentials, create
     an open setup access point.
   - Run a catch-all DNS server that redirects clients to a local Captive
     Portal.
   - Serve a second compact embedded page that accepts a target Wi-Fi SSID and
     password.
   - Never log, echo, or include the submitted password in an HTTP response.
   - Attempt the submitted credentials without interrupting fan operation, and
     shut down the open setup network after a successful station connection.
   - Credential persistence is unrestricted. Keeping credentials only for the
     current session or persisting them by any suitable method is acceptable.
   - An implementation that successfully connects an SD card and reliably
     persists credentials and/or other configuration data may receive
     discretionary bonus credit.

## Presentation Requirements

A group presentation of the device and its functionalities must be delivered at
the end of the semester, between December 9 and December 16. EVERY TEAM MEMBER
must present. A short demo video (30 seconds to 1 minute) needs to be included
during the presentation to show the device in operation. If Advanced Features
are implemented, highlight them clearly. Presentation length and order will be
announced after group formation.

## Other Requirements

### Group Size

3 - 5 students.

### Development Platform

Arduino framework for ESP32-S3 series. We provide an ESP32-S3 Zero development
MCU unit.

A fan for PC cooling. You can use any kind of fan, as long as you can drive,
control and monitor it with a MCU. We provide a 12-inch PWM fan.

An OLED display, either driven by a dedicated driver circuit or driven by your
own method. We provide a 0.95-inch OLED display driven by SSD1306 if you need.
It also comes with 4 buttons. SSD1306 can be programmed with an Arduino library.

A thermometer. We provide an SHT30 digital thermometer module if you need. ESP32
can communicate with it via I2C. This module can be programmed with an Arduino
library.

### Code Readability

Code Readability is an unscored compliance requirement. Source filenames must
be meaningful. No particular abbreviation policy, identifier naming
convention, or naming style is required.

No cap-eligible source file may contain more than **15%** of the project's total
implementation lines at submission time, and each cap-eligible source file
must contain at least **4 physical lines**. The upper threshold is derived from
the Fanmaster prototype at commit `1849661` and rounded upward to the next
5-percentage-point boundary:

| Baseline quantity | Value |
| --- | ---: |
| Longest cap-eligible source file | `fanmaster.ino` |
| Lines in `fanmaster.ino` | 125 |
| Total project implementation lines | 1,047 |
| Calculation | `125 / 1,047 * 100%` |
| Unrounded result | 11.938872970391595% |
| Rounded-up increment | 5 percentage points |
| Enforced maximum | **15%** |
| Enforced minimum per file | **4 physical lines** |

For this metric, a source file is a tracked `.ino`, `.cpp`, or `.h` file, and a
line is one physical line, including blank and comment lines. Markdown and
other documentation are not implementation lines. The dedicated source file
that carries the embedded Web Panel HTML is excluded from the per-file cap.
Lines inside its raw HTML string are also excluded from the total implementation
line count; its remaining C++ wrapper code is included in the total. In the
prototype, this removes 579 raw HTML lines from `WebPanel.cpp` and produces the
1,047-line denominator above.

At submission time, the percentage must be recalculated from the submitted
revision using the same rules. Adding padding, duplicated code, generated code,
or artificial line breaks solely to satisfy the percentage does not meet the
readability requirement. This requirement does not contribute points to the
course project grade.

## Things to Submit

1. **Runnable source code** with clear comments and a `README.md` explaining:

    - How to compile and flash.

    - Hardware setup details including visual **wiring diagram**.

    - If Advanced Features are implemented: describe configuration flow, NVS usage,
      upload logic.

    - If custom algorithms or modules are implemented, include brief explanation
    and comparison.

2. **Summary Report (PDF)** including:

    - System overview and implementation.

    - Challenges and solutions.

    - Special section: Describe any custom modules/algorithms and their
    performance impact.

    - Contribution per member.

3. **Presentation materials**:

    - Slides

    - Demo video: which clearly shows:

      - Boot -> Wi-Fi connection

      - Web panel inspection and controlling

      - Demonstration of all implemented features and functions
