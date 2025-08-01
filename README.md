# Anker Solix 2 Balcony Power Plant Monitor

This repository contains the firmware and documentation for an ESP32‑based
display that visualises the daily energy production and consumption of an
Anker Solix 2 balcony power plant.  The device uses a standalone
ESP32‑2432S032C module with a built‑in **3.2" 320x240 pixel TFT LCD** and
capacitive touch.  The module is based on a dual‑core ESP32 SoC with built‑in
Wi‑Fi and Bluetooth.  The display resolution is
240 x 320 pixels and it supports 65 k colours.

## Features

* **Standalone monitoring** – The ESP32 connects directly to your Wi‑Fi
  network and retrieves data either from the **Anker Solix cloud** or a local
  **smart‑meter**.  No additional servers or Raspberry Pi are required.
* **Graphical display** – A 24‑point curve shows the hourly **generation**
  (green) and **consumption** (red) of the current day.  The graph is
  automatically scaled to the maximum value so that even small differences are
  visible.
* **Numerical summaries** – The current battery charge (in %), daily energy
  production and daily energy consumption (in kWh) are displayed below the
  graph.
* **Dual modes** – At compile time you can select between retrieving data from
  the **Anker cloud** (requires account credentials) or from a **local
  smart‑meter** running on your LAN.
* **Manual refresh and timestamp** – When touch support is enabled the
  firmware draws a **Refresh** button on the lower right of the screen.
  Tapping this button triggers an immediate update.  Beneath the button the
  display shows the time of the last successful data retrieval in
  ``HH:MM:SS`` format.

## Hardware

This project targets the **Sunton ESP32‑2432S032C** development board (also
marketed under various names on Amazon, AliExpress, etc.).  The board
combines an ESP32‑D0WDQ6 microcontroller with a **3.2" 320x240 pixel
65 k‑colour touch display** driven by an ST7789 controller.  The
physical module size is approximately **93.7 x 55 mm** and the effective
display area is **64.8 x 48.6 mm**.

## File structure

| Path                                    | Description                                                                 |
|-----------------------------------------|-----------------------------------------------------------------------------|
| `src/main.cpp`                          | Main Arduino sketch.  Handles Wi‑Fi, API calls, graph drawing and user interface. |
| `src/secrets.h`                         | Template for storing your Wi‑Fi credentials and API endpoints.  **Do not commit your real credentials**. |
| `README.md`                             | This file.  Explains how to build, flash and use the monitor.              |

## Dependencies

Install the following libraries via the Arduino IDE library manager or PlatformIO:

* **TFT_eSPI** by Bodmer – used to drive the ST7789 TFT.  After installing
  the library, edit `TFT_eSPI/User_Setup.h` and:
  * enable `#define ST7789_DRIVER`,
  * set `#define TFT_WIDTH  240` and `#define TFT_HEIGHT 320`,
  * configure the SPI pins according to your board.  On the ESP32‑2432S032C
    the default wiring matches the examples in the TFT_eSPI library.
* **ArduinoJson** by Benoît Blanchon – used to parse JSON responses.

These libraries are available from the Arduino Library Manager.  In PlatformIO
you can add them to your `platformio.ini` dependencies.

The firmware uses the built‑in `HTTPClient` and `WiFi` libraries provided by
the ESP32 core.

