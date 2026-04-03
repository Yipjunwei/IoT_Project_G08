// =====================================================
// HELPER UTILITIES
// =====================================================

bool isValidMeshNodeName(const String& name) {
    return name.startsWith("NODE_");
}

int extractNodeNum(const String& nodeId) {
    int underscore = nodeId.lastIndexOf('_');
    if (underscore < 0) return -1;
    String numPart = nodeId.substring(underscore + 1);
    if (numPart.length() == 0) return -1;
    return numPart.toInt();
}

bool isMyScanSlot(unsigned long nowMs) {
    unsigned long cyclePos = nowMs % BLE_SCAN_CYCLE_MS;
    unsigned long slotStart = (NODE_NUM - 1) * BLE_SCAN_WINDOW_MS;
    unsigned long slotEnd = slotStart + BLE_SCAN_WINDOW_MS;
    return cyclePos >= slotStart && cyclePos < slotEnd;
}

unsigned long fnv1aHash32(const String& value);
String toHex8(unsigned long value);

String computeBleAuthTag(const String& nodeId) {
    String material = nodeId + "|" + String(PACKET_AUTH_KEY) + "|BLE";
    String hex = toHex8(fnv1aHash32(material));
    return hex.substring(0, 4);
}

String buildBleAdvertisedName() {
    String nodeId = String(NODE_ID);
    return nodeId + "_" + computeBleAuthTag(nodeId);
}

bool parseAndVerifyBleAdvertisedName(const String& advertisedName, String& nodeIdOut, int& nodeNumOut) {
    if (!advertisedName.startsWith("NODE_")) return false;

    int firstUnderscore = advertisedName.indexOf('_');
    int secondUnderscore = advertisedName.indexOf('_', firstUnderscore + 1);
    if (secondUnderscore < 0) return false;

    String numStr = advertisedName.substring(firstUnderscore + 1, secondUnderscore);
    if (numStr.length() == 0) return false;

    int nodeNum = numStr.toInt();
    if (nodeNum <= 0) return false;

    String nodeId = "NODE_" + numStr;
    String tag = advertisedName.substring(secondUnderscore + 1);
    if (tag.length() == 0) return false;

    String expectedTag = computeBleAuthTag(nodeId);
    if (!expectedTag.equalsIgnoreCase(tag)) return false;

    nodeIdOut = nodeId;
    nodeNumOut = nodeNum;
    return true;
}

struct RxSeqEntry {
    String nodeId;
    String channel;
    unsigned long lastSeq;
    bool used;
};

static RxSeqEntry rxSeqTable[MAX_NEIGHBOURS + 4];

struct AuthNodeEntry {
    String nodeId;
    bool used;
};

static AuthNodeEntry authNodeTable[MAX_NEIGHBOURS + 4];

void markNodeAuthenticated(const String& nodeId) {
    if (nodeId.length() == 0 || nodeId == String(NODE_ID)) return;

    int firstFree = -1;
    for (int i = 0; i < MAX_NEIGHBOURS + 4; i++) {
        if (!authNodeTable[i].used) {
            if (firstFree < 0) firstFree = i;
            continue;
        }
        if (authNodeTable[i].nodeId == nodeId) return;
    }

    int slot = (firstFree >= 0) ? firstFree : 0;
    authNodeTable[slot].used = true;
    authNodeTable[slot].nodeId = nodeId;
}

bool isNodeAuthenticated(const String& nodeId) {
    if (nodeId.length() == 0 || nodeId == String(NODE_ID)) return false;
    for (int i = 0; i < MAX_NEIGHBOURS + 4; i++) {
        if (authNodeTable[i].used && authNodeTable[i].nodeId == nodeId) {
            return true;
        }
    }
    return false;
}

unsigned long fnv1aHash32(const String& value) {
    unsigned long hash = 2166136261UL;
    for (int i = 0; i < value.length(); i++) {
        hash ^= (unsigned char)value.charAt(i);
        hash *= 16777619UL;
    }
    return hash;
}

String toHex8(unsigned long value) {
    char out[9];
    snprintf(out, sizeof(out), "%08lX", value);
    return String(out);
}

