import pytest

from lib_gateway_client import fetch_nodes, is_gateway_reachable


@pytest.mark.integration
def test_gateway_api_schema(pytestconfig):
    gateway_url = pytestconfig.getoption("gateway_url")
    if not is_gateway_reachable(gateway_url):
        pytest.skip(f"Gateway not reachable at {gateway_url}")

    payload = fetch_nodes(gateway_url)
    assert isinstance(payload, dict)
    assert "nodes" in payload
    assert isinstance(payload["nodes"], list)

    for node in payload["nodes"]:
        assert "node_id" in node
        assert "links" in node
        assert isinstance(node["links"], list)

        for link in node["links"]:
            assert "to" in link
            assert "protocol" in link
            assert "latency" in link
            assert "energy" in link
            assert "reliability" in link
