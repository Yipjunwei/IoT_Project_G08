#!/usr/bin/env python3
import argparse
import csv
import json
import os
import sys
import time
from typing import Dict, List

from lib_gateway_client import is_gateway_reachable, sample_gateway
from lib_metrics import summarize_run, summarize_sample


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def _sleep_interruptible(seconds: float) -> bool:
    end = time.time() + max(0.0, seconds)
    while time.time() < end:
        remaining = end - time.time()
        try:
            time.sleep(min(0.2, remaining))
        except KeyboardInterrupt:
            return False
    return True


def run_scenario(base_url: str, scenario: str, duration_s: int, interval_s: float) -> Dict[str, object]:
    started = time.time()
    deadline = started + duration_s
    samples: List[Dict[str, object]] = []
    sample_summaries: List[Dict[str, float]] = []
    failed_samples = 0
    last_error = ""
    interrupted = False

    while time.time() < deadline:
        try:
            sample = sample_gateway(base_url)
            ssum = summarize_sample(sample)
            sample["summary"] = ssum
            sample["scenario"] = scenario
            samples.append(sample)
            sample_summaries.append(ssum)
        except Exception as exc:
            failed_samples += 1
            last_error = str(exc)
            print(f"[WARN] Sample failed ({scenario}): {exc}")
        remaining_s = max(0.0, deadline - time.time())
        print(
            f"[PROGRESS] {scenario}: samples={len(samples)} failed={failed_samples} "
            f"remaining={remaining_s:.1f}s"
        )
        if not _sleep_interruptible(interval_s):
            interrupted = True
            print(f"[WARN] Scenario interrupted by user: {scenario}")
            break

    run_summary = summarize_run(sample_summaries)
    return {
        "scenario": scenario,
        "started_at": started,
        "ended_at": time.time(),
        "duration_s": duration_s,
        "interval_s": interval_s,
        "interrupted": interrupted,
        "summary": run_summary,
        "samples": samples,
        "failed_samples": failed_samples,
        "last_error": last_error,
    }


def write_outputs(out_dir: str, report: Dict[str, object]) -> None:
    ensure_dir(out_dir)
    ts = int(time.time())
    json_path = os.path.join(out_dir, f"performance_report_{ts}.json")
    csv_path = os.path.join(out_dir, f"performance_samples_{ts}.csv")

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "scenario",
                "timestamp",
                "node_count",
                "link_count",
                "valid_link_count",
                "avg_latency_ms",
                "p95_latency_ms",
                "avg_energy",
                "avg_reliability",
            ]
        )
        for sc in report["scenarios"]:
            scenario = sc["scenario"]
            for sample in sc["samples"]:
                s = sample["summary"]
                writer.writerow(
                    [
                        scenario,
                        sample["timestamp"],
                        int(s.get("node_count", 0)),
                        int(s.get("link_count", 0)),
                        int(s.get("valid_link_count", 0)),
                        round(s.get("avg_latency_ms", 0.0), 4),
                        round(s.get("p95_latency_ms", 0.0), 4),
                        round(s.get("avg_energy", 0.0), 6),
                        round(s.get("avg_reliability", 0.0), 6),
                    ]
                )

    print(f"[OK] JSON report: {json_path}")
    print(f"[OK] CSV samples: {csv_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run system/performance tests against gateway API")
    parser.add_argument("--gateway-url", default="http://172.20.10.12", help="Gateway base URL")
    parser.add_argument("--duration", type=int, default=60, help="Duration per scenario (seconds)")
    parser.add_argument("--interval", type=float, default=1.5, help="Sampling interval (seconds)")
    parser.add_argument(
        "--scenarios",
        nargs="+",
        default=["latency_objective", "energy_objective", "reliability_objective"],
        help="Scenario labels to run (operator applies physical/network condition manually)",
    )
    parser.add_argument(
        "--wait-for-enter",
        action="store_true",
        help="Wait for Enter before each scenario (default: disabled)",
    )
    parser.add_argument("--out-dir", default="scripts/output", help="Output directory")
    args = parser.parse_args()

    if not is_gateway_reachable(args.gateway_url):
        print(f"[ERROR] Gateway unreachable: {args.gateway_url}")
        return 2

    print(f"[INFO] Gateway reachable: {args.gateway_url}")
    print("[INFO] Apply intended network condition before each scenario starts.")

    scenario_reports = []
    aborted = False
    for scenario in args.scenarios:
        print(f"\n[RUN] Scenario: {scenario}")
        if args.wait_for_enter and sys.stdin.isatty():
            print("Press Enter to start this scenario...")
            try:
                input().strip()
            except EOFError:
                print("[WARN] No interactive stdin detected; auto-starting.")
            except KeyboardInterrupt:
                print("\n[WARN] Run aborted by user before scenario start.")
                aborted = True
                break
        else:
            print("[INFO] Starting now.")
        report = run_scenario(args.gateway_url, scenario, args.duration, args.interval)
        scenario_reports.append(report)
        print(f"[DONE] {scenario}: {report['summary']}")
        if report.get("interrupted"):
            aborted = True
            break

    final_report = {
        "gateway_url": args.gateway_url,
        "generated_at": time.time(),
        "aborted": aborted,
        "scenarios": scenario_reports,
    }
    write_outputs(args.out_dir, final_report)
    if aborted:
        print("[WARN] Run ended early. Partial results were saved.")
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
