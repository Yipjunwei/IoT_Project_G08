# Test Suite (Unit + Integration + System + Failure)

  This project includes a full testing workflow for:

  1. Unit testing
  2. Integration testing
  3. Failure/robustness testing
  4. System/performance testing

  ## Folder Structure

  - scripts/tests/test_unit_metrics.py
  - scripts/tests/test_integration_gateway_api.py
  - scripts/tests/test_failure_robustness.py
  - scripts/run_failure_tests.py
  - scripts/run_performance_tests.py
  - scripts/run_all_tests.sh
  - scripts/requirements-test.txt
  - Output directory: scripts/output/

  ## Prerequisites

  - Python 3.9+
  - Gateway running and reachable (example: http://172.20.10.12)
  - Nodes powered on and sending telemetry for integration/performance tests

  ## Install Dependencies (macOS/Homebrew-safe)

  Use a virtual environment to avoid externally-managed-environment errors:

  python3 -m venv .venv
  source .venv/bin/activate
  python -m pip install -U pip
  python -m pip install -r scripts/requirements-test.txt

  ## Run All Test Categories

  ./scripts/run_all_tests.sh http://172.20.10.12 120

  Arguments:

  - arg1: gateway URL (default: http://172.20.10.12)
  - arg2: duration in seconds per performance scenario (default: 120)

  ## Run Tests Individually

  ### 1) Unit Testing

  Validates metric logic and link validation rules.

  python -m pytest -q scripts/tests/test_unit_metrics.py

  ### 2) Integration Testing

  Validates gateway API schema and live reachability.

  python -m pytest -q scripts/tests/test_integration_gateway_api.py --gateway-u
  rl http://172.20.10.12

  ### 3) Failure/Robustness Testing

  Sends malformed UDP payloads and verifies gateway stays responsive.

  python -m pytest -q scripts/tests/test_failure_robustness.py --gateway-url
  http://172.20.10.12
  python scripts/run_failure_tests.py --gateway-url http://172.20.10.12

  ### 4) System/Performance Testing

  Collects timed samples and exports CSV + JSON reports.

  python scripts/run_performance_tests.py --gateway-url http://172.20.10.12 --d
  uration 120 --interval 2

  Default scenarios:

  - baseline
  - latency_objective
  - energy_objective
  - reliability_objective

  You can override scenarios:

  python scripts/run_performance_tests.py \
    --gateway-url http://172.20.10.12 \
    --duration 120 \
    --interval 2 \
    --scenarios baseline interference node_failure

  ## Output Files

  All generated reports are saved in:

  scripts/output/

  Files:

  - failure_report_<timestamp>.json
  - performance_report_<timestamp>.json
  - performance_samples_<timestamp>.csv

  ## Notes

  - Performance scenarios are operator-driven labels.
  - Before each scenario starts, apply the intended physical/network condition,
    then press Enter.
  - If gateway is offline, integration/failure tests will skip or fail with
    clear messages.

  ## Deactivate Environment

  deactivate
