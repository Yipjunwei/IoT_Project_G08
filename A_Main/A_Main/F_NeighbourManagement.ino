// =====================================================
// NEIGHBOUR MANAGEMENT
// =====================================================

void clearNeighbours() {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        neighbours[i].nodeId = "";
        neighbours[i].nodeNum = -1;
        neighbours[i].wifiIp = "";
        neighbours[i].lastSeenMs = 0;
        neighbours[i].active = false;

        neighbours[i].ble  = {false, 0, 0.0f, 0.0f, 0.0f, 0};
        neighbours[i].wifi = {false, 0, 0.0f, 0.0f, 0.0f, 0};
        neighbours[i].lora = {false, 0, 0.0f, 0.0f, 0.0f, 0};
    }
}

int findNeighbourIndexById(const String& nodeId) {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (neighbours[i].active && neighbours[i].nodeId == nodeId) {
            return i;
        }
    }
    return -1;
}

int findFreeNeighbourSlot() {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].active) {
            return i;
        }
    }
    return -1;
}

int upsertNeighbourBase(const String& nodeId, int nodeNum) {
    if (nodeId == String(NODE_ID)) return -1;
    if (nodeNum <= 0) return -1;

    int idx = findNeighbourIndexById(nodeId);
    if (idx < 0) idx = findFreeNeighbourSlot();
    if (idx < 0) return -1;

    neighbours[idx].nodeId = nodeId;
    neighbours[idx].nodeNum = nodeNum;
    neighbours[idx].lastSeenMs = millis();
    neighbours[idx].active = true;
    return idx;
}

void upsertBleNeighbour(const String& nodeId, int nodeNum, int rssi) {
    int idx = upsertNeighbourBase(nodeId, nodeNum);
    if (idx < 0) return;

    neighbours[idx].ble.available = true;
    neighbours[idx].ble.rssi = rssi;
    neighbours[idx].ble.latencyMs = estimateBleLatencyFromRssi(rssi);
    neighbours[idx].ble.energyCost = estimateBleEnergy(rssi);
    neighbours[idx].ble.reliability = estimateBleReliabilityFromRssi(rssi);
    neighbours[idx].ble.lastSeenMs = millis();

    trackBleEnergy(rssi);

    Serial.print("[BLE] Upsert neighbour: ");
    Serial.print(nodeId);
    Serial.print(" RSSI=");
    Serial.println(rssi);
}

void upsertWifiNeighbour(const String& nodeId, int nodeNum, const String& ip, int rssi) {
    int idx = upsertNeighbourBase(nodeId, nodeNum);
    if (idx < 0) return;

    neighbours[idx].wifiIp = ip;
    neighbours[idx].wifi.available = true;
    neighbours[idx].wifi.rssi = rssi;
    neighbours[idx].wifi.latencyMs = estimateWifiLatencyFromRssi(rssi);
    neighbours[idx].wifi.energyCost = estimateWifiEnergy(rssi);
    neighbours[idx].wifi.reliability = estimateWifiReliabilityFromRssi(rssi);
    neighbours[idx].wifi.lastSeenMs = millis();

    Serial.print("[WIFI] Upsert neighbour: ");
    Serial.print(nodeId);
    Serial.print(" IP=");
    Serial.print(ip);
    Serial.print(" RSSI=");
    Serial.println(rssi);
}

void upsertLoraNeighbour(const String& nodeId, int nodeNum, int rssi, float latencyMs, float energyMj, float reliability) {
    int idx = upsertNeighbourBase(nodeId, nodeNum);
    if (idx < 0) return;

    neighbours[idx].lora.available = true;
    neighbours[idx].lora.rssi = rssi;
    neighbours[idx].lora.latencyMs = latencyMs;
    neighbours[idx].lora.energyCost = energyMj;
    neighbours[idx].lora.reliability = reliability;
    neighbours[idx].lora.lastSeenMs = millis();

    loraPacketCount++;
    totalLoraEnergy += energyMj;
    if (loraPacketCount > 0) {
        loraEnergyPerPacket = totalLoraEnergy / loraPacketCount;
    }

    Serial.print("[LORA] Upsert neighbour: ");
    Serial.print(nodeId);
    Serial.print(" RSSI=");
    Serial.println(rssi);
}
void expireNeighbours() {
    unsigned long now = millis();
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].active) continue;

        // Expire each protocol independently based on its own lastSeenMs
        if (neighbours[i].ble.available &&
            now - neighbours[i].ble.lastSeenMs > NEIGHBOUR_TIMEOUT_MS) {
            neighbours[i].ble.available = false;
            Serial.print("[NBR] BLE expired: ");
            Serial.println(neighbours[i].nodeId);
        }
        if (neighbours[i].wifi.available &&
            now - neighbours[i].wifi.lastSeenMs > NEIGHBOUR_TIMEOUT_MS) {
            neighbours[i].wifi.available = false;
            Serial.print("[NBR] WiFi expired: ");
            Serial.println(neighbours[i].nodeId);
        }
        if (neighbours[i].lora.available &&
            now - neighbours[i].lora.lastSeenMs > NEIGHBOUR_TIMEOUT_MS) {
            neighbours[i].lora.available = false;
            Serial.print("[NBR] LoRa expired: ");
            Serial.println(neighbours[i].nodeId);
        }

        // Mark neighbour fully inactive only when all protocols gone
        if (!neighbours[i].ble.available &&
            !neighbours[i].wifi.available &&
            !neighbours[i].lora.available) {
            neighbours[i].active = false;
            Serial.print("[NBR] Fully expired: ");
            Serial.println(neighbours[i].nodeId);
        }
    }
}

