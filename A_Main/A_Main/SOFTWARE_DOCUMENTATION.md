# Software Documentation for Report

## 1. Project Summary

This project is a real-time multi-protocol IoT mesh analytics system with three parts:

- ESP32 edge nodes (M5StickC Plus, Arduino/C++)
- Raspberry Pi Pico W gateway (MicroPython)
- Web dashboard (HTML/CSS/JavaScript)

The system collects live telemetry over Wi-Fi, BLE, and LoRa, builds a live network graph, and computes routes using Dijkstra based on the selected objective (latency, energy, or reliability).

## 2. Implemented Architecture

### 2.1 Edge Node Firmware

The firmware is split into modules:

- `A_Main.ino`: setup, loop, scheduling, configuration
- `B_DataStructures.ino`: shared globals and data structures
- `C_EnergyModel.ino`: energy estimation/tracking
- `D_LinkMetrics.ino`: link metric estimation
- `E_HelperUtils.ino`: packet auth/hash/parse/dedup helpers
- `F_NeighbourManagement.ino`: neighbor table management
- `G_BLE_Functions.ino`: BLE advertising and scanning
- `H_WiFi_Functions.ino`: Wi-Fi setup and UDP communication
- `I_Display.ino`: on-device LCD views
- `J_LoRa_Functions.ino`: LoRa serial bridge processing

### 2.2 Gateway

Gateway implementation (`pico_gateway_dashboard.py`) provides:

- UDP ingestion on port `5005`
- in-memory node/link state
- packet verification and parsing
- sequence deduplication
- stale node expiry
- HTTP server on port `80`:
  - `/` dashboard
  - `/api/nodes` telemetry API

### 2.3 Dashboard

Dashboard features:

- live topology graph
- protocol filters (`WIFI`, `BLE`, `LORA`)
- link tooltip with metrics
- route planner (source, destination, objective)
- forwarding/routing table by source node

## 3. Routing and Analytics

Implemented routing features:

- Dijkstra shortest path computation
- objective-based edge weighting:
  - `latency`: lower is better
  - `energy`: lower is better
  - `reliability`: converted to cost (`1 - reliability`)
- hop-by-hop and total route metrics
- forwarding table generation
- inferred reverse links when one direction is missing

Suggested wording:

- "The system applies Dijkstra on live telemetry-derived graph weights for objective-based route analysis."
- "Route selection changes with the selected optimization objective."

## 4. Security and Robustness

Implemented controls:

- shared-key wrapped packets: `node_id|seq|data|hash`
- hash verification at nodes and gateway
- sequence deduplication to block duplicates/replays
- unauthenticated Wi-Fi HELLO discovery packets dropped
- BLE neighbor acceptance requires valid auth tag
- invalid metric links filtered out before routing
- stale node expiry for topology cleanup
- legacy unauthenticated gateway parsing path exists but is disabled by default (`ALLOW_LEGACY_UNAUTH = False`)

Security scope:

- Integrity/authentication checks are implemented.
- Transport confidentiality is not implemented (no TLS).
- No user login/token auth is implemented for HTTP endpoints.

Suggested wording:

- "The prototype includes telemetry integrity and anti-replay controls suitable for controlled testbed use."
- "Additional transport and access-control hardening is required for production deployment."

## 5. Metrics in Current Implementation

Metrics currently used:

- latency
- energy metric
- reliability
- RSSI

Metrics not implemented as primary routing objectives:

- jitter
- throughput/bandwidth
- PDR as direct routing objective
- routing overhead

Suggested wording:

- "Current evaluation focuses on latency, reliability, and energy; jitter and throughput are planned for future expansion."

## 6. Testing and Validation

Available testing scripts:

- unit tests: `scripts/tests/test_unit_metrics.py`
- API integration tests: `scripts/tests/test_integration_gateway_api.py`
- robustness tests: `scripts/tests/test_failure_robustness.py`, `scripts/run_failure_tests.py`
- performance runner: `scripts/run_performance_tests.py`
- full runner: `scripts/run_all_tests.sh`

Robustness test coverage includes malformed payloads and invalid hash payloads (`BADHASH`) and checks that the API remains responsive without accepting attacker nodes.

Suggested wording:

- "Automated tests cover metric logic, API behavior, malformed payload handling, and scenario-based performance sampling."

## 7. What You Can Claim in Report

Safe claims:

- end-to-end multi-protocol telemetry pipeline (Wi-Fi, BLE, LoRa bridge)
- live topology visualization with protocol filtering
- Dijkstra-based objective routing analysis
- packet integrity validation and deduplication
- malformed payload rejection and stale node expiry
- scripted robustness and performance testing workflow

## 8. Claims to Qualify or Avoid

Claims to avoid (unless you add new implementation evidence):

- multiple routing algorithms evaluated end-to-end
- full jitter/throughput/PDR objective evaluation
- production-grade security
- gateway-only route computation

Better alternatives:

- "Dijkstra-based objective routing is implemented and validated in the dashboard workflow."
- "The prototype provides integrity-focused security controls for controlled LAN testing."

## 9. Suggested Mapping to Report Sections

- Introduction and problem statement: Sections 1 and 3
- Design and architecture: Section 2
- Security design: Section 4
- Implementation: Sections 2, 3, 5
- Testing and validation: Section 6
- Contributions and scope: Sections 7 and 8
- Limitations/future work: Sections 5 and 8

## 10. Executive Summary Paragraph

This project implements a real-time multi-protocol IoT mesh analytics prototype using ESP32 edge nodes and a Pico W gateway. The system collects Wi-Fi, BLE, and LoRa telemetry, builds a live topology view, and computes objective-based routes using Dijkstra for latency, energy, and reliability. It includes packet integrity checks, sequence deduplication, malformed payload rejection, and stale-node expiry to improve robustness of routing inputs. Testing scripts cover unit logic, API integration, robustness under malformed traffic, and scenario-based performance sampling. The current implementation is suitable for controlled testbed analysis, with transport encryption and expanded metric coverage identified as future work.
