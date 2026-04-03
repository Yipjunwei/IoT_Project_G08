#include <WiFi.h>
#include <WiFiUdp.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <M5StickCPlus.h>

// =====================================================
// CHANGE THESE FOR EACH NODE
// =====================================================
#define NODE_ID             "NODE_1"
#define NODE_NUM            1
#define BLE_DEVICE_NAME     "NODE_1"

// =====================================================
// PACKET AUTH SETTINGS
// =====================================================
#define PACKET_AUTH_KEY     "mesh_shared_key_v1"
//#define PACKET_AUTH_KEY     "fakekey"

// =====================================================
// WIFI SETTINGS
// =====================================================
#define WIFI_SSID           "Jw"
#define WIFI_PASSWORD       "junwei123123"

#define WIFI_HELLO_PORT     41000
#define WIFI_LOCAL_PORT     41002
#define WIFI_BROADCAST_IP   "255.255.255.255"

// Gateway
#define GATEWAY_IP          "172.20.10.12"
#define GATEWAY_PORT        5005

// =====================================================
// BLE SETTINGS
// =====================================================
#define SERVICE_UUID           "12345678-1234-1234-1234-1234567890ab"
#define MAX_NEIGHBOURS         10

#define BLE_SCAN_WINDOW_MS     2000
#define BLE_SCAN_CYCLE_MS      10000
#define BLE_SCAN_DURATION_SEC  1

#define NEIGHBOUR_TIMEOUT_MS   20000
#define DISPLAY_REFRESH_MS     500
#define LINKSTATE_SEND_MS      5000
#define WIFI_HELLO_INTERVAL_MS 5000

// =====================================================
// LORA SETTINGS
// =====================================================
#define LORA_ENABLED        true    
#define LORA_SERIAL         Serial2  // Use Serial2 for LoRa bridge communication
#define LORA_BAUD           9600
#define LORA_RX_PIN         33      // M5StickC GPIO33 (G33) Connected to LoRa D1 (TX)
#define LORA_POLL_MS        100     // Poll LoRa bridge every 100ms

#define BLE_SCAN_INTERVAL_MS 2000          // scan every 2s
#define LINK_STATE_SEND_INTERVAL_MS 5000   // send every 5s


// =====================================================
// DATA STRUCTURES 
// =====================================================
struct LinkMetric {
    bool available;
    int rssi;
    float latencyMs;
    float energyCost;
    float reliability;
    unsigned long lastSeenMs;  // per-protocol expiry
};

struct NeighbourEntry {
    String nodeId;
    int nodeNum;
    String wifiIp;
    unsigned long lastSeenMs;
    bool active;

    LinkMetric ble;
    LinkMetric wifi;
    LinkMetric lora; 
};

// =====================================================
// GLOBALS - Declared here, defined in other files
// =====================================================
extern WiFiUDP udp;

extern BLEScan* pBLEScan;
extern BLEServer* pServer;
extern BLEService* pService;
extern BLEAdvertising* pAdvertising;

extern NeighbourEntry neighbours[MAX_NEIGHBOURS];

extern bool currentlyScanning;
extern unsigned long lastUiRefreshMs;
extern unsigned long lastLinkStateSendMs;
extern unsigned long lastScanSlotRunMs;
extern unsigned long lastWifiHelloMs;

extern String currentRoute;

// Energy model variables
extern float BLE_ENERGY_COST_BASE;
extern float WIFI_ENERGY_COST_BASE;
extern float totalEnergy;
extern unsigned long lastEnergyTime;
extern float bleEnergyStart;
extern float bleEnergyPerPacket;
extern int blePacketCount;
extern float wifiEnergyStart;
extern float wifiEnergyPerPacket;
extern int wifiPacketCount;
extern float totalBleEnergy;
extern float totalWifiEnergy;

// LoRa globals
extern unsigned long lastLoraPollMs;
extern int loraPacketCount;
extern float loraEnergyPerPacket;
extern float totalLoraEnergy;
extern unsigned long txSequence;

// Forward declarations for functions used across files
void upsertLoraNeighbour(const String& nodeId, int nodeNum, int rssi, float latencyMs, float energyMj, float reliability);

// =====================================================
// Arduino setup/loop
// =====================================================
void setupWifi();
void setupBle();
void setupLora();
void updateDisplay();

void pollUdp();
void pollLoRa();
void sendLoraMessage(const String& message);
void sendWifiHello();
void runScanRound();
void printNeighbourTable();
void sendLinkStateToGateway();
void printNeighbourTable();

void clearNeighbours();
void expireNeighbours();

void upsertBleNeighbour(const String& nodeId, int nodeNum, int rssi);
void upsertWifiNeighbour(const String& nodeId, int nodeNum, const String& ip, int rssi);
void upsertLoraNeighbour(const String& nodeId, int nodeNum, int rssi, float latencyMs, float energyMj, float reliability);

float estimateBleLatencyFromRssi(int rssi);
float estimateBleReliabilityFromRssi(int rssi);

float estimateWifiLatencyFromRssi(int rssi);
float estimateWifiReliabilityFromRssi(int rssi);

float estimateLoraReliabilityFromRssi(int rssi);

int normalizeBleRssiToQuality(int rssi);
int normalizeWifiRssiToQuality(int rssi);
int normalizeLoraRssiToQuality(int rssi);
int applyWifiInfrastructurePenalty(int quality);

void setup() {
    M5.begin();
    M5.Axp.ScreenBreath(15);
    Serial.begin(115200);
    delay(1000);
    
    delay(NODE_NUM * 200);
    lastEnergyTime = millis();
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(5, 10);
    M5.Lcd.println("BOOT");

    Serial.println();
    Serial.println("==========================================");
    Serial.println("BLE + WIFI Node");
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    Serial.print("Node Num: ");
    Serial.println(NODE_NUM);
    Serial.println("==========================================");

    clearNeighbours();
    setupWifi();
    setupBle();
    setupLora();  // ← Added LoRa setup
    updateDisplay();

    lastUiRefreshMs = millis();
    lastLinkStateSendMs = millis();
    lastScanSlotRunMs = 0;
    lastWifiHelloMs = millis();
    lastLoraPollMs = millis();
}

void loop() {
    M5.update();   // always first

    pollUdp();

    if (LORA_ENABLED) {
        pollLoRa();
    }

    unsigned long now = millis();

    if (now - lastWifiHelloMs >= WIFI_HELLO_INTERVAL_MS) {
        sendWifiHello();
        lastWifiHelloMs = now;
    }

    if (now - lastScanSlotRunMs >= BLE_SCAN_INTERVAL_MS) {
        runScanRound();
        lastScanSlotRunMs = now;
    }

    expireNeighbours();

    if (now - lastLinkStateSendMs >= LINK_STATE_SEND_INTERVAL_MS) {
        printNeighbourTable();
        sendLinkStateToGateway();
        sendLoraMessage(buildLinkStatePacket());
        lastLinkStateSendMs = now;
    }

    updateDisplay();
}
