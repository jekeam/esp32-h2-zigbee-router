# ESP32-H2 Zigbee Router 20 dBm

Arduino firmware for an ESP32-H2 Zigbee range extender/router.

## Behavior

- Zigbee role: router/range extender.
- IEEE 802.15.4 TX power is requested at `20 dBm`.
- BOOT button held for 3 seconds resets Zigbee NVRAM, then reboots.
- RGB LED on GPIO8:
  - blue while the router is not joined to a coordinator;
  - red during BOOT 3s factory reset and until the Zigbee stack starts after reboot;
  - dim green after the router joins the coordinator.

## Arduino Settings

Use ESP32 Arduino core `3.3.8` or newer.

Board:

```text
ESP32H2 Dev Module
```

Important options:

```text
Zigbee Mode: Zigbee ZCZR (coordinator/router)
Partition Scheme: Zigbee ZCZR 4MB with spiffs
USB CDC On Boot: Enabled
Upload port: COM20
```

Equivalent `arduino-cli` command:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32h2:CDCOnBoot=cdc,PartitionScheme=zigbee_zczr,FlashMode=qio,FlashSize=4M,UploadSpeed=921600,DebugLevel=info,EraseFlash=none,ZigbeeMode=zczr" .
arduino-cli upload -p COM20 --fqbn "esp32:esp32:esp32h2:CDCOnBoot=cdc,PartitionScheme=zigbee_zczr,FlashMode=qio,FlashSize=4M,UploadSpeed=921600,DebugLevel=info,EraseFlash=none,ZigbeeMode=zczr" .
```
