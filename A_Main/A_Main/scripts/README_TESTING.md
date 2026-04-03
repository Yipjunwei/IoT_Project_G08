# Test Suite (Unit + Integration + System + Failure)

This folder contains the full test workflow you requested.

## Prerequisites
- Python 3.9+
- `pytest` installed (`pip install pytest`)
- Gateway running and reachable (default: `http://172.20.10.12`)
- Nodes sending telemetry for meaningful performance output

## Test Types
1. **Unit testing**
   - File: `scripts/tests/test_unit_metrics.py`
   - Validates link metric rules and summary math.

2. **Integration testing**
   - File: `scripts/tests/test_integration_gateway_api.py`
   - Verifies `/api/nodes` schema and live gateway reachability.

3. **Failure/robustness testing**
   - Files:
     - `scripts/tests/test_failure_robustness.py` (pytest check)
     - `scripts/run_failure_tests.py` (JSON report)
   - Sends malformed UDP payloads and checks gateway remains responsive.

4. **System/performance testing**
   - File: `scripts/run_performance_tests.py`
   - Collects timed samples and exports CSV + JSON summaries.

## Quick Start
Run all stages:

```bash
./scripts/run_all_tests.sh http://172.20.10.12 120
```

Arguments:
- Arg1: gateway URL (default `http://172.20.10.12`)
- Arg2: per-scenario duration in seconds (default `120`)

## Manual Commands
### Unit
```bash
pytest -q scripts/tests/test_unit_metrics.py
```

### Integration
```bash
pytest -q scripts/tests/test_integration_gateway_api.py --gateway-url http://172.20.10.12
```

### Failure
```bash
pytest -q scripts/tests/test_failure_robustness.py --gateway-url http://172.20.10.12
python3 scripts/run_failure_tests.py --gateway-url http://172.20.10.12
```

### Performance
```bash
python3 scripts/run_performance_tests.py \
  --gateway-url http://172.20.10.12 \
  --duration 120 \
  --interval 2 \
  --scenarios baseline latency_objective energy_objective reliability_objective
```

## Outputs
Saved to `scripts/output/`:
- `failure_report_<timestamp>.json`
- `performance_report_<timestamp>.json`
- `performance_samples_<timestamp>.csv`

## Notes
- Performance scenarios are **operator-driven labels**. Apply the physical/network condition before pressing Enter.
- If gateway is offline, integration/failure tests are skipped or fail with clear messages.
