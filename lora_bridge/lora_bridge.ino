// =============================================================
// lora_bridge.ino
// Maker UNO + LoRa Module — transparent bridge to M5Stick
//
// Measures and reports three link metrics as JSON:
//   latency_ms   — one-way estimate via RTT/2 (PING/PONG)
//   rssi / snr   — signal strength from radio hardware
//   energy_mj    — estimated from SX1276 datasheet + airtime
//
// CHANGE BEFORE FLASHING:
//   THIS_NODE_NAME — "UNO_A" through "UNO_E"
//   TEST_MODE      — true while testing, false when using M5Stick
// =============================================================

// ── Node identity ────────────────────────────────────────────
#define THIS_NODE_NAME  "UNO_B"

// ── Test mode ────────────────────────────────────────────────
// Sends a PING every TEST_INTERVAL_MS so you can verify both
// UNOs communicate before connecting the M5Stick.
// Set false before connecting to the M5Stick.
#define TEST_MODE        true
#define TEST_INTERVAL_MS 5000

// ── LoRa pin mapping (SX1276/SX1278 → Maker UNO) ─────────────
// NSS→D10  MOSI→D11  MISO→D12  SCK→D13  RST→D9  DIO0→D2
#define LORA_NSS_PIN    10
#define LORA_RST_PIN     9
#define LORA_DIO0_PIN    2

// ── LoRa radio config — must match on every node ─────────────
#define LORA_FREQUENCY  915E6  // 915E6=Americas  868E6=Europe
#define LORA_SF          7     // spreading factor: 7=fast, 12=long range
#define LORA_BANDWIDTH  125E3  // 125 kHz
#define LORA_CODING      5     // coding rate 4/5

// ── Serial baud to M5Stick ───────────────────────────────────
// Must match LORA_UART_BAUD in M5Stick config.h
#define BRIDGE_BAUD     9600

// ── Hardware ─────────────────────────────────────────────────
#define LED_PIN         13

// ── Energy model (SX1276 datasheet, cannot measure directly) ─
// TX: ~125 mW at +20 dBm    RX: ~11.5 mW in receive mode
#define TX_POWER_MW     125.0f
#define RX_POWER_MW      11.5f

// ── Max LoRa payload ─────────────────────────────────────────
#define MAX_PAYLOAD     200

// =============================================================
// Includes
// =============================================================

#include <SPI.h>
#include <LoRa.h>

// =============================================================
// State
// =============================================================

// Radio
bool loraReady = false;

// Counters
unsigned long txCount         = 0;
unsigned long rxCount         = 0;
float         totalTxEnergyMj = 0.0f;
float         totalRxEnergyMj = 0.0f;

// RTT latency tracking
unsigned long lastPingTxMs  = 0;
bool          waitingForPong = false;
float         lastLatencyMs  = 0.0f;
unsigned long rttCount       = 0;
float         avgLatencyMs   = 0.0f;

// Serial input
String        inputBuffer = "";

// Timers
unsigned long lastTestMs = 0;

// =============================================================
// LED helpers
// =============================================================

void ledBlink(int count, int onMs = 50, int offMs = 50) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_PIN, HIGH); delay(onMs);
        digitalWrite(LED_PIN, LOW);  delay(offMs);
    }
    digitalWrite(LED_PIN, HIGH);  // leave on (ready state)
}

// =============================================================
// Energy and airtime
// =============================================================

// Symbol duration: Ts = 2^SF / BW  (milliseconds)
float symbolDurationMs() {
    return (float)(1 << LORA_SF) / (float)LORA_BANDWIDTH * 1000.0f;
}

// On-air time in milliseconds (Semtech SX1276 datasheet formula)
float airtimeMs(int payloadBytes) {
    float ts       = symbolDurationMs();
    float preamble = (8.0f + 4.25f) * ts;
    int   cr       = LORA_CODING - 4;
    int   nSym     = max(0, (int)ceil(
                       (8.0f * payloadBytes - 4.0f * LORA_SF + 44.0f)
                       / (4.0f * LORA_SF)));
    float payload  = nSym * (cr + 4) * ts;
    return preamble + payload;
}

