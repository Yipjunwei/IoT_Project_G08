// =====================================================
// BLE SCAN CALLBACK
// =====================================================
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String advName = advertisedDevice.getName().c_str();
        String nodeId;
        int otherNum = -1;

        if (!parseAndVerifyBleAdvertisedName(advName, nodeId, otherNum)) {
            Serial.print("[BLE AUTH] Rejected: ");
            Serial.println(advName);
            return;
        }

        if (nodeId == String(NODE_ID)) return;
        markNodeAuthenticated(nodeId);

        int rssi = advertisedDevice.getRSSI();

        Serial.print("[BLE SCAN] Seen: ");
        Serial.print(nodeId);
        Serial.print(" RSSI=");
        Serial.println(rssi);

        upsertBleNeighbour(nodeId, otherNum, rssi);
    }
};

// =====================================================
// BLE SETUP AND FUNCTIONS
// =====================================================
void startAdvertising() {
    String bleName = buildBleAdvertisedName();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.print("[BLE] Advertising as ");
    Serial.println(bleName);
}

void stopAdvertising() {
    if (pAdvertising != nullptr) {
        pAdvertising->stop();
        Serial.println("[BLE] Advertising stopped");
    }
}

void setupBle() {
    String bleName = buildBleAdvertisedName();
    BLEDevice::init(bleName.c_str());

    pServer = BLEDevice::createServer();
    pService = pServer->createService(SERVICE_UUID);
    pService->start();

    startAdvertising();

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
}

void runScanRound() {
    if (currentlyScanning) return;
    if (pBLEScan == nullptr) return;

    currentlyScanning = true;

    // Measure power before scanning
    float voltageBefore = M5.Axp.GetVinVoltage();
    float currentBefore = abs(M5.Axp.GetVinCurrent());
    float powerBefore = voltageBefore * currentBefore; // mW

    Serial.println("[BLE] Starting scan round...");
    stopAdvertising();

    pBLEScan->start(BLE_SCAN_DURATION_SEC, false);

    // Measure power after scanning
    float voltageAfter = M5.Axp.GetVinVoltage();
    float currentAfter = abs(M5.Axp.GetVinCurrent());
    float powerAfter = voltageAfter * currentAfter; // mW

    Serial.println("[BLE] Scan round complete");
    Serial.print("[BLE POWER] V:");
    Serial.print(voltageAfter, 2);
    Serial.print("V, I:");
    Serial.print(currentAfter, 1);
    Serial.print("mA, P:");
    Serial.print(powerAfter, 1);
    Serial.print("mW (delta:");
    Serial.print(powerAfter - powerBefore, 1);
    Serial.println("mW)");
    
    pBLEScan->clearResults();

    startAdvertising();
    currentlyScanning = false;
}