String computePacketHash(const String& nodeId, unsigned long seq, const String& data) {
    String payload = nodeId + "|" + String(seq) + "|" + data + "|" + String(PACKET_AUTH_KEY);
    return toHex8(fnv1aHash32(payload));
}

String buildAuthenticatedPacket(const String& data) {
    unsigned long seq = txSequence++;
    String nodeId = String(NODE_ID);
    String hash = computePacketHash(nodeId, seq, data);
    return nodeId + "|" + String(seq) + "|" + data + "|" + hash;
}

bool parseAuthenticatedPacket(const String& pkt, String& nodeIdOut, unsigned long& seqOut, String& dataOut, String& hashOut) {
    int p1 = pkt.indexOf('|');
    if (p1 <= 0) return false;

    int p2 = pkt.indexOf('|', p1 + 1);
    if (p2 <= p1 + 1) return false;

    int p3 = pkt.lastIndexOf('|');
    if (p3 <= p2 + 1) return false;

    String seqStr = pkt.substring(p1 + 1, p2);
    unsigned long seq = strtoul(seqStr.c_str(), nullptr, 10);
    if (seq == 0 && seqStr != "0") return false;

    nodeIdOut = pkt.substring(0, p1);
    seqOut = seq;
    dataOut = pkt.substring(p2 + 1, p3);
    hashOut = pkt.substring(p3 + 1);
    return nodeIdOut.length() > 0 && hashOut.length() > 0;
}

bool verifyAuthenticatedPacket(const String& nodeId, unsigned long seq, const String& data, const String& hashIn) {
    String expected = computePacketHash(nodeId, seq, data);
    return expected.equalsIgnoreCase(hashIn);
}

bool isDuplicatePacket(const String& nodeId, const String& channel, unsigned long seq) {
    if (nodeId == String(NODE_ID)) return true;

    int firstFree = -1;
    for (int i = 0; i < MAX_NEIGHBOURS + 4; i++) {
        if (!rxSeqTable[i].used) {
            if (firstFree < 0) firstFree = i;
            continue;
        }

        if (rxSeqTable[i].nodeId == nodeId && rxSeqTable[i].channel == channel) {
            if (seq <= rxSeqTable[i].lastSeq) return true;
            rxSeqTable[i].lastSeq = seq;
            return false;
        }
    }

    int slot = (firstFree >= 0) ? firstFree : 0;
    rxSeqTable[slot].used = true;
    rxSeqTable[slot].nodeId = nodeId;
    rxSeqTable[slot].channel = channel;
    rxSeqTable[slot].lastSeq = seq;
    return false;
}

String buildLinkStatePacket() {
    String pkt = "LS|";
    pkt += String(NODE_ID);

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (!neighbours[i].active) continue;

        if (neighbours[i].ble.available) {
            pkt += "|";
            pkt += neighbours[i].nodeId;
            pkt += ",BLE,";
            pkt += String(neighbours[i].ble.rssi);
            pkt += ",";
            pkt += String(neighbours[i].ble.latencyMs, 1);
            pkt += ",";
            pkt += String(neighbours[i].ble.energyCost, 2);
            pkt += ",";
            pkt += String(neighbours[i].ble.reliability, 2);
        }

        if (neighbours[i].wifi.available) {
            pkt += "|";
            pkt += neighbours[i].nodeId;
            pkt += ",WIFI,";
            pkt += String(neighbours[i].wifi.rssi);
            pkt += ",";
            pkt += String(neighbours[i].wifi.latencyMs, 1);
            pkt += ",";
            pkt += String(neighbours[i].wifi.energyCost, 2);
            pkt += ",";
            pkt += String(neighbours[i].wifi.reliability, 2);
        }

        if (neighbours[i].lora.available) {
            pkt += "|";
            pkt += neighbours[i].nodeId;
            pkt += ",LORA,";
            pkt += String(neighbours[i].lora.rssi);
            pkt += ",";
            pkt += String(neighbours[i].lora.latencyMs, 1);
            pkt += ",";
            pkt += String(neighbours[i].lora.energyCost, 2);
            pkt += ",";
            pkt += String(neighbours[i].lora.reliability, 2);
        }
    }

    return pkt;
}
