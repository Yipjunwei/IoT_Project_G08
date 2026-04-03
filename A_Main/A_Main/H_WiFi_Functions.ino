// =====================================================
// WIFI FUNCTIONS
// =====================================================

bool sendUdpMessage(IPAddress ip, uint16_t port, const String& msg) {
    // Measure power before sending
    float voltageBefore = M5.Axp.GetVinVoltage();
    float currentBefore = abs(M5.Axp.GetVinCurrent());
    float powerBefore = voltageBefore * currentBefore; // mW
    
    udp.beginPacket(ip, port);
    size_t written = udp.print(msg);
    bool ok = udp.endPacket() == 1;

    if (ok && written > 0) {
        // Measure power after sending
        delay(5); // Small delay to capture power spike
        float voltageAfter = M5.Axp.GetVinVoltage();
        float currentAfter = abs(M5.Axp.GetVinCurrent());
        float powerAfter = voltageAfter * currentAfter; // mW
        
        // Track WiFi energy based on current WiFi RSSI
        int rssi = WiFi.RSSI();
        trackWifiEnergy(rssi);
        
        // Print power consumption details
        Serial.print("[WIFI POWER] V:");
        Serial.print(voltageAfter, 2);
        Serial.print("V, I:");
        Serial.print(currentAfter, 1);
        Serial.print("mA, P:");
        Serial.print(powerAfter, 1);
        Serial.print("mW (delta:");
        Serial.print(powerAfter - powerBefore, 1);
        Serial.println("mW)");
    }

    return ok && written > 0;
}

void setupWifi() {
    Serial.println("[WIFI] setup start");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        Serial.print(".");
        tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WIFI] Connected IP=");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WIFI] Failed to connect");
    }

    udp.begin(WIFI_LOCAL_PORT);
    Serial.printf("[WIFI] UDP listening on %d\n", WIFI_LOCAL_PORT);
}

String buildWifiHelloPacket() {
    String data = "HELLO,";
    data += WiFi.localIP().toString();
    data += ",WIFI";
    return buildAuthenticatedPacket(data);
}

void sendWifiHello() {
    if (WiFi.status() != WL_CONNECTED) return;

    IPAddress bcast;
    if (!bcast.fromString(WIFI_BROADCAST_IP)) return;

    String hello = buildWifiHelloPacket();
    sendUdpMessage(bcast, WIFI_HELLO_PORT, hello);
    sendUdpMessage(bcast, WIFI_LOCAL_PORT, hello);

    Serial.print("[WIFI] HELLO sent: ");
    Serial.println(hello);
}

void sendLinkStateToGateway() {
    if (WiFi.status() != WL_CONNECTED) return;

    IPAddress gw;
    if (!gw.fromString(GATEWAY_IP)) return;

    String data = buildLinkStatePacket();
    String pkt = buildAuthenticatedPacket(data);
    sendUdpMessage(gw, GATEWAY_PORT, pkt);

    Serial.println("[SEND] Link-state sent to gateway");
}

void processGatewayMessage(const String& data) {
    if (data.startsWith("ROUTE|")) {
        currentRoute = data.substring(6);
        Serial.print("[ROUTE RECEIVED] ");
        Serial.println(currentRoute);
        return;
    }

    if (data.startsWith("ROUTE,")) {
        currentRoute = data.substring(6);
        Serial.print("[ROUTE RECEIVED] ");
        Serial.println(currentRoute);
    }
}

void processIncomingUdp(const String& msg) {
    if (!msg.startsWith("HELLO|")) return;

    int p1 = msg.indexOf('|');
    int p2 = msg.indexOf('|', p1 + 1);
    int p3 = msg.indexOf('|', p2 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0) return;

    String nodeId = msg.substring(p1 + 1, p2);
    String ip = msg.substring(p2 + 1, p3);
    int nodeNum = extractNodeNum(nodeId);

    // Use receiver's own AP RSSI — reflects our actual link quality,
    // not the sender's stale value from a different location.
    int rssi = WiFi.RSSI();

    upsertWifiNeighbour(nodeId, nodeNum, ip, rssi);
}

void pollUdp() {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return;

    char incoming[256];
    int len = udp.read(incoming, sizeof(incoming) - 1);
    if (len <= 0) return;
    incoming[len] = '\0';

    String msg = String(incoming);
    String nodeId;
    String data;
    String hash;
    unsigned long seq = 0;

    if (parseAuthenticatedPacket(msg, nodeId, seq, data, hash)) {
        if (!verifyAuthenticatedPacket(nodeId, seq, data, hash)) {
            Serial.println("[AUTH] UDP hash mismatch, dropped");
            return;
        }
        markNodeAuthenticated(nodeId);
        if (isDuplicatePacket(nodeId, "WIFI", seq)) {
            Serial.println("[DEDUPE] UDP duplicate dropped");
            return;
        }

        if (data.startsWith("HELLO,")) {
            int p1 = data.indexOf(',');
            int p2 = data.indexOf(',', p1 + 1);
            if (p1 < 0 || p2 < 0) return;

            String ip = data.substring(p1 + 1, p2);
            int nodeNum = extractNodeNum(nodeId);
            int rssi = WiFi.RSSI();
            upsertWifiNeighbour(nodeId, nodeNum, ip, rssi);
            return;
        }

        processGatewayMessage(data);
        return;
    }

    // Enforce auth for neighbour discovery:
    // unauthenticated packets are ignored to prevent fake/poisoned neighbours.
    if (msg.startsWith("HELLO|") || msg.startsWith("HELLO,")) {
        Serial.println("[AUTH] UDP unauth HELLO dropped");
        return;
    }

    // Optional compatibility for non-neighbour control messages.
    processGatewayMessage(msg);
}
