#!/usr/bin/env bash
set -euo pipefail

GATEWAY_URL="${1:-http://172.20.10.12}"
DURATION="${2:-120}"

echo "[1/4] Unit tests"
pytest -q scripts/tests/test_unit_metrics.py

echo "[2/4] Integration tests"
pytest -q scripts/tests/test_integration_gateway_api.py --gateway-url "$GATEWAY_URL"

echo "[3/4] Failure/robustness tests"
pytest -q scripts/tests/test_failure_robustness.py --gateway-url "$GATEWAY_URL"
python3 scripts/run_failure_tests.py --gateway-url "$GATEWAY_URL"

echo "[4/4] System/performance tests"
python3 scripts/run_performance_tests.py --gateway-url "$GATEWAY_URL" --duration "$DURATION"

echo "[DONE] All test stages completed"
