import os
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def pytest_addoption(parser):
    parser.addoption(
        "--gateway-url",
        action="store",
        default=os.getenv("GATEWAY_URL", "http://172.20.10.12"),
        help="Gateway base URL for integration/failure tests",
    )


def pytest_configure(config):
    config.addinivalue_line("markers", "integration: requires live gateway")
    config.addinivalue_line("markers", "failure: sends malformed UDP to gateway")
