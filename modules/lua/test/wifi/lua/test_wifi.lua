local wifi = sihd.wifi
local timeout = sihd.util.time.ms(1000)

local function must_raise(fn, ...)
    local ok, err = pcall(fn, ...)
    assert(ok == false, "expected an error, got success")
    return err
end

local unmanaged = must_raise(wifi.backend, "sihd-no-such-interface")
local missing = must_raise(wifi.link, "sihd-no-such-interface")
assert(missing.code == sihd.ErrorCode.not_supported or missing.code == sihd.ErrorCode.not_found)
if missing.code == sihd.ErrorCode.not_found then
    assert(missing.message == "no wifi interface named sihd-no-such-interface")
end
local unheard = must_raise(wifi.access_points, "sihd-no-such-interface")
assert(unheard.code == sihd.ErrorCode.not_supported or unheard.code == sihd.ErrorCode.not_found)
if unheard.code == sihd.ErrorCode.not_found then
    assert(unheard.message == "no wifi interface named sihd-no-such-interface")
end

local unmonitored = must_raise(wifi.Monitor.open, "sihd-no-such-interface", timeout)
assert(unmonitored.code == unmanaged.code)
assert(unmonitored.message == unmanaged.message)

local empty_ssid = must_raise(wifi.connect, "sihd-no-such-interface", "", timeout)
assert(empty_ssid.code == sihd.ErrorCode.invalid_argument)
assert(empty_ssid.message == "an ssid holds 1 to 32 bytes")
local short_passphrase = must_raise(wifi.connect, "sihd-no-such-interface", "home", "short", timeout)
assert(short_passphrase.code == sihd.ErrorCode.invalid_argument)
assert(short_passphrase.message == "a passphrase holds 8 to 63 characters")
local not_a_passphrase = must_raise(wifi.connect, "sihd-no-such-interface", "home", 12, timeout)
assert(not_a_passphrase.code == sihd.ErrorCode.invalid_argument)
assert(not_a_passphrase.message == "connect passphrase must be a string")
for _, wrong in ipairs({"soon", true}) do
    local bad_timeout = must_raise(wifi.scan, "sihd-no-such-interface", wrong)
    assert(bad_timeout.code == sihd.ErrorCode.invalid_argument)
    assert(bad_timeout.message == "a timeout is a number of nanoseconds or a Duration")
end
local bad_run_timeout = must_raise(wifi.Monitor.open, "sihd-no-such-interface", "soon")
assert(bad_run_timeout.code == sihd.ErrorCode.invalid_argument)

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

local mac = string.char(0x00, 0x11, 0x22, 0x33, 0x44, 0x55)
local radiotap_header = string.char(0x00, 0x00, 0x0f, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x9e, 0x09, 0xc0, 0x00, 0xd0)
local beacon_elements = string.char(0x00, 0x04) .. "home" .. string.char(0x01, 0x02, 0x82, 0x84, 0x03, 0x01, 0x0b) .. rsn_psk
local beacon_body = string.rep("\0", 8) .. string.char(0x64, 0x00, 0x11, 0x04) .. beacon_elements
local beacon_frame = string.char(0x80, 0x00, 0x00, 0x00) .. string.rep("\255", 6) .. mac .. mac
                     .. string.char(0x30, 0x12) .. beacon_body

local packet = wifi.dissect_radiotap(radiotap_header .. beacon_frame)
assert(packet.radiotap.signal_dbm == -48)
assert(packet.radiotap.frequency_mhz == 2462)
assert(packet.radiotap.rate_kbps == 6000)
assert(packet.radiotap.header_size == 15)
assert(packet.frame.type == wifi.Frame.Type.management)
assert(packet.frame.subtype == wifi.Frame.ManagementSubtype.beacon)
assert(packet.frame.addr1 == "FF:FF:FF:FF:FF:FF")
assert(packet.frame.addr2 == "00:11:22:33:44:55")
assert(packet.frame.addr4 == nil)
assert(packet.frame.sequence_number == 0x123)
assert(wifi.addresses(packet.frame).bssid == "00:11:22:33:44:55")
assert(wifi.addresses(packet.frame).da == "FF:FF:FF:FF:FF:FF")
assert(wifi.dissect_80211(beacon_frame).radiotap == nil)

