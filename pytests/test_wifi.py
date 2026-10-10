"""Wi-Fi: status, searching, and the rules for joining a network and for the access point."""
import ipaddress
import time

import pytest


def wait_for_station(device, status, seconds=45):
    """Polls until the joined-network status is as wanted; returns the last report."""
    end = time.time() + seconds
    report = None
    while time.time() < end:
        try:
            report = device.json("/api/wifi")
            if report["station"]["status"] == status:
                return report
        except Exception:
            pass  # the board drops off the network for a moment while it rejoins
        time.sleep(1)
    raise AssertionError(f"station never became {status}; last report: {report}")


def keys_of(value):
    if isinstance(value, dict):
        for key, inner in value.items():
            yield key
            yield from keys_of(inner)
    elif isinstance(value, list):
        for inner in value:
            yield from keys_of(inner)


# ---- Status -----------------------------------------------------------------------


def test_status_describes_the_joined_network_and_the_access_point(device):
    report = device.json("/api/wifi")
    station = report["station"]
    assert station["status"] in ("notConfigured", "connecting", "connected", "failed")
    assert station["addressing"] in ("dhcp", "static")
    assert set(station["static"]) == {"ip", "mask", "gateway", "dns1", "dns2"}

    ap = report["accessPoint"]
    assert ap["ssid"]
    assert ap["ip"] == "192.168.4.1"
    assert isinstance(ap["clients"], int)
    assert isinstance(ap["defaultPassword"], bool)

    assert report["hostname"] == "assimilation-eye"
    assert len(report["mac"].split(":")) == 6


def test_status_never_contains_a_password(device):
    report = device.json("/api/wifi")
    assert [key for key in keys_of(report) if "pass" in key.lower()] == ["defaultPassword"]
    assert "resistanceisfutile" not in str(report)


def test_status_shows_the_address_the_interface_is_reached_at(device):
    try:
        reached_at = ipaddress.ip_address(device.host)
    except ValueError:
        pytest.skip("the board was addressed by name")
    if device.host == "192.168.4.1":
        pytest.skip("connected through the board's own access point")
    station = device.json("/api/wifi")["station"]
    assert station["status"] == "connected"
    assert station["ip"] == str(reached_at)
    assert ipaddress.ip_address(station["gateway"])
    network = ipaddress.ip_network(f"{station['ip']}/{station['mask']}", strict=False)
    assert ipaddress.ip_address(station["gateway"]) in network
    assert -100 <= station["rssi"] <= 0


# ---- Searching ----------------------------------------------------------------------


def test_search_finds_nearby_networks(device):
    assert device.get("/api/wifi/scan?start=1").status_code == 200
    end = time.time() + 25
    result = None
    while time.time() < end:
        time.sleep(1)
        result = device.json("/api/wifi/scan")
        if not result["scanning"]:
            break
    assert result is not None and result["scanning"] is False, "the search never finished"

    networks = result["networks"]
    assert len(networks) >= 1, "a search in a home should find at least one network"
    names = [n["ssid"] for n in networks]
    assert len(names) == len(set(names)), "each network is listed once"
    strengths = [n["rssi"] for n in networks]
    assert strengths == sorted(strengths, reverse=True), "strongest first"
    for network in networks:
        assert 1 <= len(network["ssid"].encode()) <= 32
        assert -127 <= network["rssi"] <= 0
        assert isinstance(network["secure"], bool)
        assert 1 <= network["channel"] <= 14

    # Searching must not knock the board off its network for good.
    assert wait_for_station(device, device.json("/api/wifi")["station"]["status"])


# ---- Rules for joining --------------------------------------------------------------

STATIC = {"addressing": "static", "ip": "10.11.12.50", "mask": "255.255.255.0", "gateway": "10.11.12.1"}

BAD_JOINS = [
    ({}, "network name is required"),
    ({"ssid": ""}, "network name is required"),
    ({"ssid": 7}, "network name is required"),
    ({"ssid": "n" * 33}, "at most 32 characters"),
    ({"ssid": "TestNet", "password": "short"}, "8 to 63 characters"),
    ({"ssid": "TestNet", "password": "p" * 64}, "8 to 63 characters"),
    ({"ssid": "TestNet", "addressing": "banana"}, "dhcp or static"),
    ({"ssid": "TestNet", "addressing": "static"}, "needs an IP address, a subnet mask and a gateway"),
    ({"ssid": "TestNet", **STATIC, "gateway": ""}, "needs an IP address, a subnet mask and a gateway"),
    ({"ssid": "TestNet", **STATIC, "ip": "10.11.12"}, "IP address is not a valid address"),
    ({"ssid": "TestNet", **STATIC, "ip": "10.11.12.256"}, "IP address is not a valid address"),
    ({"ssid": "TestNet", **STATIC, "mask": "255.255.0.255"}, "subnet mask is not valid"),
    ({"ssid": "TestNet", **STATIC, "mask": "banana"}, "subnet mask is not a valid address"),
    ({"ssid": "TestNet", **STATIC, "gateway": "10.11.13.1"}, "gateway is not in the same subnet"),
    ({"ssid": "TestNet", **STATIC, "gateway": "10.11.12.50"}, "must be different"),
    ({"ssid": "TestNet", **STATIC, "ip": "10.11.12.0"}, "network or broadcast address"),
    ({"ssid": "TestNet", **STATIC, "ip": "10.11.12.255"}, "network or broadcast address"),
    ({"ssid": "TestNet", **STATIC, "dns1": "8.8.8"}, "first DNS server is not a valid address"),
    ({"ssid": "TestNet", **STATIC, "dns2": "x"}, "second DNS server is not a valid address"),
    (
        {"ssid": "TestNet", "addressing": "static", "ip": "192.168.4.50", "mask": "255.255.255.0", "gateway": "192.168.4.1"},
        "overlaps 192.168.4.x",
    ),
    (
        {"ssid": "TestNet", "addressing": "static", "ip": "192.168.1.50", "mask": "255.255.0.0", "gateway": "192.168.1.1"},
        "overlaps 192.168.4.x",
    ),
]


