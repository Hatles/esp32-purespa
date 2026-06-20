# Over-The-Air (OTA) Firmware Updates

This guide explains how to compile the firmware binary for the ESP32 PureSpa Controller and update the device wirelessly via the web dashboard.

---

## 1. How OTA Works

To support wireless updates safely, the ESP32's flash memory layout is configured with a **Dual OTA Partition Table** (`partitions_two_ota.csv`):
* **`ota_0`**: Holds the active firmware application.
* **`ota_1`**: Holds the backup/passive application.

When you upload a new binary file (`.bin`) through the Web UI:
1. The ESP32 streams the binary data directly into the passive partition.
2. Once the download completes, the ESP32 validates the checksum and cryptographic signature (if configured).
3. If valid, it changes the boot registers to point to the new partition as the active boot partition.
4. The ESP32 reboots, launching the updated version. If the new partition fails to boot, the ESP-IDF roll-back mechanism automatically restores the previous working version.

---

## 2. Prerequisites

* **ESP-IDF v5.5.2** (or compatible) installed and configured on your system.
* **Node.js** installed (if you make changes to the Web UI in `main/index.html` or `main/config.html` and need to re-compress the files).

---

## 3. Step 1: Compiling the Firmware

1. Open your terminal (e.g. ESP-IDF Command Prompt on Windows or bash/zsh on macOS/Linux).
2. Navigate to the project root directory:
   ```bash
   cd c:\DEV\esp32-purespa
   ```
3. (Optional) If you have modified `index.html` or `config.html`, compress them to generate the updated `.gz` payloads:
   ```bash
   cd mock-backend
   npm run compress
   cd ..
   ```
4. Build the project using `idf.py`:
   ```bash
   idf.py build
   ```
5. Once the build finishes successfully, you will find the generated binary at:
   ```text
   build/esp32-purespa.bin
   ```

---

## 4. Step 2: Accessing the Admin Panel

1. Connect your computer or phone to the same Wi-Fi network as the ESP32.
2. Open your web browser and navigate to:
   * **`http://purespa.local/`** (via mDNS)
   * *Alternatively*, navigate to the IP address of your ESP32 (e.g., `http://192.168.1.150/`).
3. Click the **Settings Gear Icon** in the top-right corner of the dashboard to open the administrative panel.

---

## 5. Step 3: Upgrading the Device

1. Scroll down the administration panel to the **Firmware Update (OTA)** section.
2. Click **Choose File (.bin)**.
3. Locate and select the compiled firmware file: `build/esp32-purespa.bin`.
4. The upload starts automatically. You will see a real-time progress bar tracking the transfer percentage.
5. When the upload reaches **100%**, a message will announce that the update was successful and the ESP32 is rebooting.
6. The browser will reload after a few seconds, connecting back to the updated dashboard.

> [!WARNING]
> Do not disconnect the power supply or power down the ESP32 during the update process. Doing so may corrupt the active write partition, forcing you to re-flash the board via USB.