float txEnergyMj(int bytes) { return TX_POWER_MW * airtimeMs(bytes) / 1000.0f; }
float rxEnergyMj(int bytes) { return RX_POWER_MW * airtimeMs(bytes) / 1000.0f; }

// =============================================================
// Signal quality
// =============================================================

const char* signalQuality(int rssi) {
    if (rssi >= -60)  return "EXCELLENT";
    if (rssi >= -80)  return "GOOD";
    if (rssi >= -100) return "FAIR";
    if (rssi >= -120) return "WEAK";
    return "POOR";
}

// =============================================================
// JSON builder
// Single function that builds every JSON output in the sketch.
// Pass empty strings / -1 for fields that are not applicable.
// =============================================================

// =============================================================
// JSON builder — flat format matching target schema:
// {
//   "node":       this node name,
//   "latency_ms": one-way RTT/2 estimate, null if unavailable,
//   "rssi":       signal strength dBm,
//   "snr":        signal-to-noise ratio dB,
//   "energy_mj":  cumulative session TX+RX energy in mJ,
//   "tx_count":   total packets transmitted,
//   "rx_count":   total packets received
// }
// energy_mj is the SESSION TOTAL, not per-packet.
// Per-packet energy is always constant (same SF/BW/size)
// so the running total is the meaningful number.
// Pass latencyMs=-1 when unavailable (outputs null).
// Pass rssi=0 when unavailable (rssi+snr fields omitted).
// =============================================================

String buildJson(const char* from,  
                 float latencyMs,   
                 int   rssi,        
                 float snr) {

    String j = "{";
    j += "\"node\":\"" + String(THIS_NODE_NAME) + "\"";
    j += ",\"from\":\"" + String(from) + "\"";

    if (latencyMs >= 0)
        j += ",\"latency_ms\":" + String(latencyMs, 1);
    else
        j += ",\"latency_ms\":null";

    if (rssi != 0) {
        j += ",\"rssi\":" + String(rssi);
        j += ",\"snr\":" + String(snr, 1);
    }

    // Per-packet energy
    float totalPackets = (float)(txCount + rxCount);
    float energyPerPacket = (totalPackets > 0) 
        ? (totalTxEnergyMj + totalRxEnergyMj) / totalPackets 
        : 0.0f;
    
    j += ",\"energy_mj\":" + String(energyPerPacket, 3);
    j += ",\"tx_count\":" + String(txCount);
    j += ",\"rx_count\":" + String(rxCount);
    
    // ADD: Calculate packet loss
    // Expected: if we sent N pings, we should receive N pongs
    // Reliability = successful_pongs / total_pings_sent
    float reliability = (txCount > 0) ? (float)rttCount / (float)txCount : 0.0f;
    if (reliability > 1.0f) reliability = 1.0f;  // cap at 100%
    j += ",\"reliability\":" + String(reliability, 2);  // ← NEW
    
    j += "}";
    return j;
}
// =============================================================
// LoRa transmit
// =============================================================

void transmit(const String& payload) {
    if (!loraReady) return;

    String p = payload.length() > MAX_PAYLOAD
               ? payload.substring(0, MAX_PAYLOAD)
               : payload;

    LoRa.beginPacket();
    LoRa.print(p);
    LoRa.endPacket(true);  // non-blocking

    totalTxEnergyMj += txEnergyMj(p.length());
    txCount++;
    ledBlink(1);
}

// Send a timed PING for RTT measurement
// Format: PING|<millis>|<node>
//
// Each node waits a different base offset before transmitting so
// all nodes don't fire at the same moment (LoRa collision prevention).
// Offset = (node letter index) * 150ms + small random jitter.
// UNO_A=0ms  UNO_B=150ms  UNO_C=300ms  UNO_D=450ms  UNO_E=600ms
void sendPing() {
    if (!loraReady) return;

    int baseOffset = (THIS_NODE_NAME[4] - 'A') * 150;
    int jitter     = random(0, 100);
    delay(baseOffset + jitter);

    unsigned long now = millis();
    String packet = "PING|" + String(now) + "|" + THIS_NODE_NAME;
    lastPingTxMs   = now;
    waitingForPong = true;
    transmit(packet);
}

