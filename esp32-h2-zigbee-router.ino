#include <Arduino.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Select Tools -> Zigbee Mode -> Zigbee ZCZR (coordinator/router)."
#endif

#include <Adafruit_NeoPixel.h>
#include <Zigbee.h>
#include <esp_err.h>
#include <esp_ieee802154.h>

// ESP32-H2 Super Mini: BOOT is GPIO9 and the onboard WS2812 is usually GPIO8.
#define BOOT_BUTTON_PIN BOOT_PIN
#define WS2812_GPIO 8
#define WS2812_LEDS 1

#define ZIGBEE_EXTENDER_ENDPOINT 1
#define ZIGBEE_MAX_CHILDREN 20
#define ZIGBEE_TX_POWER_DBM 20

static constexpr uint32_t RESET_HOLD_MS = 3000;
static constexpr uint32_t LED_REFRESH_MS = 250;
static constexpr uint32_t RESET_MARKER = 0xB00720DB;

RTC_DATA_ATTR static uint32_t g_resetMarker = 0;

static Adafruit_NeoPixel pixels(WS2812_LEDS, WS2812_GPIO, NEO_GRB + NEO_KHZ800);
static ZigbeeRangeExtender zbExtender(ZIGBEE_EXTENDER_ENDPOINT);

static bool g_zigbeeStarted = false;
static bool g_resetInProgress = false;
static bool g_txPowerAppliedAfterJoin = false;
static bool g_lastConnected = false;
static uint32_t g_lastLedRefresh = 0;

static void setLed(uint8_t brightness, uint8_t r, uint8_t g, uint8_t b) {
  pixels.setBrightness(brightness);
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

static void setLedBlueNotJoined() {
  setLed(80, 0, 0, 140);
}

static void setLedRedReset() {
  setLed(110, 180, 0, 0);
}

static void setLedDimGreenJoined() {
  setLed(8, 0, 120, 0);
}

static void updateStatusLed(bool force = false) {
  const uint32_t now = millis();
  if (!force && (now - g_lastLedRefresh) < LED_REFRESH_MS) {
    return;
  }
  g_lastLedRefresh = now;

  if (g_resetInProgress || (g_resetMarker == RESET_MARKER && !g_zigbeeStarted)) {
    setLedRedReset();
    return;
  }

  if (Zigbee.connected()) {
    setLedDimGreenJoined();
  } else {
    setLedBlueNotJoined();
  }
}

static void applyZigbeeTxPower(const char *reason) {
  esp_err_t err = esp_ieee802154_set_txpower(ZIGBEE_TX_POWER_DBM);
  int8_t actual = esp_ieee802154_get_txpower();

  if (err == ESP_OK) {
    Serial.printf("[ZB] TX power requested: %d dBm (%s), current: %d dBm\n",
                  ZIGBEE_TX_POWER_DBM, reason, actual);
  } else {
    Serial.printf("[ZB] TX power request failed: %s (%s), current: %d dBm\n",
                  esp_err_to_name(err), reason, actual);
  }
  Serial.flush();
}

static void identify(uint16_t time) {
  Serial.printf("[ZB] identify requested for %u seconds\n", time);
  Serial.flush();
  if (time > 0) {
    setLed(60, 120, 120, 120);
  } else {
    updateStatusLed(true);
  }
}

static void handleBootButton() {
  static bool wasPressed = false;
  static bool resetRequested = false;
  static uint32_t pressedAt = 0;

  const bool pressed = digitalRead(BOOT_BUTTON_PIN) == LOW;
  const uint32_t now = millis();

  if (pressed && !wasPressed) {
    wasPressed = true;
    resetRequested = false;
    pressedAt = now;
  }

  if (pressed && !resetRequested && (now - pressedAt) >= RESET_HOLD_MS) {
    resetRequested = true;
    g_resetInProgress = true;
    g_resetMarker = RESET_MARKER;
    setLedRedReset();

    Serial.println("[BOOT] 3s hold detected. Resetting Zigbee NVRAM; release BOOT to reboot.");
    Serial.flush();
    Zigbee.factoryReset(false);
  }

  if (!pressed && wasPressed) {
    wasPressed = false;
    if (resetRequested) {
      Serial.println("[BOOT] Rebooting after Zigbee factory reset.");
      Serial.flush();
      delay(150);
      ESP.restart();
    }
  }
}

static void updateZigbeeJoinState() {
  const bool connected = Zigbee.connected();
  if (connected != g_lastConnected) {
    g_lastConnected = connected;
    Serial.printf("[ZB] coordinator link: %s\n", connected ? "joined" : "not joined");
    Serial.flush();
    updateStatusLed(true);
  }

  if (connected && !g_txPowerAppliedAfterJoin) {
    applyZigbeeTxPower("joined coordinator");
    g_txPowerAppliedAfterJoin = true;
  } else if (!connected) {
    g_txPowerAppliedAfterJoin = false;
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 2000) {
    delay(10);
  }

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  pixels.begin();
  pixels.clear();
  pixels.show();

  if (g_resetMarker == RESET_MARKER) {
    g_resetInProgress = true;
    setLedRedReset();
  } else {
    setLedBlueNotJoined();
  }

  Serial.println();
  Serial.println("Boot: ESP32-H2 Zigbee Router 20 dBm");
  Serial.printf("Pins: WS2812=%u, BOOT=%u\n", WS2812_GPIO, BOOT_BUTTON_PIN);
  Serial.printf("Zigbee: endpoint=%u, max_children=%u, tx_power=%d dBm\n",
                ZIGBEE_EXTENDER_ENDPOINT, ZIGBEE_MAX_CHILDREN, ZIGBEE_TX_POWER_DBM);
  Serial.flush();

  zbExtender.onIdentify(identify);
  zbExtender.setManufacturerAndModel("SAVA_Lab", "ESP32-H2 ZigBee Router");
  zbExtender.setPowerSource(ZB_POWER_SOURCE_MAINS);

  Zigbee.addEndpoint(&zbExtender);
  Zigbee.setTimeout(30000);

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ROUTER_CONFIG();
  zigbeeConfig.nwk_cfg.zczr_cfg.max_children = ZIGBEE_MAX_CHILDREN;

  Serial.println("[ZB] Starting Zigbee router stack...");
  Serial.flush();
  g_zigbeeStarted = Zigbee.begin(&zigbeeConfig);

  if (!g_zigbeeStarted) {
    Serial.println("[ZB] Zigbee failed to start, rebooting.");
    Serial.flush();
    setLedRedReset();
    delay(1500);
    ESP.restart();
  }

  applyZigbeeTxPower("stack started");
  g_resetMarker = 0;
  g_resetInProgress = false;
  updateStatusLed(true);

  Serial.println("[ZB] Stack is up. Waiting for coordinator join if needed.");
  Serial.flush();
}

void loop() {
  handleBootButton();
  updateZigbeeJoinState();
  updateStatusLed();
  delay(20);
}
