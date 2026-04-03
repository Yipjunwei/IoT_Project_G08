from typing import Any, Dict, Iterable, List


VALID_PROTOCOLS = {"WIFI", "BLE", "LORA"}


def is_positive_number(value: Any) -> bool:
    try:
        return float(value) > 0
    except Exception:
        return False


def is_valid_link(link: Dict[str, Any]) -> bool:
    proto = str(link.get("protocol", "")).strip().upper()
    if proto not in VALID_PROTOCOLS:
        return False
    if str(link.get("to", "")).strip() == "":
        return False
    if not is_positive_number(link.get("latency", 0)):
        return False
    if not is_positive_number(link.get("energy", 0)):
        return False
    if not is_positive_number(link.get("reliability", 0)):
        return False
    if int(link.get("rssi", 0)) == 0:
        return False
    return True


def _percentile(sorted_values: List[float], p: float) -> float:
    if not sorted_values:
        return 0.0
    if p <= 0:
        return sorted_values[0]
    if p >= 100:
        return sorted_values[-1]
    k = (len(sorted_values) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(sorted_values) - 1)
    if f == c:
        return sorted_values[f]
    return sorted_values[f] + (sorted_values[c] - sorted_values[f]) * (k - f)


def summarize_links(links: Iterable[Dict[str, Any]]) -> Dict[str, float]:
    latencies: List[float] = []
    energies: List[float] = []
    reliabilities: List[float] = []

    for link in links:
        if not is_valid_link(link):
            continue
        latencies.append(float(link["latency"]))
        energies.append(float(link["energy"]))
        reliabilities.append(float(link["reliability"]))

    latencies.sort()
    energies.sort()
    reliabilities.sort()

    count = len(latencies)
    if count == 0:
        return {
            "valid_link_count": 0,
            "avg_latency_ms": 0.0,
            "p95_latency_ms": 0.0,
            "avg_energy": 0.0,
            "avg_reliability": 0.0,
        }

    return {
        "valid_link_count": float(count),
        "avg_latency_ms": sum(latencies) / count,
        "p95_latency_ms": _percentile(latencies, 95),
        "avg_energy": sum(energies) / count,
        "avg_reliability": sum(reliabilities) / count,
    }


def summarize_sample(sample: Dict[str, Any]) -> Dict[str, float]:
    summary = summarize_links(sample.get("links", []))
    summary["node_count"] = float(sample.get("node_count", 0))
    summary["link_count"] = float(sample.get("link_count", 0))
    return summary


def summarize_run(sample_summaries: List[Dict[str, float]]) -> Dict[str, float]:
    if not sample_summaries:
        return {
            "samples": 0.0,
            "avg_node_count": 0.0,
            "avg_link_count": 0.0,
            "avg_latency_ms": 0.0,
            "avg_energy": 0.0,
            "avg_reliability": 0.0,
            "p95_latency_ms": 0.0,
        }

    def mean(key: str) -> float:
        return sum(s.get(key, 0.0) for s in sample_summaries) / len(sample_summaries)

    lat_values = sorted(s.get("avg_latency_ms", 0.0) for s in sample_summaries)

    return {
        "samples": float(len(sample_summaries)),
        "avg_node_count": mean("node_count"),
        "avg_link_count": mean("link_count"),
        "avg_latency_ms": mean("avg_latency_ms"),
        "avg_energy": mean("avg_energy"),
        "avg_reliability": mean("avg_reliability"),
        "p95_latency_ms": _percentile(lat_values, 95),
    }
