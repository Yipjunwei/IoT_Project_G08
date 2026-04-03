from lib_metrics import is_positive_number, is_valid_link, summarize_links


def test_is_positive_number():
    assert is_positive_number(1)
    assert is_positive_number(0.1)
    assert not is_positive_number(0)
    assert not is_positive_number(-1)
    assert not is_positive_number("abc")


def test_is_valid_link_accepts_good_link():
    link = {
        "to": "NODE_2",
        "protocol": "WIFI",
        "rssi": -60,
        "latency": 12.5,
        "energy": 1.2,
        "reliability": 0.93,
    }
    assert is_valid_link(link)


def test_is_valid_link_rejects_zero_fields():
    base = {
        "to": "NODE_2",
        "protocol": "WIFI",
        "rssi": -60,
        "latency": 12.5,
        "energy": 1.2,
        "reliability": 0.93,
    }
    for field, value in [("rssi", 0), ("latency", 0), ("energy", 0), ("reliability", 0)]:
        link = dict(base)
        link[field] = value
        assert not is_valid_link(link)


def test_summarize_links_ignores_invalid_links():
    links = [
        {
            "to": "NODE_2",
            "protocol": "WIFI",
            "rssi": -50,
            "latency": 10,
            "energy": 1.0,
            "reliability": 0.9,
        },
        {
            "to": "NODE_3",
            "protocol": "BLE",
            "rssi": -70,
            "latency": 20,
            "energy": 2.0,
            "reliability": 0.8,
        },
        {
            "to": "NODE_4",
            "protocol": "LORA",
            "rssi": 0,
            "latency": 15,
            "energy": 1.5,
            "reliability": 0.7,
        },
    ]
    s = summarize_links(links)
    assert s["valid_link_count"] == 2
    assert s["avg_latency_ms"] == 15
    assert s["avg_energy"] == 1.5
