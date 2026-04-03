import pytest

from lib_gateway_client import fetch_nodes, is_gateway_reachable, parse_host_port, send_udp_payload


@pytest.mark.failure
@pytest.mark.integration
def test_gateway_survives_malformed_udp(pytestconfig):
    gateway_url = pytestconfig.getoption("gateway_url")
    if not is_gateway_reachable(gateway_url):
        pytest.skip(f"Gateway not reachable at {gateway_url}")

    host, _ = parse_host_port(gateway_url)

    malformed = [
        b"",
        b"garbage",
        b"LS|",
        b"NODE_X|1|LS|NODE_X|NODE_1,WIFI,0,0,0,0|BADHASH",
    ]

    for pkt in malformed:
        send_udp_payload(host, 5005, pkt)

    # if gateway still returns API payload, robustness condition passes
    payload = fetch_nodes(gateway_url)
    assert "nodes" in payload
    assert isinstance(payload["nodes"], list)