-- the decoded objects keep their bytes once the input is gone, whatever is decoded and allocated since
local frame = wifi.dissect_radiotap(radiotap_header .. beacon_frame).frame
local radiotap = wifi.dissect_radiotap(radiotap_header .. beacon_frame).radiotap
local beacon = wifi.parse_beacon(frame.body)
local other_elements = string.char(0x00, 0x05) .. "other" .. string.char(0x03, 0x01, 0x01)
local other_frame = string.char(0x80, 0x00, 0x00, 0x00) .. string.rep("\170", 6) .. mac .. mac .. string.char(0x00, 0x00)
                    .. string.rep("\0", 12) .. other_elements
local churn = {}
local noise = {}
for index = 1, 200 do
    churn[index] = wifi.dissect_radiotap(radiotap_header .. other_frame).frame
    noise[index] = string.rep(string.char(index % 251), 512)
end
collectgarbage()
assert(frame.body == beacon_body)
assert(radiotap.payload == beacon_frame)
assert(beacon.information_elements == beacon_elements)
assert(beacon.interval_tu == 100)
assert(beacon.ess and beacon.privacy and not beacon.ibss)
assert(churn[1].addr1 == "AA:AA:AA:AA:AA:AA" and #noise == 200)

local only_radiotap = wifi.parse_radiotap(radiotap_header .. beacon_frame)
assert(only_radiotap.signal_dbm == -48 and only_radiotap.noise_dbm == nil and not only_radiotap.fcs_at_end)
assert(wifi.parse_frame(beacon_frame).addr3 == "00:11:22:33:44:55")
local qos = wifi.parse_frame(string.char(0x88, 0x01, 0x00, 0x00) .. mac .. mac .. mac .. string.char(0x00, 0x00, 0x85, 0x00))
assert(qos.amsdu and qos.qos_tid == 5)
local ack = wifi.parse_frame(string.char(0xd4, 0x00, 0x00, 0x00) .. mac)
assert(ack.subtype == wifi.Frame.ControlSubtype.ack and ack.addr2 == nil)

local antennas = wifi.parse_radiotap(string.char(0x00, 0x00, 0x16, 0x00, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0xa0,
                                                 0x20, 0x08, 0x00, 0x00, 0xc8, 0x00, 0xc4, 0x01, 0xc2, 0x02))
assert(antennas.signal_dbm == -56 and #antennas.antenna_signals == 2)
assert(antennas.antenna_signals[1].antenna == 1 and antennas.antenna_signals[1].signal_dbm == -60)
assert(antennas.antenna_signals[2].antenna == 2 and antennas.antenna_signals[2].signal_dbm == -62)

assert(wifi.parse_probe_response(beacon_body).interval_tu == 100)
local request = wifi.parse_assoc_request(string.char(0x31, 0x04, 0x0a, 0x00, 0x00, 0x00))
assert(request.capability == 0x0431 and request.listen_interval == 10 and request.information_elements == "\0\0")
local reassociation = wifi.parse_reassoc_request(string.char(0x31, 0x04, 0x0a, 0x00) .. mac .. string.char(0x00, 0x00))
assert(reassociation.current_ap == "00:11:22:33:44:55" and reassociation.listen_interval == 10)
local response = wifi.parse_assoc_response(string.char(0x31, 0x04, 0x00, 0x00, 0x05, 0xc0))
assert(response.status == 0 and response.aid == 5)
local authentication = wifi.parse_authentication(string.char(0x03, 0x00, 0x01, 0x00, 0x00, 0x00))
assert(authentication.algorithm == 3 and authentication.sequence == 1 and authentication.status == 0)
local action = wifi.parse_action(string.char(0x03, 0x01, 0x02))
assert(action.category == 3 and action.body == "\1\2")

local ies = wifi.ies
assert(ies.ssid(beacon.information_elements) == "home")
assert(ies.channel(beacon.information_elements) == 11)
local rates = ies.rates(beacon.information_elements)
assert(#rates == 2 and rates[1].kbps == 1000 and rates[2].basic)
local elements = ies.elements(beacon.information_elements)
assert(#elements == 4 and elements[1].id == 0 and elements[4].id == 48)
local rsn = ies.rsn(beacon.information_elements)
assert(ies.cipher_str(rsn.group) == "ccmp")
assert(ies.akm_str(rsn.akms[1]) == "psk")
assert(ies.cipher_str(rsn.pairwise[1]) == "ccmp" and rsn.group.type == 4 and rsn.group.oui == string.char(0x00, 0x0f, 0xac))
assert(ies.wpa(beacon.information_elements) == nil)
assert(ies.wps(beacon.information_elements) == nil)

local wpa = ies.wpa(string.char(0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00,
                                0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02))
assert(ies.cipher_str(wpa.group) == "tkip" and ies.akm_str(wpa.akms[1]) == "psk")

assert(ies.ssid(string.char(0x00, 0x04, 0x61, 0xff, 0xfe, 0x62)) == "a\255\254b")
local wps = ies.wps(string.char(0xdd, 0x10, 0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x21, 0x00, 0x03, 0xff,
                                0x61, 0x62))
assert(wps.version == 0x10 and wps.manufacturer == "\255ab" and wps.model_name == "" and not wps.ap_setup_locked)

local llc = wifi.parse_llc_snap(string.char(0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e, 0x01))
assert(llc.ethertype == 0x888e and llc.payload == "\1" and llc.oui == "\0\0\0")
assert(wifi.parse_reason(string.char(0x07, 0x00)) == 7)

local mcs = wifi.parse_radiotap(string.char(0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x07, 0x05, 0x07)).mcs
assert(mcs.index == 7 and mcs.bandwidth_mhz == 40 and mcs.short_gi and mcs.rate_kbps == 150000)

local ht = ies.ht_capabilities(string.char(0x2d, 0x1a, 0xce, 0x11, 0x1b, 0xff, 0xff) .. string.rep("\0", 10) .. "\1"
                               .. string.rep("\0", 10))
assert(ht.width_40mhz and ht.short_gi_40mhz and ht.rx_spatial_streams == 2 and ht.ampdu_length_exponent == 3)
local vht = ies.vht_capabilities(string.char(0xbf, 0x0c, 0xb0, 0x71, 0xd0, 0x33, 0xfa, 0xff, 0x68, 0x01, 0xfa, 0xff, 0x68, 0x01))
assert(vht.su_beamformee and vht.mu_beamformee and vht.rx_spatial_streams == 2 and vht.max_mpdu_length == 3895)
local country = ies.country(string.char(0x07, 0x09, 0x55, 0x53, 0x20, 0x01, 0x0b, 0x1e, 0x24, 0x04, 0x17))
assert(country.code == "US" and country.environment == " " and country.channels[2].first == 36)
local odd_country = ies.country(string.char(0x07, 0x06, 0xff, 0xfe, 0xfd, 0x01, 0x0b, 0x1e))
assert(odd_country.code == "\255\254" and odd_country.environment == "\253" and odd_country.channels[1].count == 11)
assert(ies.tim(string.char(0x05, 0x04, 0x00, 0x02, 0x01, 0x00)).multicast_buffered)
local standards = ies.standards(string.char(0x2d, 0x00, 0xbf, 0x00, 0xff, 0x01, 0x23))
assert(standards.ht and standards.vht and standards.he and not standards.eht)
local vendors = ies.vendors(string.char(0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x7f))
assert(vendors[1].oui == string.char(0x00, 0x10, 0x18) and vendors[1].type == 2 and vendors[1].data == "\127")
local readable = ies.readable_elements(string.char(0x00, 0x00, 0x01, 0x02, 0x02))
assert(#readable == 1 and readable[1].id == 0)

assert(wifi.taxonomy.signature(string.char(0x00, 0x00, 0x01, 0x01, 0x02), string.char(0x00, 0x00)) == "wifi4|probe:0,1|assoc:0")
assert(wifi.taxonomy.element_signature("") == "")

assert(wifi.management_subtype(packet.frame) == wifi.Frame.ManagementSubtype.beacon)
assert(wifi.control_subtype(packet.frame) == nil)
assert(wifi.control_subtype(ack) == wifi.Frame.ControlSubtype.ack)
assert(wifi.management_subtype(ack) == nil)
local tack = wifi.parse_frame(string.char(0x34, 0x00, 0x00, 0x00) .. mac .. mac .. mac .. "\0\0")
assert(wifi.control_subtype(tack) == wifi.Frame.ControlSubtype.tack)
local extension = wifi.parse_frame(string.char(0x64, 0x00, 0x00, 0x00) .. mac .. mac .. mac .. "\0\0")
assert(wifi.control_subtype(extension) == wifi.Frame.ControlSubtype.control_frame_extension)

assert(wifi.parse_mac("00:11:22:33:44:55") == mac)
assert(wifi.is_group("ff:ff:ff:ff:ff:ff") and not wifi.is_group("00:11:22:33:44:55"))
assert(wifi.is_locally_administered("da:a1:19:00:00:01") and not wifi.is_locally_administered("00:11:22:33:44:55"))

local ds_parameter = ies.find(beacon.information_elements, ies.Id.ds_parameter)
assert(ds_parameter.id == ies.Id.ds_parameter and ds_parameter.data == "\11")
assert(ies.find(beacon.information_elements, ies.Id.tim) == nil)
local broadcom = string.char(0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x7f)
local vendor = ies.find_vendor(broadcom, string.char(0x00, 0x10, 0x18), 2)
assert(vendor.type == 2 and vendor.data == "\127" and vendor.oui == string.char(0x00, 0x10, 0x18))
assert(ies.find_vendor(broadcom, string.char(0x00, 0x10, 0x18), 3) == nil)
assert(ies.vendor_of(ies.elements(broadcom)[1]).type == 2)
assert(ies.vendor_of(ies.elements(beacon.information_elements)[1]) == nil)
assert(ies.parse_rsn(rsn_psk:sub(3)).group.type == 4)
local wps_attributes = string.char(0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x21, 0x00, 0x03, 0xff, 0x61, 0x62)
assert(ies.parse_wps(wps_attributes).manufacturer == "\255ab")
local attributes = ies.wps_attributes(wps_attributes)
assert(#attributes == 2 and attributes[1].id == 0x104a and attributes[1].value == "\16")
assert(attributes[2].id == 0x1021 and attributes[2].value == "\255ab")

local truncated_inputs = {
    {"parse_mac", wifi.parse_mac, "00:11"},
    {"is_group", wifi.is_group, "zz:11:22:33:44:55"},
    {"parse_rsn", ies.parse_rsn, string.rep("\0", 3)},
    {"parse_wps", ies.parse_wps, string.char(0x10, 0x21, 0x00, 0x09, 0x61)},
    {"dissect_80211", wifi.dissect_80211, ""},
    {"dissect_radiotap", wifi.dissect_radiotap, string.char(0x00, 0x00, 0x08)},
    {"parse_radiotap", wifi.parse_radiotap, string.char(0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00)},
    {"parse_frame", wifi.parse_frame, string.char(0x80, 0x00, 0x00)},
    {"parse_beacon", wifi.parse_beacon, string.rep("\0", 11)},
    {"parse_assoc_response", wifi.parse_assoc_response, string.rep("\0", 5)},
    {"parse_authentication", wifi.parse_authentication, string.rep("\0", 5)},
    {"parse_reason", wifi.parse_reason, "\0"},
    {"parse_llc_snap", wifi.parse_llc_snap, string.char(0xaa, 0xaa, 0x03)},
    {"ht_capabilities", ies.ht_capabilities, string.char(0x2d, 0x05, 0xce, 0x11, 0x1b, 0xff, 0xff)},
    {"vht_capabilities", ies.vht_capabilities, string.char(0xbf, 0x04, 0xb0, 0x71, 0xd0, 0x33)},
    {"country", ies.country, string.char(0x07, 0x02, 0x55, 0x53)},
    {"tim", ies.tim, string.char(0x05, 0x02, 0x00, 0x01)},
    {"elements", ies.elements, string.char(0x00, 0x05, 0x61)},
}
for _, case in ipairs(truncated_inputs) do
    local truncated = must_raise(case[2], case[3])
    assert(truncated.code == sihd.ErrorCode.invalid_argument, case[1])
end
