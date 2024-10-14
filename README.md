# Black Magic Probe / DapLink for ESP32-S2

WiFi/USB capable version of the famous BlackMagicProbe (or DapLink) debugger.

# Clone the Repository

Clone the repository with:

```shell
git clone --recursive https://github.com/flipperdevices/blackmagic-esp32-s2.git
```

You must recursively clone the submodules, which `--recursive` will do for you. When you pull new changes, be sure to run `git submodule update --init --recursive`.

# Develop with ESP-IDF

Set up **ESP-IDF v4.4** following [Espressif's instructions](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/get-started/index.html). Make sure `idf.py` is in your PATH and you can successfully run the program.

## Build with ESP-IDF

In your clone of this repository, run:

```shell
idf.py build
```

> Note: the `sdkconfig` file already configures the project to target the ESP32-S2. You don't need to run `idf.py set-target esp32s2`, which will overwrite some of the necessary settings in `sdkconfig`.

## Flash with ESP-IDF

Connect the dev board to your computer with a USB cable. Find the name of the serial port device on your computer that corresponds to the dev board. See the [Flipper Zero guide] on how to connect to the dev board over USB or [Espressif's guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html) on how to find the port name.

Run:

```shell
idf.py -p <port> flash
```

### [(Issue) No Serial Connection Found, Failed to Connect](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/get-started/index.html#encountered-issues-while-flashing)

To resolve this issue, you have to manually put your ESP32 board into Firmware Download mode (also known as Flash mode or programming mode), you typically need to manipulate the boot and reset buttons. Here's how to do it for common ESP32 development boards:

#### Steps to Manually Enter Firmware Download Mode

1. **Identify the Buttons**:

   - **Boot Button**: Often labeled as "Boot," "GPIO0," or something similar.
   - **EN (Reset) Button**: Often labeled as "EN," "RESET," or "CHIP_PU."

2. **Connect the Board**: Make sure your ESP32 board is connected to your computer via USB.
3. **Hold the Boot Button**:

   - Press and hold the **Boot** button (GPIO0) on the ESP32 board.

4. **Press the EN Button**:

   - While still holding the Boot button, press the **EN (Reset)** button once and then release it. This resets the board.

5. **Release the Boot Button**:

   - After pressing the EN button, release the **Boot** button. The board should now be in Firmware Download mode.

6. **Start the Flashing Process**:

   - You can now start flashing your firmware using `idf.py flash` or another appropriate command, depending on the tools you are using.

#### Additional Method (Pulling GPIO0 Down)

If your ESP32 board does not have dedicated buttons for Boot and EN, or if you want to do it manually without pressing buttons:

1. **Pull GPIO0 Down**:

   - You can use a resistor (around 10k ohms) to pull GPIO0 to ground (GND). Connect one end of the resistor to GPIO0 and the other end to GND.

2. **Reset the Board**:

   - After pulling GPIO0 down, you need to reset the board by briefly connecting the EN pin (or the reset button) to ground, then releasing it.

3. **Release GPIO0**:

   - After resetting the board, disconnect the GPIO0 pin from GND. The board should now be in Firmware Download mode.

## Debug Cortex-M and Cortex-A target using GDB Client on CLI 

To use the Blackmagic Probe firmware on an ESP32S2 to debug Cortex-M and Cortex-A targets, follow these steps:

### 1. Hardware Setup
- **Wiring**: Connect the ESP32S2 to your Cortex-M target board. On the ESP32S2, the `IO1` pin is used for SWCLK (clock) and `IO2` for SWDIO (data). Also, ensure you connect the `3.3V` pin from the ESP32S2 to your target board to provide the necessary power regulation.
  
### 2. Network Setup
- **Connect to ESP32S2 Wi-Fi**: The ESP32S2 will create a wireless network for debugging. On your computer, connect to the SSID named `blackmagic`, using the password `iamwitcher`.

### 3. Start GDB Client
- **Load the Binary in GDB**: Start the GDB client with your target binary using the following command:
  ```bash
  arm-none-eabi-gdb my_program.elf
  ```

### 4. Connect to the GDB Server on ESP32S2
- **Remote Debugging**: Once inside the GDB client, connect to the GDB server running on the ESP32S2. By default, the ESP32S2's IP address is `192.168.4.1` and the GDB server runs on port `2345`. Use this command to establish the connection:
  ```bash
  (gdb) target extended-remote 192.168.4.1:2345
  ```

### 5. Scan and Attach to the Target
- **SWD Scan**: Run the following command to scan for available SWD targets connected to the ESP32S2:
  ```bash
  (gdb) monitor swdp_scan
  ```
  This will output information about the target, including its voltage, device ID, and description. For example:
  ```
  Target voltage: 3.4V
  Device  IR Len  IDCODE      Description
  1       5       0x06413041  ST Microelectronics: STM32F4xx.
  ```

- **Attach to Target**: If the target is identified (e.g., an STM32F4xx device), you can attach to it using:
  ```bash
  (gdb) attach 1
  ```

This will attach the GDB client to the target for remote debugging.

You are now set to start debugging your Cortex-M (or Cortex-A) target using the Blackmagic Probe firmware running on the ESP32S2!

## Web interface development

Web interface is located in `components/svelte-portal` and written in Svelte. To build it, you need to install Node.js and run `npm install` in `components/svelte-portal` directory. Then you can run `npm run dev` to start development server or `npm run build` to build production version.

Typical workflow is to fix the board's IP address in `components/svelte-portal/src/lib/Api.svelte` and then run `npm run dev`. After that, you can open `http://localhost:5000` in your browser and see changes in the web interface in real time with live reload.

If you want to change local ip or port, you need to run `export HOST={ip} PORT={port}` before `npm run dev`.

```shell
export HOST=127.0.0.1 PORT=3000
npm run dev
```

When you're done, you need to run `npm run build`, `idf.py build` and then `idf.py -p <port> flash`. You can then open `http://blackmagic.local` in your browser and see the changes in the web interface.

```shell
npm run build
idf.py build
idf.py -p <port> flash
```

## Schematic

[Flipper Zero Wi-Fi Module Schematic](https://cdn.flipperzero.one/Flipper_Zero_WI-FI_Module_V1_Schematic.PDF)
