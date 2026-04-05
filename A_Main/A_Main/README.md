# Multi-Protocol IoT Mesh Analytics

This repository contains a working prototype for real-time IoT mesh telemetry and route analysis across Wi-Fi, BLE, and LoRa.

The system has three parts:

- ESP32 edge node firmware (`.ino` modules)
- Pico W gateway backend (`pico_gateway_dashboard.py`)
- Browser dashboard served by the gateway

## Features

- Multi-protocol neighbour discovery and link-state telemetry
- Live topology dashboard with protocol filters
- Objective-based routing analysis using Dijkstra:
  - latency
  - energy
  - reliability
- Routing table and source-destination route view
- Telemetry integrity and robustness controls:
  - shared-key packet hash verification
  - sequence-based deduplication
  - malformed payload rejection
  - stale node expiry

## Repository Structure

### Node Firmware (Arduino, ESP32)

- `A_Main.ino`: main setup/loop and scheduling
- `B_DataStructures.ino`: shared globals and data structures
- `C_EnergyModel.ino`: energy estimation/tracking
- `D_LinkMetrics.ino`: link metric estimation
- `E_HelperUtils.ino`: auth/hash/parse/dedup helpers
- `F_NeighbourManagement.ino`: neighbour table management
- `G_BLE_Functions.ino`: BLE advertising/scanning
- `H_WiFi_Functions.ino`: Wi-Fi and UDP logic
- `I_Display.ino`: on-device LCD pages
- `J_LoRa_Functions.ino`: LoRa serial-bridge logic

### Gateway and Dashboard

- `pico_gateway_dashboard.py`: UDP ingest, HTTP API, and embedded dashboard

### Test and Evaluation Scripts

- `scripts/run_all_tests.sh`
- `scripts/run_performance_tests.py`
- `scripts/run_failure_tests.py`
- `scripts/tests/*`

## How It Works

1. Nodes discover neighbours over BLE/Wi-Fi and ingest LoRa data through serial bridge.
2. Nodes build link-state payloads and send authenticated packets to the gateway (UDP port `5005`).
3. Gateway verifies packets, deduplicates by sequence, filters invalid links, and maintains active topology state.
4. Dashboard fetches `/api/nodes` and renders live topology and route analysis.

## Configuration

Edit node settings in `A_Main.ino`:

```cpp
#define NODE_ID             "NODE_1"
#define NODE_NUM            1
#define BLE_DEVICE_NAME     "NODE_1"

#define PACKET_AUTH_KEY     "mesh_shared_key_v1"

#define WIFI_SSID           "YourSSID"
#define WIFI_PASSWORD       "YourPassword"

#define GATEWAY_IP          "x.x.x.x"
#define GATEWAY_PORT        5005
```

Edit gateway settings in `pico_gateway_dashboard.py`:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `PACKET_AUTH_KEY` (must match nodes)
- `UDP_PORT` (default `5005`)
- `HTTP_PORT` (default `80`)

## Build and Run

### Node Firmware

1. Keep all `.ino` files in one Arduino sketch folder.
2. Open `A_Main.ino` in Arduino IDE.
3. Install required board/libraries.
4. Flash each node with unique `NODE_ID` / `NODE_NUM`.

### Gateway

1. Flash/upload `pico_gateway_dashboard.py` to Pico W.
2. Reboot the gateway.
3. Open `http://<gateway-ip>/` in a browser.

## Testing

Run full test flow:

```bash
./scripts/run_all_tests.sh http://<gateway-ip> 120
```

Run specific suites:

```bash
pytest -q scripts/tests/test_unit_metrics.py
pytest -q scripts/tests/test_integration_gateway_api.py --gateway-url http://<gateway-ip>
pytest -q scripts/tests/test_failure_robustness.py --gateway-url http://<gateway-ip>
python3 scripts/run_performance_tests.py --gateway-url http://<gateway-ip> --duration 60
```

## Security Scope

Implemented:

- packet integrity/authentication with shared-key hash
- sequence deduplication
- malformed payload rejection

Not implemented:

- TLS/HTTPS transport encryption
- user login/token auth for dashboard/API

This makes the current build suitable for controlled LAN/testbed use. Production deployment needs stronger security hardening.
