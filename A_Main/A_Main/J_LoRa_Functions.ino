// =====================================================
// LORA FUNCTIONS (via Serial Bridge)
// =====================================================

void setupLora() {
    if (!LORA_ENABLED) {
        Serial.println("[LORA] Disabled in config");
        return;
    }

    LORA_SERIAL.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
    delay(500);

    Serial.println("[LORA] Bridge initialized");
    Serial.print("[LORA] RX=GPIO");
    Serial.print(LORA_RX_PIN);
    Serial.println(" (connected to LoRa D1/TX)");
    if (LORA_TX_PIN >= 0) {
        Serial.print("[LORA] TX=GPIO");
        Serial.println(LORA_TX_PIN);
    } else {
        Serial.println("[LORA] TX=Not connected (receive-only mode)");
    }
}

void pollLoRa() {
    if (!LORA_ENABLED) return;

    // ADD DEBUG
    if (LORA_SERIAL.available()) {
        Serial.print("[LORA DEBUG] Bytes available: ");
        Serial.println(LORA_SERIAL.available());
    }

    while (LORA_SERIAL.available()) {
        String line = LORA_SERIAL.readStringUntil('\n');
        line.trim();

        Serial.print("[LORA DEBUG] Received: ");  // ADD THIS
        Serial.println(line);                       // ADD THIS

        if (line.length() == 0) continue;

        if (line.startsWith("RX:")) {
            String payload = line.substring(3);
            payload.trim();

            String nodeId;
            String data;
            String hash;
            unsigned long seq = 0;

            if (parseAuthenticatedPacket(payload, nodeId, seq, data, hash)) {
                if (!verifyAuthenticatedPacket(nodeId, seq, data, hash)) {
                    Serial.println("[AUTH] LoRa hash mismatch, dropped");
                    continue;
                }
                markNodeAuthenticated(nodeId);
                if (isDuplicatePacket(nodeId, "LORA", seq)) {
                    Serial.println("[DEDUPE] LoRa duplicate dropped");
                    continue;
                }

                // For now, authenticated LoRa payload carries JSON data.
                processLoraPacket(data);
                continue;
            }

            String fallbackNode = getLoraNodeIdFromJson(payload);
            if (fallbackNode.length() > 0 && isNodeAuthenticated(fallbackNode)) {
                Serial.println("[AUTH] LoRa fallback accepted (trusted node)");
                processLoraPacket(payload);
                continue;
            }

            Serial.println("[AUTH] LoRa unauth packet dropped");
            continue;
        } 
        else if (line.startsWith("{")) {
            String fallbackNode = getLoraNodeIdFromJson(line);
            if (fallbackNode.length() > 0 && isNodeAuthenticated(fallbackNode)) {
                Serial.println("[AUTH] LoRa raw JSON accepted (trusted node)");
                processLoraPacket(line);
                continue;
            }

            Serial.println("[AUTH] LoRa raw JSON dropped (node not trusted)");
            continue;
        }
        else {
            Serial.print("[LORA] ");
            Serial.println(line);
        }
    }
}

String getLoraNodeIdFromJson(const String& jsonStr) {
    String fromNode = extractJsonValue(jsonStr, "from");
    if (fromNode.length() == 0) {
        fromNode = extractJsonValue(jsonStr, "node");
    }
    if (fromNode.length() == 0) return "";
    return loraNodeToMeshNode(fromNode);
}

void processLoraPacket(const String& jsonStr) {
    Serial.print("[LORA RX] ");
    Serial.println(jsonStr);

    // PRIORITIZE "from" field (the neighbour), fallback to "node"
    String fromNode = extractJsonValue(jsonStr, "from");  // ← SWAP ORDER
    if (fromNode.length() == 0) {
        fromNode = extractJsonValue(jsonStr, "node");
    }
    
    String latencyStr = extractJsonValue(jsonStr, "latency_ms");
    String rssiStr    = extractJsonValue(jsonStr, "rssi");
    String energyStr  = extractJsonValue(jsonStr, "energy_mj");
    String reliabilityStr = extractJsonValue(jsonStr, "reliability");

    if (fromNode.length() == 0) {
        Serial.println("[LORA] No 'node' or 'from' field, ignoring");
        return;
    }

    String nodeId = loraNodeToMeshNode(fromNode);
    int nodeNum = extractNodeNum(nodeId);

    // Debug output
    Serial.print("[LORA DEBUG] fromNode=");
    Serial.print(fromNode);
    Serial.print(" -> nodeId=");
    Serial.print(nodeId);
    Serial.print(" nodeNum=");
    Serial.println(nodeNum);

    int rssi = rssiStr.toInt();
    float latencyMs = latencyStr.toFloat();
    float energyMj = energyStr.toFloat();
    float reliability = (reliabilityStr.length() > 0) ? reliabilityStr.toFloat() : 0.0f;

    Serial.print("[LORA] Upsert neighbour: ");
    Serial.print(nodeId);
    Serial.print(" RSSI=");
    Serial.print(rssi);
    Serial.print(" latency=");
    Serial.print(latencyMs);
    Serial.print(" energy=");
    Serial.print(energyMj);
    Serial.print(" reliability=");
    Serial.println(reliability);

    upsertLoraNeighbour(nodeId, nodeNum, rssi, latencyMs, energyMj, reliability);
}
String extractJsonValue(const String& json, const String& key) {
    String searchKey = "\"" + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return "";

    int valueStart = keyPos + searchKey.length();

    while (valueStart < json.length() && json.charAt(valueStart) == ' ') {
        valueStart++;
    }

    if (valueStart >= json.length()) return "";

    if (json.charAt(valueStart) == '"') {
        valueStart++;
        int valueEnd = json.indexOf('"', valueStart);
        if (valueEnd > valueStart) {
            return json.substring(valueStart, valueEnd);
        }
    } else {
        int valueEnd = valueStart;
        while (valueEnd < json.length() &&
               (isDigit(json.charAt(valueEnd)) ||
                json.charAt(valueEnd) == '.' ||
                json.charAt(valueEnd) == '-')) {
            valueEnd++;
        }
        return json.substring(valueStart, valueEnd);
    }

    return "";
}

String loraNodeToMeshNode(const String& loraNode) {
    if (loraNode.startsWith("UNO_")) {
        char letter = loraNode.charAt(4);
        int nodeNum = letter - 'A' + 1;
        return "NODE_" + String(nodeNum);
    }
    return loraNode;
}

void sendLoraMessage(const String& message) {
    if (!LORA_ENABLED) return;
    if (LORA_TX_PIN < 0) {
        Serial.println("[LORA TX] Skipped: TX pin not configured");
        return;
    }

    String pkt = buildAuthenticatedPacket(message);
    LORA_SERIAL.print("TX:");
    LORA_SERIAL.println(pkt);

    Serial.print("[LORA TX] ");
    Serial.println(pkt);
}