// Echo a received PING back as PONG with replier appended
// Format: PONG|<original_timestamp>|<original_sender>|<this_node>
//
// Staggered reply delay prevents multiple nodes from sending
// their PONGs simultaneously when they all hear the same PING.
// UNO_A=0ms  UNO_B=80ms  UNO_C=160ms  UNO_D=240ms  UNO_E=320ms
void sendPong(const String& pingPayload) {
    int replyDelay = (THIS_NODE_NAME[4] - 'A') * 80 + random(0, 40);
    delay(replyDelay);
    String pong = "PONG|" + pingPayload.substring(5) + "|" + THIS_NODE_NAME;
    transmit(pong);
}

// =============================================================
// Receive handlers — one function per packet type
// =============================================================

// PING received: echo back as PONG, log receipt, forward to M5Stick
void handlePing(const String& packet) {
    // Parse sender name: PING|<timestamp>|<sender>
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    String sender = (p1 > 0 && p2 > p1) ? packet.substring(p2 + 1) : "unknown";

    // Print receipt to Serial Monitor so this node shows incoming PINGs
    String receipt = "{";
    receipt += "\"node\":\"" + String(THIS_NODE_NAME) + "\"";
    receipt += ",\"received_ping_from\":\"" + sender + "\"";
    receipt += ",\"rx_count\":" + String(rxCount);
    receipt += "}";
    Serial.println(receipt);

    // Forward to M5Stick so it can update its neighbour table
    Serial.println("RX:" + receipt);

    sendPong(packet);
}

// PONG received: compute RTT, print metrics, forward to M5Stick
// PONG format: PONG|<timestamp>|<original_sender>|<replier>
void handlePong(const String& packet,
                unsigned long rxTimeMs,
                int rssi, float snr, int packetSize) {

    String inner = packet.substring(5);
    int    p1    = inner.indexOf('|');
    int    p2    = inner.lastIndexOf('|');

    if (p1 <= 0 || p2 <= p1) return;  // malformed

    String tsStr    = inner.substring(0, p1);
    String replier  = inner.substring(p2 + 1);

    // Validate timestamp is all digits
    for (int i = 0; i < tsStr.length(); i++) {
        if (!isDigit(tsStr[i])) return;
    }

    unsigned long sentMs = (unsigned long)tsStr.toInt();
    float         oneWay = (float)(rxTimeMs - sentMs) / 2.0f;
    float         rxE    = rxEnergyMj(packetSize);

    totalRxEnergyMj += rxE;
    lastLatencyMs    = oneWay;
    waitingForPong   = false;
    rttCount++;
    avgLatencyMs    += (oneWay - avgLatencyMs) / (float)rttCount;

    String json = buildJson(replier.c_str(), oneWay, rssi, snr);
    Serial.println(json);          // human-readable metrics to Serial Monitor
    Serial.println("RX:" + json);  // forward to M5Stick over UART
}

// Regular packet (mesh traffic from M5Stick chain)
void handleData(const String& packet, int rssi, float snr, int packetSize) {
    float rxE = rxEnergyMj(packetSize);
    totalRxEnergyMj += rxE;

    // Extract src field from JSON payload if present
    String from = "unknown";
    int srcIdx = packet.indexOf("\"src\":\"");
    if (srcIdx >= 0) {
        int start = srcIdx + 7;
        int end   = packet.indexOf("\"", start);
        if (end > start) from = packet.substring(start, end);
    }

    // Print metrics to Serial Monitor
    // Use latest known latency instead of null
    String metrics = buildJson(from.c_str(), lastLatencyMs, rssi, snr);
    Serial.println(metrics);

    // Forward original packet + injected metrics to M5Stick
    if (packet.startsWith("{") && packet.endsWith("}")) {
        String fwd = packet.substring(0, packet.length() - 1);
        fwd += ",\"rssi\":" + String(rssi);
        fwd += ",\"snr\":" + String(snr, 1);
        fwd += ",\"energy_mj\":" + String(rxE, 3);
        fwd += ",\"latency_ms\":" + String(lastLatencyMs, 1);
        fwd += "}";
        Serial.println("RX:" + fwd);
    } else {
        // If not JSON, still forward latest latency as JSON wrapper
        String wrapped = "{";
        wrapped += "\"node\":\"" + String(THIS_NODE_NAME) + "\"";
        wrapped += ",\"from\":\"" + from + "\"";
        wrapped += ",\"payload\":\"" + packet + "\"";
        wrapped += ",\"rssi\":" + String(rssi);
        wrapped += ",\"snr\":" + String(snr, 1);
        wrapped += ",\"energy_mj\":" + String(rxE, 3);
        wrapped += ",\"latency_ms\":" + String(lastLatencyMs, 1);
        wrapped += "}";
        Serial.println("RX:" + wrapped);
    }

    ledBlink(2, 30, 30);
}

