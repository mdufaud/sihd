import gc

import sihd

wifi = sihd.wifi

try:
    wifi.backend("sihd-no-such-interface")
    assert(False)
except sihd.Error as error:
    unmanaged = error

try:
    wifi.link("sihd-no-such-interface")
    assert(False)
except sihd.Error as missing:
    assert(missing.code in (sihd.ErrorCode.not_supported, sihd.ErrorCode.not_found))
    if missing.code == sihd.ErrorCode.not_found:
        assert(str(missing) == "no wifi interface named sihd-no-such-interface")

try:
    wifi.access_points("sihd-no-such-interface")
    assert(False)
except sihd.Error as unheard:
    assert(unheard.code in (sihd.ErrorCode.not_supported, sihd.ErrorCode.not_found))
    if unheard.code == sihd.ErrorCode.not_found:
        assert(str(unheard) == "no wifi interface named sihd-no-such-interface")

monitor = wifi.Monitor(sihd.util.time.ms(1000))
try:
    monitor.open("sihd-no-such-interface")
    assert(False)
except sihd.Error as unmonitored:
    assert(unmonitored.code == unmanaged.code)
    assert(str(unmonitored) == str(unmanaged))
assert(monitor.run() == False)

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

for network in (link, access_point, known):
    network.ssid = bytes([0x61, 0xff, 0xfe, 0x62])
    assert(network.ssid == "a\ufffd\ufffdb")

mac = bytes([0x00, 0x11, 0x22, 0x33, 0x44, 0x55])
radiotap_header = bytes([0x00, 0x00, 0x0f, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x9e, 0x09, 0xc0, 0x00, 0xd0])
beacon_elements = bytes([0x00, 0x04]) + b"home" + bytes([0x01, 0x02, 0x82, 0x84, 0x03, 0x01, 0x0b]) + rsn_psk
beacon_body = bytes(8) + bytes([0x64, 0x00, 0x11, 0x04]) + beacon_elements
beacon_frame = bytes([0x80, 0x00, 0x00, 0x00]) + b"\xff" * 6 + mac + mac + bytes([0x30, 0x12]) + beacon_body

packet = wifi.dissect_radiotap(radiotap_header + beacon_frame)
assert(packet.radiotap.signal_dbm == -48)
assert(packet.radiotap.frequency_mhz == 2462)
assert(packet.radiotap.rate_kbps == 6000)
assert(packet.radiotap.header_size == 15)
assert(packet.frame.type == wifi.Frame.Type.management)
assert(packet.frame.subtype == int(wifi.Frame.ManagementSubtype.beacon))
assert(packet.frame.addr1 == "FF:FF:FF:FF:FF:FF")
assert(packet.frame.addr2 == "00:11:22:33:44:55")
assert(packet.frame.addr4 is None)
assert(packet.frame.sequence_number == 0x123)
assert(wifi.addresses(packet.frame).bssid == "00:11:22:33:44:55")
assert(wifi.addresses(packet.frame).ra == "FF:FF:FF:FF:FF:FF")
assert(wifi.addresses(packet.frame).da == "FF:FF:FF:FF:FF:FF")
assert(wifi.dissect_80211(beacon_frame).radiotap is None)

# the decoded objects keep their bytes once the input is gone, whatever is decoded and allocated since
frame = wifi.dissect_radiotap(radiotap_header + beacon_frame).frame
radiotap = wifi.dissect_radiotap(radiotap_header + beacon_frame).radiotap
beacon = wifi.parse_beacon(frame.body)
other_elements = bytes([0x00, 0x05]) + b"other" + bytes([0x03, 0x01, 0x01])
other_frame = bytes([0x80, 0x00, 0x00, 0x00]) + b"\xaa" * 6 + mac + mac + bytes([0x00, 0x00]) + bytes(12) + other_elements
churn = [wifi.dissect_radiotap(radiotap_header + other_frame).frame for _ in range(200)]
noise = [bytes([index % 251]) * 512 for index in range(200)]
gc.collect()
assert(frame.body == beacon_body)
assert(radiotap.payload == beacon_frame)
assert(beacon.information_elements == beacon_elements)
assert(beacon.interval_tu == 100)
assert(beacon.ess and beacon.privacy and not beacon.ibss)
assert(churn[0].addr1 == "AA:AA:AA:AA:AA:AA" and len(noise) == 200)

only_radiotap = wifi.parse_radiotap(radiotap_header + beacon_frame)
assert(only_radiotap.signal_dbm == -48 and only_radiotap.noise_dbm is None and not only_radiotap.fcs_at_end)
assert(wifi.parse_frame(beacon_frame).addr3 == "00:11:22:33:44:55")
qos = wifi.parse_frame(bytes([0x88, 0x01, 0x00, 0x00]) + mac * 3 + bytes([0x00, 0x00, 0x85, 0x00]))
assert(qos.amsdu and qos.qos_tid == 5)
ack = wifi.parse_frame(bytes([0xd4, 0x00, 0x00, 0x00]) + mac)
assert(ack.subtype == int(wifi.Frame.ControlSubtype.ack) and ack.addr2 is None)

