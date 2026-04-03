import json
import socket
import time
import urllib.error
import urllib.request
from typing import Any, Dict, List, Tuple


def _normalize_base_url(base_url: str) -> str:
    base_url = base_url.strip().rstrip("/")
    if not base_url.startswith("http://") and not base_url.startswith("https://"):
        base_url = "http://" + base_url
    return base_url


def fetch_nodes(base_url: str, timeout_s: float = 3.0) -> Dict[str, Any]:
    """Fetch JSON payload from /api/nodes and return parsed object."""
    base_url = _normalize_base_url(base_url)
    url = f"{base_url}/api/nodes"
    req = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        raw = resp.read().decode("utf-8")
        payload = json.loads(raw)
    if not isinstance(payload, dict):
        raise ValueError("Gateway response is not a JSON object")
    if "nodes" not in payload or not isinstance(payload["nodes"], list):
        raise ValueError("Gateway response missing 'nodes' list")
    return payload


def is_gateway_reachable(base_url: str, timeout_s: float = 2.0) -> bool:
    try:
        fetch_nodes(base_url, timeout_s=timeout_s)
        return True
    except Exception:
        return False


def flatten_links(nodes: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    links: List[Dict[str, Any]] = []
    for node in nodes:
        src = str(node.get("node_id", "")).strip()
        for link in node.get("links", []) or []:
            item = dict(link)
            item["from"] = src
            links.append(item)
    return links


def sample_gateway(base_url: str) -> Dict[str, Any]:
    payload = fetch_nodes(base_url)
    nodes = payload.get("nodes", [])
    links = flatten_links(nodes)
    return {
        "timestamp": time.time(),
        "node_count": len(nodes),
        "link_count": len(links),
        "nodes": nodes,
        "links": links,
    }


def send_udp_payload(host: str, port: int, payload: bytes) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(payload, (host, port))
    finally:
        sock.close()


def parse_host_port(base_url: str, default_http_port: int = 80) -> Tuple[str, int]:
    base_url = _normalize_base_url(base_url)
    no_scheme = base_url.split("://", 1)[1]
    if "/" in no_scheme:
        no_scheme = no_scheme.split("/", 1)[0]
    if ":" in no_scheme:
        host, port_s = no_scheme.rsplit(":", 1)
        return host, int(port_s)
    return no_scheme, default_http_port
