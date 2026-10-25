import sihd

wifi = sihd.wifi

# failures raise sihd.Error, a host without wifi stack answers not_supported
try:
    wifi.link("sihd-no-such-interface")
    assert(False)
except sihd.Error as missing:
    if missing.code != sihd.ErrorCode.not_supported:
        assert(missing.code == sihd.ErrorCode.not_found)
        assert(str(missing) == "no wifi interface named sihd-no-such-interface")

try:
    wifi.access_points("sihd-no-such-interface")
    assert(False)
except sihd.Error as unheard:
    if unheard.code != sihd.ErrorCode.not_supported:
        assert(unheard.code == sihd.ErrorCode.not_found)
        assert(str(unheard) == "no wifi interface named sihd-no-such-interface")

# the monitor fails like backend() whatever daemon this host runs
try:
    wifi.backend("sihd-no-such-interface")
    assert(False)
except sihd.Error as error:
    unmanaged = error
monitor = wifi.Monitor(sihd.util.time.ms(1000))
try:
    monitor.open("sihd-no-such-interface")
    assert(False)
except sihd.Error as unmonitored:
    assert(unmonitored.code == unmanaged.code)
    assert(str(unmonitored) == str(unmanaged))
assert(monitor.run() == False)

# both connect forms check their arguments before reaching any backend
try:
    wifi.connect("sihd-no-such-interface", "", sihd.util.time.ms(1000))
    assert(False)
except sihd.Error as empty_ssid:
    assert(empty_ssid.code == sihd.ErrorCode.invalid_argument)
    assert(str(empty_ssid) == "an ssid holds 1 to 32 bytes")
try:
    wifi.connect("sihd-no-such-interface", "home", "short", sihd.util.time.ms(1000))
    assert(False)
except sihd.Error as short_passphrase:
    assert(short_passphrase.code == sihd.ErrorCode.invalid_argument)
    assert(str(short_passphrase) == "a passphrase holds 8 to 63 characters")

# an rsn element carrying a psk akm suite
rsn_psk = bytes([0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00,
                 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00])
assert(wifi.security_of(rsn_psk, True) == wifi.Security.personal)
assert(wifi.security_of(b"", True) == wifi.Security.wep)
assert(wifi.security_of(b"", False) == wifi.Security.open)

assert(wifi.Monitor.Event.Type.scan_done != wifi.Monitor.Event.Type.state_changed)
event = wifi.Monitor.Event()
assert(event.type == wifi.Monitor.Event.Type.scan_done)
assert(event.state == wifi.State.disconnected)

link = wifi.Link()
assert(link.ssid == "" and link.signal_dbm == 0 and link.tx_bitrate_bps == 0)
access_point = wifi.AccessPoint()
assert(access_point.security == wifi.Security.unknown and access_point.associated == False)
known = wifi.KnownNetwork()
assert(known.ssid == "" and known.security == wifi.Security.unknown)
