local wifi = sihd.wifi
local timeout = sihd.util.time.ms(1000)

local function must_raise(fn, ...)
    local ok, err = pcall(fn, ...)
    assert(ok == false, "expected an error, got success")
    return err
end

-- failures raise a sihd.Error, a host without wifi stack answers not_supported
local missing = must_raise(wifi.link, "sihd-no-such-interface")
if missing.code ~= sihd.ErrorCode.not_supported then
    assert(missing.code == sihd.ErrorCode.not_found)
    assert(missing.message == "no wifi interface named sihd-no-such-interface")
end
local unheard = must_raise(wifi.access_points, "sihd-no-such-interface")
if unheard.code ~= sihd.ErrorCode.not_supported then
    assert(unheard.code == sihd.ErrorCode.not_found)
    assert(unheard.message == "no wifi interface named sihd-no-such-interface")
end

-- the monitor fails like backend() whatever daemon this host runs
local unmanaged = must_raise(wifi.backend, "sihd-no-such-interface")
local unmonitored = must_raise(wifi.Monitor.open, "sihd-no-such-interface", timeout)
assert(unmonitored.code == unmanaged.code)
assert(unmonitored.message == unmanaged.message)

-- both connect forms check their arguments before reaching any backend
local empty_ssid = must_raise(wifi.connect, "sihd-no-such-interface", "", timeout)
assert(empty_ssid.code == sihd.ErrorCode.invalid_argument)
assert(empty_ssid.message == "an ssid holds 1 to 32 bytes")
local short_passphrase = must_raise(wifi.connect, "sihd-no-such-interface", "home", "short", timeout)
assert(short_passphrase.code == sihd.ErrorCode.invalid_argument)
assert(short_passphrase.message == "a passphrase holds 8 to 63 characters")
local not_a_passphrase = must_raise(wifi.connect, "sihd-no-such-interface", "home", 12, timeout)
assert(not_a_passphrase.code == sihd.ErrorCode.invalid_argument)
assert(not_a_passphrase.message == "connect passphrase must be a string")

-- an rsn element carrying a psk akm suite
local rsn_psk = string.char(0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00,
                            0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00)
assert(wifi.security_of(rsn_psk, true) == wifi.Security.personal)
assert(wifi.security_of("", true) == wifi.Security.wep)
assert(wifi.security_of("", false) == wifi.Security.open)

local link = wifi.Link()
assert(link.ssid == "" and link.signal_dbm == 0 and link.tx_bitrate_bps == 0)
local access_point = wifi.AccessPoint()
assert(access_point.security == wifi.Security.unknown and access_point.associated == false)
local known = wifi.KnownNetwork()
assert(known.ssid == "" and known.security == wifi.Security.unknown)
local event = wifi.Monitor.Event()
assert(event.type == wifi.Monitor.Event.Type.scan_done)
assert(event.state == wifi.State.disconnected)
assert(wifi.MonitorEvent == nil)
assert(wifi.Backend.network_manager ~= wifi.Backend.iwd)
