// =====================================================
// GLOBAL VARIABLE DEFINITIONS
// =====================================================
WiFiUDP udp;

BLEScan* pBLEScan = nullptr;
BLEServer* pServer = nullptr;
BLEService* pService = nullptr;
BLEAdvertising* pAdvertising = nullptr;

NeighbourEntry neighbours[MAX_NEIGHBOURS];

bool currentlyScanning = false;
unsigned long lastUiRefreshMs = 0;
unsigned long lastLinkStateSendMs = 0;
unsigned long lastScanSlotRunMs = 0;
unsigned long lastWifiHelloMs = 0;
unsigned long lastLoraPollMs = 0;
String currentRoute = "";
int loraPacketCount = 0;
float totalLoraEnergy = 0.0f;
float loraEnergyPerPacket = 0.0f;
unsigned long txSequence = 1;
int displayScreen = 0;   // 0=MAIN, 1=WIFI, 2=BLE, 3=LORA

bool displayButtonLatch = false;
unsigned long lastDisplayButtonMs = 0;