// =============================================================
// Main receive dispatcher
// =============================================================

void checkLoRaReceive() {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    unsigned long rxTimeMs = millis();

    String received = "";
    while (LoRa.available()) received += (char)LoRa.read();
    received.trim();

    int   rssi = LoRa.packetRssi();
    float snr  = LoRa.packetSnr();
    rxCount++;

    if      (received.startsWith("PING|")) handlePing(received);
    else if (received.startsWith("PONG|")) handlePong(received, rxTimeMs, rssi, snr, packetSize);
    else                                   handleData(received, rssi, snr, packetSize);
}

// =============================================================
// Serial command handler
// =============================================================

void processCommand(const String& line) {
    if (line.startsWith("TX:")) {
        String payload = line.substring(3);
        payload.trim();
        if (payload.length() == 0) {
            Serial.println("{\"error\":\"empty payload\"}");
            return;
        }
        transmit(payload);

    } else if (line == "STATUS" || line == "STATS") {
        // Reuses buildJson — last known latency, no live signal reading
        Serial.println(buildJson(THIS_NODE_NAME, lastLatencyMs, 0, 0.0f));

    } else {
        Serial.println("{\"error\":\"unknown command\",\"cmd\":\"" + line + "\"}");
    }
}

// =============================================================
// Test mode
// =============================================================

void runTestMode() {
    unsigned long now = millis();

    // Timeout: declare packet lost after 10 seconds with no PONG
    if (waitingForPong && (now - lastPingTxMs > 10000)) {
        Serial.println("{\"type\":\"WARN\",\"node\":\"" + String(THIS_NODE_NAME)
                       + "\",\"msg\":\"no_pong_timeout\"}");
        waitingForPong = false;
    }

    if (now - lastTestMs >= TEST_INTERVAL_MS) {
        lastTestMs = now;
        sendPing();
    }
}

// =============================================================
// Setup
// =============================================================

void setup() {
    Serial.begin(BRIDGE_BAUD);
    pinMode(LED_PIN, OUTPUT);

    randomSeed(analogRead(A0));

    ledBlink(3, 100, 100);  // startup blink

    LoRa.setPins(LORA_NSS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("{\"error\":\"lora_init_failed\"}");
        while (true) ledBlink(1, 50, 50);
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING);
    LoRa.enableCrc();

    loraReady = true;
    lastTestMs = millis();

    String boot = "{";
    boot += "\"type\":\"BOOT\"";
    boot += ",\"node\":\""    + String(THIS_NODE_NAME) + "\"";
    boot += ",\"freq_mhz\":"  + String(LORA_FREQUENCY / 1E6, 2);
    boot += ",\"sf\":"        + String(LORA_SF);
    boot += ",\"bw_khz\":"    + String(LORA_BANDWIDTH / 1E3, 0);
    boot += ",\"mode\":\""    + String(TEST_MODE ? "TEST" : "BRIDGE") + "\"";
    boot += "}";
    Serial.println(boot);
}

// =============================================================
// Loop
// =============================================================

void loop() {
    // Read serial commands one character at a time
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            inputBuffer.trim();
            if (inputBuffer.length() > 0) processCommand(inputBuffer);
            inputBuffer = "";
        } else {
            inputBuffer += c;
            if (inputBuffer.length() > 300) inputBuffer = "";  // overflow guard
        }
    }

    checkLoRaReceive();

    if (TEST_MODE) runTestMode();
}