If you wish to use the on‑screen refresh button you must also install a
**GT911** touch driver library such as
[`alex-code/GT911`](https://github.com/alex-code/GT911) and set the
``HAS_TOUCH`` macro to `1` in `src/main.cpp`.  Without touch support the
button is drawn but cannot be pressed; the firmware will still refresh
automatically every five minutes.

## Configuration

1. Copy `src/secrets.h` to a new file named `src/secrets.h` (the template is
   included in this repository).
2. Open `src/secrets.h` in a text editor and set the following fields:
   * **Wi‑Fi credentials** – `WIFI_SSID` and `WIFI_PASSWORD` must match your
     home network.
   * **Anker cloud** – if you plan to use the Anker cloud, provide your
     Anker account e‑mail in `ANKER_USER`, your password in `ANKER_PASSWORD`,
     the two‑letter country code in `ANKER_COUNTRY` and the API
     endpoints in `ANKER_AUTH_URL` and `ANKER_ENERGY_URL`.  Consult the
     Anker API documentation or the [anker‑solix‑api](https://github.com/thomluther/anker-solix-api)
     project for details on available endpoints.  The energy endpoint must
     return JSON with the following structure:
     
     ```json
     {
       "battery_percent": 80.3,
       "daily_generation": 3.45,
       "daily_consumption": 2.10,
       "generation_curve": [0.0, 0.0, ...],
       "consumption_curve": [0.0, 0.0, ...]
     }
     ```
     The curves must contain exactly 24 values corresponding to each hour of
     the day.
   * **Local smart‑meter** – if you plan to fetch data from a local meter,
     provide its IP address or hostname in `SMARTMETER_HOST`, the API
     endpoint path in `SMARTMETER_ENERGY_ENDPOINT` (e.g. `/api/daily`) and
     optionally an authentication token in `SMARTMETER_TOKEN`.

3. In `src/main.cpp` you can change the line `Mode currentMode = …;` to
   select either `Mode::MODE_ANKER_CLOUD` or `Mode::MODE_LOCAL_SMARTMETER` at
   compile time.  Alternatively, you can add a button or touch handler to
   switch modes at runtime.

4. (Optional) To enable the on‑screen refresh button, set the macro
   ``HAS_TOUCH`` to `1` at the top of `src/main.cpp` and install the
   GT911 library.  If ``HAS_TOUCH`` is left as `0` the button will be
   displayed but touches will be ignored.

## Building and flashing

### Arduino IDE

1. Install the **ESP32 board support package** from the Arduino Boards
   Manager (use the latest 3.x version).  Select **Sunton ESP32 2432S032C** or
   a generic ESP32‑WROVER module if the board is not listed.
2. Install the dependencies mentioned above (TFT_eSPI and ArduinoJson).
3. Configure TFT_eSPI according to your display as described in the
   **Dependencies** section.
4. Open `src/main.cpp` in the Arduino IDE.  The IDE will automatically
   convert it to a sketch.  Make sure `src/secrets.h` is present in the same
   directory.
5. Select the correct serial port and upload the sketch.  On first boot
   the display will show a splash screen and then update every five minutes.

### PlatformIO

Alternatively you can use [PlatformIO](https://platformio.org/):

```ini
[env:sunton_esp32_display]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
  bodmer/TFT_eSPI
  bblanchon/ArduinoJson
```

Adjust the board definition if PlatformIO has an entry for `esp32-2432s032c`.
Place `src` and `include` directories into the PlatformIO project and build
with `pio run` and upload with `pio run -t upload`.

## Usage

After uploading the firmware the ESP32 will connect to the configured Wi‑Fi
network and retrieve energy data every **five minutes**.  If the data
acquisition fails (e.g., due to network problems or incorrect endpoints) a
message is displayed on the screen.  Successful retrieval results in a
coloured graph and numerical values.  The green line represents the **solar
generation**; the red line shows **consumption**.  The values at the bottom
show the **battery charge**, **daily generation** and **daily consumption**.

If you have enabled the GT911 touch driver, a **Refresh** button appears
in the lower right corner.  Tapping this button forces an immediate
update regardless of the five‑minute schedule.  The label below the button
indicates the time (UTC) of the last successful update.  If the system
time has not been synchronised, the timestamp remains ``--:--:--`` until
valid data is retrieved.

The device does not support deep sleep because the board's integrated
Li‑ion charger enters standby if the current consumption is too low.
Therefore the ESP32 remains active and updates the display periodically.

## Limitations

* The Anker API is proprietary and may change.  You need to provide the
  correct authentication and energy endpoints; otherwise the firmware will
  not fetch any data.
* The sample implementation expects the energy endpoint to return 24 hourly
  values.  If your meter provides a different sampling rate you must adjust
  `POINTS_PER_DAY` and the parsing logic accordingly.
* If the battery charge or energy values are not present in the JSON
  response they are displayed as `--` on the screen.

## License

This project is released under the MIT License.  See the `LICENSE` file for
more information.