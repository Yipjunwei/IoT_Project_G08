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


def try_fetch_nodes(gateway_url: str, label: str) -> Dict[str, object]:
    try:
        return {
            "ok": True,
            "payload": fetch_nodes(gateway_url, timeout_s=3.0, retries=6, retry_backoff_s=0.4),
            "error": "",
        }
    except Exception as exc:
        return {"ok": False, "payload": {"nodes": []}, "error": f"{label}: {exc}"}


def run_failure_suite(gateway_url: str, udp_port: int) -> Dict[str, object]:
    host, _ = parse_host_port(gateway_url)

    before_fetch = try_fetch_nodes(gateway_url, "before_fetch")
    before = before_fetch["payload"]
    before_nodes = {n.get("node_id") for n in before.get("nodes", [])}
    attacker_before = {str(node) for node in before_nodes if str(node).startswith("NODE_X")}

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

    after_fetch = try_fetch_nodes(gateway_url, "after_fetch")
    after = after_fetch["payload"]
    after_nodes = {n.get("node_id") for n in after.get("nodes", [])}
    attacker_after = {str(node) for node in after_nodes if str(node).startswith("NODE_X")}
    attacker_new = sorted(attacker_after - attacker_before)
    attacker_present = len(attacker_new) > 0
    api_alive = bool(before_fetch["ok"]) and bool(after_fetch["ok"])
    fetch_errors: List[str] = []
    if not before_fetch["ok"]:
        fetch_errors.append(str(before_fetch["error"]))
    if not after_fetch["ok"]:
        fetch_errors.append(str(after_fetch["error"]))

    return {
        "before_node_count": len(before_nodes),
        "after_node_count": len(after_nodes),
        "api_alive": api_alive,
        "fetch_errors": fetch_errors,
        "attacker_present": attacker_present,
        "attacker_nodes_before": sorted(attacker_before),
        "attacker_nodes_after": sorted(attacker_after),
        "attacker_nodes_new": attacker_new,
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
