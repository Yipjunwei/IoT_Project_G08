#!/usr/bin/env python3
import argparse
import json
import os
import time
from typing import Dict, List

from lib_gateway_client import fetch_nodes, is_gateway_reachable, parse_host_port, send_udp_payload


MALFORMED_PAYLOADS = [
    b"",
    b"HELLO|NODE_X|1.2.3.4|WIFI",  # unauthenticated or malformed neighbor message
    b"not_a_packet",
    b"LS|",  # incomplete link-state frame
    b"NODE_X|1|LS|NODE_X|NODE_1,WIFI,0,0,0,0|BADHASH",  # intentionally invalid
]


def run_failure_suite(gateway_url: str, udp_port: int) -> Dict[str, object]:
    host, _ = parse_host_port(gateway_url)

    before = fetch_nodes(gateway_url)
    before_nodes = {n.get("node_id") for n in before.get("nodes", [])}

    injections: List[Dict[str, object]] = []
    for payload in MALFORMED_PAYLOADS:
        try:
            send_udp_payload(host, udp_port, payload)
            ok = True
            err = ""
        except Exception as e:
            ok = False
            err = str(e)
        injections.append({"payload": payload.decode("utf-8", errors="replace"), "sent": ok, "error": err})
        time.sleep(0.2)

    time.sleep(1.0)

    after = fetch_nodes(gateway_url)
    after_nodes = {n.get("node_id") for n in after.get("nodes", [])}

    attacker_present = any(str(node).startswith("NODE_X") for node in after_nodes)
    api_alive = True

    return {
        "before_node_count": len(before_nodes),
        "after_node_count": len(after_nodes),
        "api_alive": api_alive,
        "attacker_present": attacker_present,
        "injections": injections,
        "pass": api_alive and (not attacker_present),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Run failure/robustness tests against gateway UDP parser")
    parser.add_argument("--gateway-url", default="http://172.20.10.12", help="Gateway base URL")
    parser.add_argument("--udp-port", type=int, default=5005, help="Gateway UDP port")
    parser.add_argument("--out-dir", default="scripts/output", help="Output directory")
    args = parser.parse_args()

    if not is_gateway_reachable(args.gateway_url):
        print(f"[ERROR] Gateway unreachable: {args.gateway_url}")
        return 2

    result = run_failure_suite(args.gateway_url, args.udp_port)

    os.makedirs(args.out_dir, exist_ok=True)
    ts = int(time.time())
    out_path = os.path.join(args.out_dir, f"failure_report_{ts}.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

    print(f"[OK] Failure report: {out_path}")
    print(f"[RESULT] pass={result['pass']} api_alive={result['api_alive']} attacker_present={result['attacker_present']}")
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