@pytest.mark.parametrize("payload,message", BAD_JOINS, ids=[f"{i}: {m}" for i, (_, m) in enumerate(BAD_JOINS)])
def test_join_request_that_breaks_a_rule_is_refused_and_changes_nothing(device, payload, message):
    before = device.json("/api/wifi")["station"]
    response = device.post("/api/wifi", payload)
    assert response.status_code == 400, response.text
    assert message in response.json()["error"]

    after = device.json("/api/wifi")["station"]
    for key in ("configured", "ssid", "addressing", "static", "status"):
        assert after[key] == before[key], f"{key} changed after a refused request"


# ---- Rules for the access point -----------------------------------------------------

BAD_ACCESS_POINTS = [
    ({}, "network name is required"),
    ({"ssid": "", "password": "long-enough-1"}, "network name is required"),
    ({"ssid": "n" * 33, "password": "long-enough-1"}, "at most 32 characters"),
    ({"ssid": "EyeTest"}, "access point password is required"),
    ({"ssid": "EyeTest", "password": ""}, "8 to 63 characters"),
    ({"ssid": "EyeTest", "password": "short"}, "8 to 63 characters"),
    ({"ssid": "EyeTest", "password": "p" * 64}, "8 to 63 characters"),
]


@pytest.mark.parametrize("payload,message", BAD_ACCESS_POINTS, ids=[f"{i}: {m}" for i, (_, m) in enumerate(BAD_ACCESS_POINTS)])
def test_access_point_details_that_break_a_rule_are_refused(device, payload, message):
    before = device.json("/api/wifi")["accessPoint"]
    response = device.post("/api/wifi/ap", payload)
    assert response.status_code == 400, response.text
    assert message in response.json()["error"]
    assert device.json("/api/wifi")["accessPoint"]["ssid"] == before["ssid"]


# ---- Changing real settings (only with --destructive) --------------------------------


@pytest.mark.destructive
def test_static_address_then_back_to_dhcp(device):
    """Rejoins the current network with its present address fixed, then returns to DHCP.

    Uses the address DHCP already gave the board, so nothing else on the
    network can be holding it.
    """
    start = device.json("/api/wifi")["station"]
    if start["status"] != "connected" or device.host == "192.168.4.1":
        pytest.skip("needs the board joined to this PC's network")
    assert start["addressing"] == "dhcp", "run this from DHCP so it can be put back"

    fixed = {"ip": start["ip"], "mask": start["mask"], "gateway": start["gateway"], "dns1": start["gateway"]}
    try:
        # No password in the request: the board keeps the one it has.
        response = device.post("/api/wifi", {"ssid": start["ssid"], "addressing": "static", **fixed})
        assert response.status_code == 200, response.text
        assert response.json()["station"]["status"] == "connecting"

        report = wait_for_station(device, "connected")["station"]
        assert report["addressing"] == "static"
        assert report["ip"] == fixed["ip"], "the interface is reachable at the static address"
        assert report["mask"] == fixed["mask"]
        assert report["gateway"] == fixed["gateway"]
        assert report["dns1"] == fixed["dns1"]
        assert report["static"]["ip"] == fixed["ip"]
        assert device.state()["pattern"], "the API answers at the static address"
    finally:
        back = device.post("/api/wifi", {"ssid": start["ssid"], "addressing": "dhcp"})
    assert back.status_code == 200, back.text

    report = wait_for_station(device, "connected")["station"]
    assert report["addressing"] == "dhcp"
    assert report["static"]["ip"] == fixed["ip"], "the static details are kept for next time"
    assert report["ip"] == start["ip"], "DHCP handed out a different address; rerun with the new one"


@pytest.mark.destructive
def test_access_point_can_be_renamed_and_put_back(device):
    if device.host == "192.168.4.1":
        pytest.skip("this would cut the connection the test is using")
    before = device.json("/api/wifi")["accessPoint"]
    try:
        response = device.post("/api/wifi/ap", {"ssid": "EyeUnderTest", "password": "temporary-pass-1"})
        assert response.status_code == 200, response.text
        time.sleep(3)
        after = device.json("/api/wifi")["accessPoint"]
        assert after["ssid"] == "EyeUnderTest"
        assert after["defaultPassword"] is False
    finally:
        device.post("/api/wifi/ap", {"ssid": before["ssid"], "password": "resistanceisfutile"})
    time.sleep(3)
    assert device.json("/api/wifi")["accessPoint"]["ssid"] == before["ssid"]
