// =====================================================
// LINK METRIC HELPERS
// Fairer protocol-specific quality mapping
// =====================================================

// -------------------------
// Shared helper: linearly interpolate between two values
// given rssi clamped between rssiMin (weak) and rssiMax (strong).
// -------------------------
float linearFromRssi(int rssi, int rssiMin, int rssiMax,
                     float valAtMin, float valAtMax) {
    if (rssi >= rssiMax) return valAtMax;
    if (rssi <= rssiMin) return valAtMin;
    float t = (float)(rssi - rssiMin) / (float)(rssiMax - rssiMin);
    return valAtMin + t * (valAtMax - valAtMin);
}

// -------------------------
// BLE
// Useful range: -30 (very close) to -95 (near drop-out)
// -------------------------
float estimateBleLatencyFromRssi(int rssi) {
    return linearFromRssi(rssi, -95, -30, 120.0f, 15.0f);
}

float estimateBleReliabilityFromRssi(int rssi) {
    return linearFromRssi(rssi, -95, -30, 0.30f, 0.97f);
}

int normalizeBleRssiToQuality(int rssi) {
    if (rssi >= -55) return 100;
    if (rssi >= -65) return 85;
    if (rssi >= -75) return 65;
    if (rssi >= -85) return 40;
    if (rssi >= -95) return 20;
    return 10;
}

// -------------------------
// WIFI
// Routes through AP so base latency is higher than BLE.
// Useful range: -30 to -90 dBm.
// -------------------------
float estimateWifiLatencyFromRssi(int rssi) {
    return linearFromRssi(rssi, -90, -30, 150.0f, 25.0f);
}

float estimateWifiReliabilityFromRssi(int rssi) {
    return linearFromRssi(rssi, -90, -30, 0.25f, 0.97f);
}

int normalizeWifiRssiToQuality(int rssi) {
    if (rssi >= -45) return 100;
    if (rssi >= -55) return 85;
    if (rssi >= -65) return 70;
    if (rssi >= -75) return 50;
    if (rssi >= -85) return 25;
    return 10;
}

// Penalty is now baked into the latency model above — no-op kept
// for API compatibility.
int applyWifiInfrastructurePenalty(int quality) {
    return quality;
}

// -------------------------
// LORA
// -------------------------
float estimateLoraReliabilityFromRssi(int rssi) {
    if (rssi >= -90) return 0.98f;
    if (rssi >= -100) return 0.90f;
    if (rssi >= -110) return 0.78f;
    if (rssi >= -118) return 0.60f;
    return 0.40f;
}

int normalizeLoraRssiToQuality(int rssi) {
    if (rssi >= -90) return 100;
    if (rssi >= -100) return 85;
    if (rssi >= -110) return 65;
    if (rssi >= -118) return 40;
    if (rssi >= -124) return 20;
    return 10;
}