antennas = wifi.parse_radiotap(bytes([0x00, 0x00, 0x16, 0x00, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08, 0x00, 0xa0, 0x20, 0x08,
                                      0x00, 0x00, 0xc8, 0x00, 0xc4, 0x01, 0xc2, 0x02]))
assert(antennas.signal_dbm == -56 and len(antennas.antenna_signals) == 2)
assert(antennas.antenna_signals[0].antenna == 1 and antennas.antenna_signals[0].signal_dbm == -60)
assert(antennas.antenna_signals[1].antenna == 2 and antennas.antenna_signals[1].signal_dbm == -62)

assert(wifi.parse_probe_response(beacon_body).interval_tu == 100)
request = wifi.parse_assoc_request(bytes([0x31, 0x04, 0x0a, 0x00, 0x00, 0x00]))
assert(request.capability == 0x0431 and request.listen_interval == 10 and request.information_elements == bytes([0, 0]))
reassociation = wifi.parse_reassoc_request(bytes([0x31, 0x04, 0x0a, 0x00]) + mac + bytes([0x00, 0x00]))
assert(reassociation.current_ap == "00:11:22:33:44:55" and reassociation.listen_interval == 10)
response = wifi.parse_assoc_response(bytes([0x31, 0x04, 0x00, 0x00, 0x05, 0xc0]))
assert(response.status == 0 and response.aid == 5)
authentication = wifi.parse_authentication(bytes([0x03, 0x00, 0x01, 0x00, 0x00, 0x00]))
assert(authentication.algorithm == 3 and authentication.sequence == 1 and authentication.status == 0)
action = wifi.parse_action(bytes([0x03, 0x01, 0x02]))
assert(action.category == 3 and action.body == bytes([0x01, 0x02]))

ies = wifi.ies
assert(ies.ssid(beacon.information_elements) == "home")
assert(ies.channel(beacon.information_elements) == 11)
assert([rate.kbps for rate in ies.rates(beacon.information_elements)] == [1000, 2000])
assert([element.id for element in ies.elements(beacon.information_elements)] == [0, 1, 3, 48])
rsn = ies.rsn(beacon.information_elements)
assert(ies.cipher_str(rsn.group) == "ccmp")
assert(ies.akm_str(rsn.akms[0]) == "psk")
assert(ies.cipher_str(rsn.pairwise[0]) == "ccmp" and rsn.group.type == 4 and rsn.group.oui == bytes([0x00, 0x0f, 0xac]))
assert(ies.wpa(beacon.information_elements) is None)
assert(ies.wps(beacon.information_elements) is None)

wpa = ies.wpa(bytes([0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02,
                     0x01, 0x00, 0x00, 0x50, 0xf2, 0x02]))
assert(ies.cipher_str(wpa.group) == "tkip" and ies.akm_str(wpa.akms[0]) == "psk")

assert(ies.ssid(bytes([0x00, 0x04, 0x61, 0xff, 0xfe, 0x62])) == "a\ufffd\ufffdb")
wps = ies.wps(bytes([0xdd, 0x10, 0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x21, 0x00, 0x03, 0xff, 0x61, 0x62]))
assert(wps.version == 0x10 and wps.manufacturer == "\ufffdab" and wps.model_name == "" and not wps.ap_setup_locked)

llc = wifi.parse_llc_snap(bytes([0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e, 0x01]))
assert(llc.ethertype == 0x888e and llc.payload == b"\x01" and llc.oui == bytes(3))
assert(wifi.parse_reason(bytes([0x07, 0x00])) == 7)

mcs = wifi.parse_radiotap(bytes([0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x07, 0x05, 0x07])).mcs
assert(mcs.index == 7 and mcs.bandwidth_mhz == 40 and mcs.short_gi and mcs.rate_kbps == 150000)

ht = ies.ht_capabilities(bytes([0x2d, 0x1a, 0xce, 0x11, 0x1b, 0xff, 0xff]) + bytes(10) + bytes([0x01]) + bytes(10))
assert(ht.width_40mhz and ht.short_gi_40mhz and ht.rx_spatial_streams == 2 and ht.ampdu_length_exponent == 3)
vht = ies.vht_capabilities(bytes([0xbf, 0x0c, 0xb0, 0x71, 0xd0, 0x33, 0xfa, 0xff, 0x68, 0x01, 0xfa, 0xff, 0x68, 0x01]))
assert(vht.su_beamformee and vht.mu_beamformee and vht.rx_spatial_streams == 2 and vht.max_mpdu_length == 3895)
country = ies.country(bytes([0x07, 0x09, 0x55, 0x53, 0x20, 0x01, 0x0b, 0x1e, 0x24, 0x04, 0x17]))
assert(country.code == "US" and country.environment == " " and country.channels[1].first == 36)
odd_country = ies.country(bytes([0x07, 0x06, 0xff, 0xfe, 0xfd, 0x01, 0x0b, 0x1e]))
assert(odd_country.code == "\ufffd\ufffd" and odd_country.environment == "\ufffd" and odd_country.channels[0].count == 11)
assert(ies.tim(bytes([0x05, 0x04, 0x00, 0x02, 0x01, 0x00])).multicast_buffered)
standards = ies.standards(bytes([0x2d, 0x00, 0xbf, 0x00, 0xff, 0x01, 0x23]))
assert(standards.ht and standards.vht and standards.he and not standards.eht)
vendors = ies.vendors(bytes([0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x7f]))
assert(vendors[0].oui == bytes([0x00, 0x10, 0x18]) and vendors[0].type == 2 and vendors[0].data == b"\x7f")
assert([element.id for element in ies.readable_elements(bytes([0x00, 0x00, 0x01, 0x02, 0x02]))] == [0])

