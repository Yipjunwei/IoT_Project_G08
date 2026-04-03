// =====================================================
// DISPLAY FUNCTIONS
// =====================================================
void handleDisplayButton() {
    // wasPressed() is more reliable and handles debouncing internally
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        displayScreen = (displayScreen + 1) % 4;
        
        Serial.print("[DISPLAY] screen = ");
        Serial.println(displayScreen);
        
        // Force immediate redraw when button pressed
        updateDisplayNow();
    }
}

void drawHeader(const String& title) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.println(title);
    M5.Lcd.drawLine(0, 24, 240, 24, WHITE);
}

String getStrWinner(float &valueOut) {
    int idxBle = findBestBleNeighbourForLatency();
    int idxWifi = findBestWifiNeighbourForLatency();
    int idxLora = findBestLoraNeighbourForLatency();

    float bleVal = -1.0f;
    float wifiVal = -1.0f;
    float loraVal = -1.0f;

    if (idxBle >= 0) bleVal = neighbours[idxBle].ble.reliability;
    if (idxWifi >= 0) wifiVal = neighbours[idxWifi].wifi.reliability;
    if (idxLora >= 0) loraVal = neighbours[idxLora].lora.reliability;

    String winner = "NONE";
    float best = -1.0f;

    if (bleVal > best) {
        best = bleVal;
        winner = "BLE";
    }
    if (wifiVal > best) {
        best = wifiVal;
        winner = "WIFI";
    }
    if (loraVal > best) {
        best = loraVal;
        winner = "LORA";
    }

    valueOut = (best < 0.0f) ? 0.0f : best;
    return winner;
}

String getSpdWinner(float &valueOut) {
    int idxBle = findBestBleNeighbourForLatency();
    int idxWifi = findBestWifiNeighbourForLatency();
    int idxLora = findBestLoraNeighbourForLatency();

    float bleVal = 999999.0f;
    float wifiVal = 999999.0f;
    float loraVal = 999999.0f;

    if (idxBle >= 0) bleVal = neighbours[idxBle].ble.latencyMs;
    if (idxWifi >= 0) wifiVal = neighbours[idxWifi].wifi.latencyMs;
    if (idxLora >= 0) loraVal = neighbours[idxLora].lora.latencyMs;

    String winner = "NONE";
    float best = 999999.0f;

    if (bleVal < best) {
        best = bleVal;
        winner = "BLE";
    }
    if (wifiVal < best) {
        best = wifiVal;
        winner = "WIFI";
    }
    if (loraVal < best) {
        best = loraVal;
        winner = "LORA";
    }

    valueOut = (best >= 999999.0f) ? 0.0f : best;
    return winner;
}

String getEjWinner(float &valueOut) {
    float bleVal = (blePacketCount > 0) ? bleEnergyPerPacket : 999999.0f;
    float wifiVal = (wifiPacketCount > 0) ? wifiEnergyPerPacket : 999999.0f;
    float loraVal = (loraPacketCount > 0) ? loraEnergyPerPacket : 999999.0f;

    String winner = "NONE";
    float best = 999999.0f;

    if (bleVal < best) {
        best = bleVal;
        winner = "BLE";
    }
    if (wifiVal < best) {
        best = wifiVal;
        winner = "WIFI";
    }
    if (loraVal < best) {
        best = loraVal;
        winner = "LORA";
    }

    valueOut = (best >= 999999.0f) ? 0.0f : best;
    return winner;
}

