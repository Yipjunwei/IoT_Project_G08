// =====================================================
// ENERGY MODEL (REALISTIC - RSSI-BASED)
// =====================================================

// Base energy costs per operation (mJ) - calibrated values
// These represent typical energy costs for each protocol
float BLE_ENERGY_COST_BASE = 1.0;
float WIFI_ENERGY_COST_BASE = 2.2;
float totalEnergy = 0;
unsigned long lastEnergyTime = 0;

float bleEnergyStart = 0;
float bleEnergyPerPacket = 0;
int blePacketCount = 0;

float wifiEnergyStart = 0;
float wifiEnergyPerPacket = 0;
int wifiPacketCount = 0;

// Track actual measured energy consumption
float totalBleEnergy = 0;
float totalWifiEnergy = 0;

// More granular energy scaling based on RSSI
// Lower RSSI (weaker signal) = more retransmissions = more energy
float scaleEnergyByRssi(int rssi) {
    // Convert RSSI to energy multiplier with smooth scaling
    // RSSI range typically: -30 (very close) to -90 (very far)
    
    if (rssi >= -40) return 0.8;   // Very strong signal - efficient
    if (rssi >= -50) return 1.0;   // Strong signal - baseline
    if (rssi >= -60) return 1.3;   // Good signal
    if (rssi >= -70) return 1.7;   // Moderate signal
    if (rssi >= -80) return 2.3;   // Weak signal
    return 3.0;                     // Very weak signal - many retries
}

float estimateBleEnergy(int rssi) {
    // BLE energy varies more with distance due to connection overhead
    float scaleFactor = scaleEnergyByRssi(rssi);
    return BLE_ENERGY_COST_BASE * scaleFactor;
}

float estimateWifiEnergy(int rssi) {
    // WiFi has higher base cost but scales differently
    float scaleFactor = scaleEnergyByRssi(rssi);
    return WIFI_ENERGY_COST_BASE * scaleFactor;
}

void updateEnergyMeasurement() {
    float voltage = M5.Axp.GetVinVoltage();     // better than BatVoltage
    float current = abs(M5.Axp.GetVinCurrent());   // mA

    float power = voltage * current; // mW

    unsigned long now = millis();
    float dt = (now - lastEnergyTime) / 1000.0;

    totalEnergy += power * dt; // mJ

    lastEnergyTime = now;
}

// Print comprehensive power and energy statistics
void printPowerStats() {
    float voltage = M5.Axp.GetVinVoltage();
    float current = abs(M5.Axp.GetVinCurrent());
    float power = voltage * current;
    
    Serial.println("========== POWER STATS ==========");
    Serial.print("Current Power: ");
    Serial.print(power, 2);
    Serial.print(" mW (");
    Serial.print(voltage, 2);
    Serial.print("V @ ");
    Serial.print(current, 1);
    Serial.println(" mA)");
    
    Serial.print("Total Energy: ");
    Serial.print(totalEnergy / 1000.0, 2);
    Serial.println(" J");
    
    if (blePacketCount > 0) {
        Serial.print("BLE: ");
        Serial.print(blePacketCount);
        Serial.print(" pkts, avg ");
        Serial.print(bleEnergyPerPacket, 2);
        Serial.print(" mJ/pkt, total ");
        Serial.print(totalBleEnergy, 2);
        Serial.println(" mJ");
    }
    
    if (wifiPacketCount > 0) {
        Serial.print("WiFi: ");
        Serial.print(wifiPacketCount);
        Serial.print(" pkts, avg ");
        Serial.print(wifiEnergyPerPacket, 2);
        Serial.print(" mJ/pkt, total ");
        Serial.print(totalWifiEnergy, 2);
        Serial.println(" mJ");
    }
    
    if (blePacketCount > 0 && wifiPacketCount > 0) {
        float efficiency = (bleEnergyPerPacket / wifiEnergyPerPacket) * 100.0;
        Serial.print("BLE is ");
        Serial.print(efficiency, 1);
        Serial.println("% of WiFi energy cost");
    }
    
    Serial.println("=================================");
}

// Track BLE energy usage
void trackBleEnergy(int rssi) {
    float energyCost = estimateBleEnergy(rssi);
    totalBleEnergy += energyCost;
    blePacketCount++;
    bleEnergyPerPacket = totalBleEnergy / blePacketCount;
    
    Serial.print("[ENERGY] BLE RSSI=");
    Serial.print(rssi);
    Serial.print(" cost=");
    Serial.print(energyCost, 2);
    Serial.print(" mJ, avg=");
    Serial.print(bleEnergyPerPacket, 2);
    Serial.print(" mJ (");
    Serial.print(blePacketCount);
    Serial.println(" pkts)");
}

// Track WiFi energy usage
void trackWifiEnergy(int rssi) {
    float energyCost = estimateWifiEnergy(rssi);
    totalWifiEnergy += energyCost;
    wifiPacketCount++;
    wifiEnergyPerPacket = totalWifiEnergy / wifiPacketCount;
    
    Serial.print("[ENERGY] WiFi RSSI=");
    Serial.print(rssi);
    Serial.print(" cost=");
    Serial.print(energyCost, 2);
    Serial.print(" mJ, avg=");
    Serial.print(wifiEnergyPerPacket, 2);
    Serial.print(" mJ (");
    Serial.print(wifiPacketCount);
    Serial.println(" pkts)");
}