int getActiveNeighbourCount() {
    int count = 0;
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (neighbours[i].active) count++;
    }
    return count;
}

int findBestBleNeighbourForLatency() {
    int bestIdx = -1;
    float bestCost = 999999.0f;

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].ble.available) continue;

        if (neighbours[i].ble.latencyMs < bestCost) {
            bestCost = neighbours[i].ble.latencyMs;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int findBestWifiNeighbourForLatency() {
    int bestIdx = -1;
    float bestCost = 999999.0f;

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].wifi.available) continue;

        if (neighbours[i].wifi.latencyMs < bestCost) {
            bestCost = neighbours[i].wifi.latencyMs;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int findBestLoraNeighbourForLatency() {
    int bestIdx = -1;
    float bestCost = 999999.0f;

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].lora.available) continue;

        if (neighbours[i].lora.latencyMs < bestCost) {
            bestCost = neighbours[i].lora.latencyMs;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int findNearestNeighbour() {
    int bestIdx = -1;
    int bestRssi = -999;

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].active) continue;

        if (neighbours[i].ble.available && neighbours[i].ble.rssi > bestRssi) {
            bestRssi = neighbours[i].ble.rssi;
            bestIdx = i;
        }

        if (neighbours[i].wifi.available && neighbours[i].wifi.rssi > bestRssi) {
            bestRssi = neighbours[i].wifi.rssi;
            bestIdx = i;
        }

        if (neighbours[i].lora.available && neighbours[i].lora.rssi > bestRssi) {
            bestRssi = neighbours[i].lora.rssi;
            bestIdx = i;
        }
    }

    return bestIdx;
}

String chooseBetterProtocolForNeighbour(const NeighbourEntry& n) {
    bool hasBle = n.ble.available;
    bool hasWifi = n.wifi.available;
    bool hasLora = n.lora.available;

    String bestProto = "NONE";
    float bestLatency = 999999.0f;

    if (hasBle && n.ble.latencyMs < bestLatency) {
        bestLatency = n.ble.latencyMs;
        bestProto = "BLE";
    }

    if (hasWifi && n.wifi.latencyMs < bestLatency) {
        bestLatency = n.wifi.latencyMs;
        bestProto = "WIFI";
    }

    if (hasLora && n.lora.latencyMs < bestLatency) {
        bestLatency = n.lora.latencyMs;
        bestProto = "LORA";
    }

    return bestProto;
}

void printNeighbourTable() {
    Serial.println("========== NEIGHBOUR TABLE ==========");

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].active) continue;

        Serial.print("#");
        Serial.print(i);
        Serial.print(" ");
        Serial.print(neighbours[i].nodeId);
        Serial.print(" (nodeNum=");
        Serial.print(neighbours[i].nodeNum);
        Serial.println(")");

        if (neighbours[i].ble.available) {
            Serial.print("  BLE  | RSSI=");
            Serial.print(neighbours[i].ble.rssi);
            Serial.print(" Lat=");
            Serial.print(neighbours[i].ble.latencyMs, 1);
            Serial.print(" Energy=");
            Serial.print(neighbours[i].ble.energyCost, 2);
            Serial.print(" Rel=");
            Serial.println(neighbours[i].ble.reliability, 2);
        }

        if (neighbours[i].wifi.available) {
            Serial.print("  WIFI | RSSI=");
            Serial.print(neighbours[i].wifi.rssi);
            Serial.print(" Lat=");
            Serial.print(neighbours[i].wifi.latencyMs, 1);
            Serial.print(" Energy=");
            Serial.print(neighbours[i].wifi.energyCost, 2);
            Serial.print(" Rel=");
            Serial.print(neighbours[i].wifi.reliability, 2);
            Serial.print(" IP=");
            Serial.println(neighbours[i].wifiIp);
        }

        if (neighbours[i].lora.available) {
            Serial.print("  LORA | RSSI=");
            Serial.print(neighbours[i].lora.rssi);
            Serial.print(" Lat=");
            Serial.print(neighbours[i].lora.latencyMs, 1);
            Serial.print(" Energy=");
            Serial.print(neighbours[i].lora.energyCost, 2);
            Serial.print(" Rel=");
            Serial.println(neighbours[i].lora.reliability, 2);
        }

        Serial.println("-------------------------------------");
    }

    Serial.println("=====================================");
}