void showMainScreen() {
    drawHeader("MAIN");

    float strVal = 0.0f;
    float spdVal = 0.0f;
    float ejVal  = 0.0f;

    String strWinner = getStrWinner(strVal);
    String spdWinner = getSpdWinner(spdVal);
    String ejWinner  = getEjWinner(ejVal);

    M5.Lcd.setTextSize(2);

    // ADD NODE ID DISPLAY HERE
    M5.Lcd.setCursor(5, 35);
    M5.Lcd.print("ID: ");
    M5.Lcd.println(NODE_ID);

    M5.Lcd.setCursor(5, 58);
    M5.Lcd.print("STR ");
    M5.Lcd.print(strWinner);
    M5.Lcd.print(" ");
    M5.Lcd.println(strVal, 2);

    M5.Lcd.setCursor(5, 81);
    M5.Lcd.print("SPD ");
    M5.Lcd.print(spdWinner);
    M5.Lcd.print(" ");
    M5.Lcd.println(spdVal, 1);

    M5.Lcd.setCursor(5, 104);
    M5.Lcd.print("EJ  ");
    M5.Lcd.print(ejWinner);
    M5.Lcd.print(" ");
    M5.Lcd.println(ejVal, 2);

    M5.Lcd.setTextSize(1);
    // MOVED DOWN TO MAKE ROOM
    // M5.Lcd.setCursor(5, 108);
    // M5.Lcd.print("Num Neigh: ");
    // M5.Lcd.println(getActiveNeighbourCount());

    M5.Lcd.setCursor(5, 120);
    M5.Lcd.println("Press btn to cycle");
}

void showWifiScreen() {
    drawHeader("WIFI");

    int idx = findBestWifiNeighbourForLatency();

    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(5, 35);
    M5.Lcd.print("N: ");
    if (idx >= 0) M5.Lcd.println(neighbours[idx].nodeId);
    else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 58);
    M5.Lcd.print("R: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].wifi.rssi);
        M5.Lcd.println("dBm");
    } else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 81);
    M5.Lcd.print("L: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].wifi.latencyMs, 1);
        M5.Lcd.println("ms");
    } else {
        M5.Lcd.println("-");
    }

    M5.Lcd.setCursor(5, 104);
    M5.Lcd.print("E: ");
    M5.Lcd.print(wifiEnergyPerPacket, 2);
    M5.Lcd.println("mJ");
}

void showBleScreen() {
    drawHeader("BLE");

    int idx = findBestBleNeighbourForLatency();

    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(5, 35);
    M5.Lcd.print("N: ");
    if (idx >= 0) M5.Lcd.println(neighbours[idx].nodeId);
    else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 58);
    M5.Lcd.print("R: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].ble.rssi);
        M5.Lcd.println("dBm");
    } else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 81);
    M5.Lcd.print("L: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].ble.latencyMs, 1);
        M5.Lcd.println("ms");
    } else {
        M5.Lcd.println("-");
    }

    M5.Lcd.setCursor(5, 104);
    M5.Lcd.print("E: ");
    M5.Lcd.print(bleEnergyPerPacket, 2);
    M5.Lcd.println("mJ");
}

void showLoraScreen() {
    drawHeader("LORA");

    int idx = findBestLoraNeighbourForLatency();

    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(5, 35);
    M5.Lcd.print("N: ");
    if (idx >= 0) M5.Lcd.println(neighbours[idx].nodeId);
    else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 58);
    M5.Lcd.print("R: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].lora.rssi);
        M5.Lcd.println("dBm");
    } else M5.Lcd.println("-");

    M5.Lcd.setCursor(5, 81);
    M5.Lcd.print("L: ");
    if (idx >= 0) {
        M5.Lcd.print(neighbours[idx].lora.latencyMs, 1);
        M5.Lcd.println("ms");
    } else {
        M5.Lcd.println("-");
    }

    M5.Lcd.setCursor(5, 104);
    M5.Lcd.print("E: ");
    M5.Lcd.print(loraEnergyPerPacket, 2);
    M5.Lcd.println("mJ");
}

void updateDisplayNow() {
    if (displayScreen == 0) {
        showMainScreen();
    } else if (displayScreen == 1) {
        showWifiScreen();
    } else if (displayScreen == 2) {
        showBleScreen();
    } else {
        showLoraScreen();
    }
}
void updateDisplay() {
    M5.update();  // CRITICAL - call this every loop iteration
    
    handleDisplayButton();

    static unsigned long lastDrawMs = 0;
    if (millis() - lastDrawMs < 500) return;  // Increased to 500ms for less flicker
    lastDrawMs = millis();

    updateDisplayNow();
}