assert(wifi.taxonomy.signature(bytes([0x00, 0x00, 0x01, 0x01, 0x02]), bytes([0x00, 0x00])) == "wifi4|probe:0,1|assoc:0")
assert(wifi.taxonomy.element_signature(b"") == "")

assert(wifi.management_subtype(packet.frame) == wifi.Frame.ManagementSubtype.beacon)
assert(wifi.control_subtype(packet.frame) is None)
assert(wifi.control_subtype(ack) == wifi.Frame.ControlSubtype.ack)
assert(wifi.management_subtype(ack) is None)
tack = wifi.parse_frame(bytes([0x34, 0x00, 0x00, 0x00]) + mac * 3 + bytes(2))
assert(wifi.control_subtype(tack) == wifi.Frame.ControlSubtype.tack)
extension = wifi.parse_frame(bytes([0x64, 0x00, 0x00, 0x00]) + mac * 3 + bytes(2))
assert(wifi.control_subtype(extension) == wifi.Frame.ControlSubtype.control_frame_extension)

assert(wifi.parse_mac("00:11:22:33:44:55") == mac)
assert(wifi.is_group("ff:ff:ff:ff:ff:ff") and not wifi.is_group("00:11:22:33:44:55"))
assert(wifi.is_locally_administered("da:a1:19:00:00:01") and not wifi.is_locally_administered("00:11:22:33:44:55"))

ds_parameter = ies.find(beacon.information_elements, ies.Id.ds_parameter)
assert(ds_parameter.id == int(ies.Id.ds_parameter) and ds_parameter.data == b"\x0b")
assert(ies.find(beacon.information_elements, ies.Id.tim) is None)
broadcom = bytes([0xdd, 0x05, 0x00, 0x10, 0x18, 0x02, 0x7f])
vendor = ies.find_vendor(broadcom, bytes([0x00, 0x10, 0x18]), 2)
assert(vendor.type == 2 and vendor.data == b"\x7f" and vendor.oui == bytes([0x00, 0x10, 0x18]))
assert(ies.find_vendor(broadcom, bytes([0x00, 0x10, 0x18]), 3) is None)
assert(ies.vendor_of(ies.elements(broadcom)[0]).type == 2)
assert(ies.vendor_of(ies.elements(beacon.information_elements)[0]) is None)
assert(ies.parse_rsn(rsn_psk[2:]).group.type == 4)
wps_attributes = bytes([0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x21, 0x00, 0x03, 0xff, 0x61, 0x62])
assert(ies.parse_wps(wps_attributes).manufacturer == "\ufffdab")
assert([(attribute.id, attribute.value) for attribute in ies.wps_attributes(wps_attributes)] == [(0x104a, b"\x10"), (0x1021, b"\xffab")])

truncated_inputs = [
    (wifi.parse_mac, "00:11"),
    (wifi.is_group, "zz:11:22:33:44:55"),
    (ies.parse_rsn, bytes(3)),
    (ies.parse_wps, bytes([0x10, 0x21, 0x00, 0x09, 0x61])),
    (ies.find_vendor, broadcom, bytes(2), 2),
    (wifi.dissect_80211, b""),
    (wifi.dissect_radiotap, bytes([0x00, 0x00, 0x08])),
    (wifi.parse_radiotap, bytes([0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00])),
    (wifi.parse_frame, bytes([0x80, 0x00, 0x00])),
    (wifi.parse_beacon, bytes(11)),
    (wifi.parse_assoc_response, bytes(5)),
    (wifi.parse_authentication, bytes(5)),
    (wifi.parse_reason, bytes(1)),
    (wifi.parse_llc_snap, bytes([0xaa, 0xaa, 0x03])),
    (ies.ht_capabilities, bytes([0x2d, 0x05, 0xce, 0x11, 0x1b, 0xff, 0xff])),
    (ies.vht_capabilities, bytes([0xbf, 0x04, 0xb0, 0x71, 0xd0, 0x33])),
    (ies.country, bytes([0x07, 0x02, 0x55, 0x53])),
    (ies.tim, bytes([0x05, 0x02, 0x00, 0x01])),
    (ies.elements, bytes([0x00, 0x05, 0x61])),
]
for decoder, *arguments in truncated_inputs:
    try:
        decoder(*arguments)
        assert(False), decoder.__name__
    except sihd.Error as truncated:
        assert(truncated.code == sihd.ErrorCode.invalid_argument), decoder.__name__
