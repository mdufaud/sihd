# Reading 802.11 frames

A primer for the decoders of the [wifi module](../modules/wifi.md#frame-decoding): what is in a captured wifi packet, byte by byte, and what each word means. The headers under `modules/wifi/include/sihd/wifi/` only declare the types; every layout and every field is explained here, layer by layer:

| What | Header | Source |
|---|---|---|
| radiotap header (signal, channel, rate) | `radiotap.hpp` | `radiotap.cpp` |
| 802.11 MAC header, addresses, frame types | `frame.hpp` | `frame.cpp` |
| management bodies (beacon, probe, association ...) | `management.hpp` | `management.cpp` |
| information elements (SSID, rates, RSN, WPS ...) | `ies.hpp` | `ies.cpp` |
| what a device announces (HT, VHT, country, TIM ...) | `capabilities.hpp` | `capabilities.cpp` |
| LLC/SNAP, encryption | `data.hpp` | `data.cpp` |
| the whole pipeline in one call | `dissect.hpp` | `dissect.cpp` |
| one-line device signature | `taxonomy.hpp` | `taxonomy.cpp` |

## Where a packet comes from

A normal wifi card hides the radio layer: the driver strips the 802.11 header and gives the system an ethernet-like packet (pcap datalink 1). To see the raw frames the card must be put in **monitor mode**, by hand, before capturing. The driver then prefixes each frame with a **radiotap** header (datalink 127), or gives the bare frame (datalink 105). The wifi module reads only those two; a datalink 1 capture holds no 802.11 frame to decode.

```
pcap record, datalink 127
+------------------+--------------------------------------------+---------+
| radiotap header  | 802.11 frame                               | FCS     |
| signal, channel  | MAC header + body                          | 4 bytes |
+------------------+--------------------------------------------+---------+
```

## One beacon, byte by byte

The AP "home" announcing itself on channel 11, the example used by the Python and Lua tests. All multi-byte numbers are little-endian (low byte first).

```
RADIOTAP (15 bytes)
00            version, always 0
00            padding
0f 00         length 15: the 802.11 frame starts at byte 15
2e 00 00 00   present bits 1,2,3,5 = flags, rate, channel, antenna signal
00            flags (no FCS at the end)
0c            rate 12 * 500 kb/s = 6 Mb/s
9e 09         frequency 0x099e = 2462 MHz = 2.4 GHz channel 11
c0 00         channel flags 0x00c0: 2.4 GHz band, OFDM modulation
d0            signal 0xd0 = -48 dBm

802.11 MAC HEADER (24 bytes)
80 00         frame control: byte0 = 1000 00 00 -> subtype 8, type 0, version 0 = management / beacon; byte1 = no flag
00 00         duration
ff ff ff ff ff ff   address 1 = destination = everyone (broadcast)
00 11 22 33 44 55   address 2 = source = the AP
00 11 22 33 44 55   address 3 = BSSID = the AP again
30 12         sequence control 0x1230: fragment 0, sequence number 0x123 = 291

BEACON FIXED FIELDS (12 bytes)
00 00 00 00 00 00 00 00   timestamp, the AP's microsecond clock
64 00         beacon interval 100 time units = 102.4 ms
11 04         capability 0x0411: bit 0 ESS (infrastructure network), bit 4 privacy (encrypted), bit 10 short slot

INFORMATION ELEMENTS (id, length, data)
00 04 68 6f 6d 65   SSID, 4 bytes = "home"
01 02 82 84         supported rates: 0x82 = basic 1 Mb/s, 0x84 = basic 2 Mb/s
03 01 0b            DS parameter set: channel 11
30 14               RSN element, 20 bytes:
   01 00              version 1
   00 0f ac 04        group cipher: IEEE OUI, type 4 = CCMP (AES)
   01 00              1 pairwise cipher:
   00 0f ac 04          CCMP
   01 00              1 AKM:
   00 0f ac 02          PSK (a shared password)           -> WPA2-Personal
   00 00              RSN capabilities
```

`dissect_radiotap()` returns the radiotap fields and the frame, `parse_beacon()` the fixed fields, `ies::elements()` the records, then `ies::ssid()`, `ies::channel()`, `ies::rates()` and `ies::rsn()` read them. `fcs_at_end` would be set if the flags byte were `0x10`; the last 4 bytes would then be cut from the frame.

## The layers, one by one

Each part below names the header that declares it and the functions that read it. The headers hold the declarations only; the layouts live here.

### Radiotap (`radiotap.hpp`, `parse_radiotap`)

A wifi card in monitor mode hands the capture program each 802.11 frame with a small header glued in front of it: the radiotap header (pcap datalink 127, `DLT_IEEE802_11_RADIO`). It is not part of what went over the air. It tells how the radio heard the frame: at which signal level, on which channel, at which rate, at which time.

```
one captured packet (one pcap record):
+------------------+------------------------------------------+-----------------+
| radiotap header  | 802.11 frame (see below)                 | FCS (4 bytes)   |
| `header_size` B  |                                          | only if flagged |
+------------------+------------------------------------------+-----------------+
^ byte 0           ^ byte `header_size` = start of `payload`  ^ cut by `fcs_at_end`
```

The header itself, all numbers little-endian:

```
offset  0       1       2               4                       8
       +-------+-------+---------------+-----------------------+----------------...
       |version|  pad  |    length     |    present word 0     | [present word 1] fields...
       |  = 0  |       | (whole header)|    (32 bits)          | [only if bit 31 of word 0]
       +-------+-------+---------------+-----------------------+----------------...
```

- `length` is the size of the whole header, fields included: the 802.11 frame starts right after it. This is why a reader can skip a header made of fields it does not know.
- the `present` words are bitmasks. Bit N set means "field number N is stored in this header". Bit 31 set means "another present word follows". Bits 29 and 30 do not name a field: they switch the next word to another "namespace" (29: a fresh radiotap namespace, used to repeat the fields once per antenna; 30: a vendor namespace, whose data is skipped using the length it announces).
- the fields follow in bit order, absent ones take no room. Each one is aligned to its largest member, counting from the first byte of the radiotap header: a field of 2-byte members (the channel is a frequency then flags) starts at an even offset, the 8-byte TSFT at a multiple of 8. Padding bytes are inserted to get there, they carry nothing.

Example, 15 bytes, present word `0x0000002e` = bits 1 (flags), 2 (rate), 3 (channel) and 5 (antenna signal):

```
offset:  0  1  2  3    4  5  6  7     8      9      10 11   12 13      14
bytes:   00 00 0f 00 | 2e 00 00 00 | 00   | 0c   | 6c 09 | c0 00   | d0
         ver pad length  present     flags  rate   freq    channel   signal
                                                           flags
```

Rate `0x0c` = 12 * 500 kb/s = 6 Mb/s, frequency `0x096c` = 2412 MHz (2.4 GHz channel 1), channel flags `0x00c0` = 2.4 GHz band (`0x0080`) and OFDM modulation (`0x0040`) as 6 Mb/s needs, signal `0xd0` = -48 dBm. The channel field is 2 bytes aligned: it starts at offset 10, an even offset, so no padding byte was needed.

The fields the parser steps over or reads, by bit number in the present word (sizes and alignments follow radiotap.org and the Linux radiotap parser): 0 TSFT (8 bytes, aligned to 8), 1 flags, 2 rate, 3 channel (frequency 2 B + flags 2 B), 4 FHSS, 5 antenna signal in dBm, 6 antenna noise in dBm, 7 lock quality, 8 TX attenuation, 9 dB TX attenuation, 10 TX power in dBm, 11 antenna index, 12 signal in dB, 13 noise in dB, 14 RX flags, 15 TX flags, 16 RTS retries, 17 data retries, 18 XChannel (not in the official list: the walk stops on it), 19 MCS (3 bytes), 20 A-MPDU status, 21 VHT, 22 timestamp, 23 HE, 24 HE-MU, 25 HE-MU other user, 26 zero-length PSDU, 27 L-SIG, 28 TLV (the rest of the header is a list of records, the walk stops). Only the fields the `Radiotap` struct holds are kept.

A field the parser does not know the size of stops the walk (everything after it would be misplaced): what was read before it stays valid, and `header_size` still lets the caller reach the frame.

What `Radiotap` holds:

| Field | Meaning |
|---|---|
| `tsft_us` | the card's microsecond timer when the frame arrived: relative to the card, not wall-clock time |
| `rate_kbps` | data rate the frame was sent at; absent when the rate is given as an index instead (see `mcs`); a VHT or HE index is not decoded |
| `mcs` | the HT (802.11n) modulation: `index` (0-7 one spatial stream, 8-15 two, 16-23 three, 24-31 four), `bandwidth_mhz` (20 or 40, 0 when unsaid), `short_gi` (400 ns instead of 800 ns between symbols, raises the rate by 10/9), `greenfield`, `ldpc` (low-density parity-check coding instead of the convolutional one), `stbc_streams` (extra streams sent to be easier to receive), `rate_kbps` (from index, bandwidth and guard interval; empty when one is unknown or the index is above 31) |
| `frequency_mhz` | centre frequency: 2412 MHz is channel 1 (2.4 GHz: channel = (MHz - 2407) / 5, 5 GHz: (MHz - 5000) / 5) |
| `channel_flags` | modulation and band bits as the driver reports them (CCK, OFDM, 2 GHz, 5 GHz ...), not decoded |
| `signal_dbm`, `noise_dbm` | received power, around -30 (very close) down to -90 (barely heard); `signal_dbm - noise_dbm` is the signal-to-noise ratio |
| `antenna` | index of the antenna that heard the frame |
| `antenna_signals` | one entry per extra radiotap namespace that carries both an antenna index and a signal: the radio may have several antennas, each hears the frame differently |
| `short_preamble`, `wep`, `fragmented` | radiotap flags |
| `fcs_at_end` | the 4-byte frame check sequence is still glued at the end of the packet: `payload` drops it. Assumes the capture was not cut by its snaplen: on a cut packet those 4 bytes belong to the frame |
| `data_padding` | the driver inserted zero bytes between the 802.11 header and its body so the body starts on a 4-byte boundary: `dissect_radiotap()` skips them |
| `bad_fcs` | the card computed the CRC and it failed: the frame is corrupted, its fields may be garbage |
| `header_size`, `payload` | size of the radiotap header, and the 802.11 frame that follows it |

### The 802.11 MAC header (`frame.hpp`, `parse_frame`)

802.11 sends three kinds of frames (a fourth, the extension frames of 60 GHz and S1G radios, is not decoded), told apart by the `type` and `subtype` bits of the first two bytes:

| Type | Value | Role |
|---|---|---|
| management | 0 | network life: beacons (the AP announcing itself), probes (a station looking for a network), authentication, association, deauthentication, action |
| control | 1 | channel access: ACK (a frame arrived), RTS/CTS (reserve the air), PS-Poll, block ack |
| data | 2 | the payload of the stations: ethernet-like traffic |
| extension | 3 | 60 GHz (DMG) and S1G radios: `not_supported` |

Layout of a management or data frame:

```
offset 0      2        4        10       16       22          24       (30)     (+2)      (+4)
       +------+--------+--------+--------+--------+-----------+--------+--------+---------+---------+- - - - -
       | Frame| Dura-  | Addr 1 | Addr 2 | Addr 3 | Sequence  | Addr 4 | QoS    | HT      | body ...
       | Ctrl | tion   |        |        |        | Control   |        | Control| Control |
       +------+--------+--------+--------+--------+-----------+--------+--------+---------+- - - - - -
        2 B     2 B      6 B      6 B      6 B      2 B         6 B      2 B      4 B
       always (a "control" frame stops after Addr 1 or 2)       WDS only  QoS data  Order bit set
                                                                          subtypes
```

`header_size` is the length of all of this; `body` is what comes after.

Frame control, the first 2 bytes, sent low byte first:

```
byte 0:  bit 7 6 5 4 | 3 2 | 1 0                     byte 1:  bit 7     6         5     4     3     2     1    0
             subtype | type | protocol version (0)                order protected more  power retry more  from to
                                                                                  data  mgmt        frag  ds   ds
```

- `to_ds` / `from_ds`: "the frame goes to / comes from the distribution system", i.e. towards / from an AP. They decide what Addr 1..4 mean, see the table below.
- `retry`: this is a retransmission of a frame that was not acknowledged
- `power_management`: the sender announces it goes to sleep after this frame (phones do it all the time)
- `more_data` / `more_fragments`: more buffered frames for a sleeping station / more pieces of a split frame follow
- `protected_frame`: the body is encrypted (WEP/TKIP/CCMP/GCMP). The header is never encrypted.
- `order`: on QoS data and management frames, a 4-byte "HT Control" field follows the header (`ht_control`, not decoded)

Which address is which:

```
to_ds from_ds | Addr 1   Addr 2   Addr 3   Addr 4 | the frame is
------------- + ---------------------------------- + -------------------------------------------------------
  0     0     | DA       SA       BSSID    -      | any management frame; ad-hoc data; station to station
  0     1     | DA       BSSID    SA       -      | data sent BY an AP to a station
  1     0     | BSSID    SA       DA       -      | data sent BY a station to its AP
  1     1     | RA       TA       DA       SA     | AP to AP over the air (wireless bridge, "WDS"): no BSSID
```

Example: a phone (SA) browsing through an AP (BSSID) towards a router (DA): the phone sends `to_ds=1` with Addr1=AP, Addr2=phone, Addr3=router; the AP forwards the answer with `from_ds=1` and Addr1=phone, Addr2=AP, Addr3=router. `addresses()` applies this table and returns an `Addresses` (`ra`, `ta`, `bssid`, `sa`, `da`); a role the frame has none for is empty: a wireless-bridge frame has no BSSID, a short control frame has no TA, and an A-MSDU has no DA nor SA (the real ones sit in its subframes).

- Sequence control (2 bytes, low byte first): bits 0-3 the fragment number, bits 4-15 the sequence number. The sequence number counts the frames of a sender, 0 to 4095, so a retry carries the same number as the original.
- QoS control (2 bytes, data subtypes with bit 3 set): the low 4 bits are the TID, the traffic identifier = the priority queue (voice, video, best effort, background); `qos_tid` holds them. Bit 7 is `amsdu`: the body is a series of aggregated subframes, each with its own header, instead of one LLC/SNAP payload. They are not split here, `parse_llc_snap()` on such a body is wrong.
- `subtype` is the raw 4-bit value: a `ManagementSubtype` or a `ControlSubtype` by `type` (`management_subtype()` and `control_subtype()` give the enum, empty for another type or a reserved value). For data, bit 3 set means QoS data and bit 2 set means "null": a frame without payload (a station telling it sleeps or wakes up).
- `duration`: microseconds the air stays reserved after this frame, so other stations keep quiet (the NAV).
- Control frames are short and have no Addr 3: ACK and CTS are 10 bytes (Frame Control, Duration, RA); RTS, PS-Poll, CF-End and block ack (request) have a 16-byte MAC header (... RA, TA). In a PS-Poll the duration field holds the station's AID. A control wrapper is 16 bytes like an RTS but holds the carried frame control and HT control instead of a transmitter address.
- Management subtypes seen on the air: `beacon` (the AP announcing its network, typically 10 times a second), `probe_request` (a station asking "which networks are here?", possibly for one SSID).

A MAC address (`MacAddress`) is 6 bytes written `90:A4:DE:C0:46:11`. Two bits of the first byte carry meaning: bit 0 (`is_group()`) marks a multicast or broadcast address (`ff:ff:ff:ff:ff:ff` = everyone), never a real device; bit 1 (`is_locally_administered()`) marks an address not assigned by the manufacturer. Phones use random ones when probing networks to avoid being tracked, so many such addresses do not mean many devices. The first 3 bytes (`Oui`) name who defined an address, a suite or a vendor selector, handed out by the IEEE: `00:0f:ac` is the IEEE itself, `00:50:f2` is Microsoft.

### Management frame bodies (`management.hpp`)

After the 802.11 header, the body of a management frame is a few fixed fields whose size and order depend on the subtype, then a list of information elements. Each parser takes `Frame::body`, reads the fixed fields and hands back `information_elements`, the rest, for `ies::elements()`. All numbers little-endian. A parser answers `invalid_argument` when the body is shorter than the fixed fields (a probe request has none, its body is only elements).

How a station joins a network, and which frame carries each step:

```
station                                          access point
   | -- probe request (who is there?) ------------> |   body: elements only (SSID wanted, rates ...)
   | <-- ack ---------------------------------------|
   | <-- probe response (here is my network) ------- |   Beacon layout (the AP also sends beacons by itself)
   | -- authentication, sequence 1 ----------------> |   Authentication
   | <-- authentication, sequence 2 ---------------- |
   | -- association request (I want to join) ------> |   AssocRequest
   | <-- association response (status 0, AID 1) ---- |   AssocResponse
   | <== data frames, encrypted after the 4-way handshake (EAPOL) ==> |
   | -- deauthentication / disassociation (leaving) -> |  parse_reason()
```

Every frame sent to a specific station is answered by a control ACK, omitted in the diagram.

```
Beacon and probe response (parse_beacon, parse_probe_response)
+----------------+-----------------+--------------+- - - - - - - - - -
| timestamp      | beacon interval | capability   | elements ...
| 8 B            | 2 B             | 2 B          |
+----------------+-----------------+--------------+- - - - - - - - - -

AssocRequest (parse_assoc_request)
+------------+-----------------+- - - - - - -
| capability | listen interval | elements ...
| 2 B        | 2 B             |
+------------+-----------------+- - - - - - -

ReassocRequest (parse_reassoc_request): AssocRequest plus the MAC address of the AP the station comes from

AssocResponse (parse_assoc_response)
+------------+--------+--------+- - - - - -
| capability | status | AID    | elements ...
| 2 B        | 2 B    | 2 B    |
+------------+--------+--------+- - - - - -

Authentication (parse_authentication)
+-----------+----------+--------+- - - - - -
| algorithm | sequence | status | elements ...
| 2 B       | 2 B      | 2 B    |
+-----------+----------+--------+- - - - - -
```

- Beacon: `timestamp_us` is the AP's own microsecond clock, stations use it to stay in sync; `interval_tu` is the time between two beacons in time units of 1024 microseconds (100 is about 102 ms). The capability bits: bit 0 `ess` (an access point network), bit 1 `ibss` (ad-hoc), bit 4 `privacy` (encryption is needed: with no RSN element in the elements it means the old WEP, see `security_of()`).
- Association request: the station says what it supports; the listen interval is how often a sleeping station wakes up, in beacon intervals.
- Association response: `status` 0 means success; the AID (association identifier) is the number the AP gives the station; the top 2 bits of the field are always set on the air and are masked off by the parser.
- Authentication: `algorithm` 0 open system (no password at this step), 1 shared key (WEP), 3 SAE (WPA3, several rounds). `sequence` numbers the exchange: 1 from the station, 2 from the AP. `status` 0 = ok.
- Action: carries a protocol of its own, told apart by `category` (block ack setup, spectrum management, radio measurement ...). The rest is left raw.
- Reason code of a deauthentication or a disassociation (`parse_reason`, 2 bytes): why the sender cuts the link (1 unspecified, 3 the sender is leaving, 7 data from a station that is not associated ...).

Check `Frame::protected_frame` before `parse_reason()` and `parse_action()`: with protected management frames the body is encrypted and what is read is noise.

### Information elements (`ies.hpp`)

The body of a beacon, probe, association ... ends with a list of information elements: small self-describing records, each one a feature of the network or of the station. Every element is a TLV (type, length, value):

```
+---------+----------+---------------------------+
| id      | length   | data                      |   repeated until the end of the body
| 1 byte  | 1 byte   | `length` bytes (0 to 255) |
+---------+----------+---------------------------+

00 04 6f 6d 75 73          id 0 (SSID), 4 bytes: "omus"
03 01 0b                   id 3 (DS parameter set), 1 byte: channel 11
01 08 82 84 8b 96 0c 12 18 24   id 1 (supported rates), 8 bytes
```

A reader that does not know an id just skips `length` bytes: that is how 802.11 grows without breaking old devices. An element whose `length` runs past the end of the body means the frame is damaged or cut: `elements()` fails then, `readable_elements()` keeps the complete ones (for a capture cut by its snaplen). `security_of()` stays strict: a cut RSN element must not read as an open network.

The ids the library knows, most useful first:

| Id | Name | Meaning |
|---|---|---|
| 0 | SSID | the network name; length 0 = "hidden network" (it answers only to probes naming it). A hidden network may also send as many zero bytes as the real name has characters: `ssid()` returns them as they are |
| 1, 50 | Supported rates, extended | up to 8 rates in the first, the others in the second |
| 3 | DS parameter set | the channel number the AP works on (absent on most 5 GHz networks: `channel()` then reads the HT operation) |
| 48 | RSN | how the network is secured (WPA2/WPA3) |
| 221 | Vendor specific | private extensions: OUI (3 bytes: who) + type (1 byte) + data. WPA1 and WPS use it |
| 5 | TIM | traffic indication map of a beacon |
| 7 | Country | the regulatory domain: allowed channels and power |
| 45/61, 191/192, 255 + extension id | HT, VHT, HE, EHT | the "n", "ac", "ax" and "be" generations, see the next part; `find()` returns any element raw |
| 127 | Extended capabilities | a bitmap of rarely used features, kept raw |

Vendor specific element (`Vendor`, from `vendor_of()`, `find_vendor()`, `vendors()`):

```
+-----------+--------+------------------+
| oui       | type   | data             |   e.g. 00:50:f2 type 1 = WPA, type 2 = WMM (QoS), type 4 = WPS
| 3 bytes   | 1 byte | the rest         |
+-----------+--------+------------------+
```

Who made the device or the firmware often shows in these OUIs. The two defined by the standard bodies here are `ieee_oui` (WPA2/WPA3 ciphers and methods) and `microsoft_oui` (the older WPA1 and WPS elements).

Rates (`Rate`, from `rates()`): each byte of the supported rates is `basic bit (bit 7) | rate in units of 500 kb/s (bits 0-6)`: `0x82` = basic 1 Mb/s, `0x84` = basic 2 Mb/s, `0x8b` = basic 5.5 Mb/s, `0x0c` = 6 Mb/s, `0x6c` = 54 Mb/s. "Basic" rates are the ones every station must support to join the network. A byte like `0xff` is not a speed but a "BSS membership selector" (here: "this network needs HT"), `rates()` leaves those out.

Suites (`Suite`): a cipher or an authentication method is named by 4 bytes, who defines it (oui) and which one (type): `00-0f-ac-04` is the IEEE selector number 4, whose meaning depends on whether it sits in a cipher or an AKM list. `cipher_str()` and `akm_str()` name the usual ones, or answer `unknown`.

RSN element (id 48, `Rsn`, from `rsn()`): how a network is secured. It is also the body of the older WPA1 vendor element (OUI `00:50:f2`, type 1, read by `wpa()`) once those 4 bytes are skipped, only the suites then use the Microsoft OUI.

```
+---------+--------------+------------+-------------+------------+------------+--------------+
| version | group cipher | pairwise   | pairwise    | AKM        | AKM        | capabilities |
| 2 B (1) | suite 4 B    | count 2 B  | suites 4B ea| count 2 B  | suites 4Bea| 2 B, optional|
+---------+--------------+------------+-------------+------------+------------+--------------+
```

- group cipher: encrypts broadcast/multicast traffic, one for the whole network
- pairwise ciphers: encrypt the traffic of each station, the station picks one among those offered
- AKM (authentication and key management): how the keys are agreed

```
cipher type   1 WEP-40   2 TKIP (old, WPA1)   4 CCMP (AES, WPA2)   5 WEP-104   8 GCMP   9 GCMP-256   10 CCMP-256
AKM type      1 802.1X (enterprise: a RADIUS server, "MGT")   2 PSK (one shared password)   8 SAE (WPA3 password)
              18 OWE (encrypted without a password)   3/4/9 the "fast transition" variants of 1/2/8
```

Reading it: WPA2-Personal is "group CCMP, pairwise CCMP, AKM PSK"; WPA3-Personal has AKM SAE; a network offering both PSK and SAE is in "transition mode". When an RSN or a WPA element is repeated, `security_of()` lets the last one decide.

WPS (wifi protected setup, the "push the button" pairing) announces itself in a vendor element (OUI `00:50:f2`, type 4) made of attributes (`wps_attributes()` uninterpreted, `parse_wps()` and `wps()` into a `Wps`). Careful: like the EtherType of a data frame, and unlike the rest of 802.11, the two numbers of an attribute are big endian.

```
+---------------+---------------+--------------------+
| attribute id  | length        | value              |   repeated
| 2 B big-endian| 2 B big-endian| `length` bytes     |
+---------------+---------------+--------------------+
0x104a version (0x10 = WPS 1.0)   0x1044 state (1 not configured, 2 configured)   0x1057 AP setup locked
0x1021 manufacturer   0x1023 model name   0x1024 model number   0x1011 device name
```

`ap_setup_locked`: the AP refuses WPS pairing after too many failures.

### What a device announces (`capabilities.hpp`)

Beacons, probe requests and association requests list what the radio and the network can do: which generation of wifi the device speaks, how many antennas it has, how wide a channel it can use, which regulatory domain the AP follows. `ht_capabilities()`, `vht_capabilities()`, `country()` and `tim()` take the elements of a frame (`ies::elements()`) and answer empty when the frame does not carry the element, `invalid_argument` when the element is shorter than the fields they read (19 bytes for the HT one, 12 for the VHT one, 3 for the country and the TIM). `standards()` never fails.

HT capabilities (802.11n, element id 45, 26 bytes):

```
+---------------+---------------+-----------------------+-----------+-----------+---------+
| HT cap info   | A-MPDU params | supported MCS set     | extended  | beamform. | antenna |
| 2 B           | 1 B           | 16 B                  | 2 B       | 4 B       | 1 B     |
+---------------+---------------+-----------------------+-----------+-----------+---------+
```

The capability info bits: 0 LDPC coding, 1 40 MHz channels, 2-3 power save of the spatial multiplexing (0 static, 1 dynamic, 3 disabled: whether the radio switches off chains to save power), 4 greenfield (no legacy compatibility), 5 and 6 short guard interval at 20 and 40 MHz, 7 transmit STBC, 8-9 receive STBC streams, 11 big A-MSDUs (7935 bytes instead of 3839), 12 DSSS/CCK in 40 MHz, 14 "40 MHz intolerant" (asks neighbours to stay on 20 MHz). The longest A-MPDU the device receives is 2^(13 + exponent) - 1 bytes; the minimum spacing is 0 (no restriction) then 1/4, 1/2, 1, 2, 4, 8 and 16 microseconds between the MPDUs. `rx_spatial_streams` (0 to 4) is the highest group of MCS the device supports, `rx_highest_rate_mbps` the highest receive rate (0 when the device does not say).

VHT capabilities (802.11ac, element id 191, 12 bytes): a 32-bit capability info and the supported MCS set, a receive and a transmit map of 2 bits per spatial stream (0 MCS 0-7, 1 MCS 0-8, 2 MCS 0-9, 3 stream not supported) each followed by a highest rate.

The info bits: 0-1 longest MPDU (3895, 7991 or 11454 bytes, `max_mpdu_length` 0 for the reserved value), 2-3 width set (160 MHz, 160 and 80+80), 4 LDPC, 5 and 6 short guard interval at 80 and 160 MHz, 7 transmit STBC, 8-10 receive STBC streams, 11-12 single-user beamformer / beamformee, 19-20 multi-user beamformer / beamformee, 23-25 A-MPDU length exponent. `rx_spatial_streams` and `tx_spatial_streams` (0 to 8) are the highest stream whose field in the map is not "not supported".

Standards (`standards()`): which generations of wifi the elements announce, from the presence of their capability elements: HT is 802.11n, VHT 802.11ac (5 GHz), HE 802.11ax (wifi 6), EHT 802.11be (wifi 7). HE and EHT travel in an "extension" element (id 255) whose first data byte names the actual element.

Country (element id 7):

```
+-----------+-------------+--------------------------------------+
| country   | environment | triplets: first channel, number of   |   repeated, 3 bytes each
| 2 letters | 1 byte      | channels, maximum power in dBm       |
+-----------+-------------+--------------------------------------+
```

`code` is the ISO 3166 code ("US", "FR" ...); `environment` is ' ' indoor and outdoor, 'O' outdoor only, 'I' indoor only, 'X' a non-country entity (the code is then XX); `channels` lists the allowed ranges, triplets that announce an operating class instead are left out.

TIM (traffic indication map of a beacon, element id 5): which sleeping stations have frames waiting. `dtim_count` is the number of beacons left before the next DTIM beacon, the one followed by the multicast traffic, `dtim_period` the number of beacons between two DTIM beacons, `multicast_buffered` that the AP holds multicast or broadcast frames for after this beacon.

### Data frames: LLC/SNAP, aggregation, encryption (`data.hpp`, `parse_llc_snap`)

A data frame carries the same traffic as an ethernet cable, but 802.11 has no field for "what protocol is this" (the EtherType of ethernet). So the body of an unencrypted data frame begins with a small LLC/SNAP header (logical link control, subnetwork access protocol) that restores it:

```
+------+------+---------+-----------+---------------------+- - - - - - - - - - - - -
| DSAP | SSAP | control | OUI       | EtherType           | payload
| 0xaa | 0xaa | 0x03    | 3 bytes   | 2 bytes, BIG endian | the IP packet, the ARP request ...
+------+------+---------+-----------+---------------------+- - - - - - - - - - - - -
aa aa 03 | 00 00 00 | 08 00 | 45 00 00 54 ...     = SNAP, OUI 00:00:00 (the EtherType is a standard one),
                                                    0x0800 = IPv4, then an IPv4 header (0x45: version 4, 5 words)
```

`Llc::oui` is `00:00:00` for a plain EtherType, `00:00:f8` for the old "bridge tunnel" encapsulation. Usual EtherTypes: `0x0800` IPv4, `0x0806` ARP, `0x86dd` IPv6, `0x8100` VLAN tag, `0x888e` EAPOL (802.1X: the "4-way handshake" that sets up a WPA2 key travels in EAPOL frames, before encryption starts). `parse_llc_snap()` answers `not_supported` when the header is not SNAP.

Aggregation: a QoS data frame with `Frame::amsdu` set carries several packets glued together (an A-MSDU): each subframe is a destination address, a source address, a length and then its own LLC/SNAP header. This library does not split them, `parse_llc_snap()` on such a body is wrong.

Encryption: when `Frame::protected_frame` is set, what follows the 802.11 header is not an LLC header:

```
+--------------+--------------------------------------+---------+
| CCMP header  | ENCRYPTED: LLC/SNAP header + payload | MIC     |     WPA2 (CCMP/AES), 8 + 8 bytes added
| 8 bytes      |                                      | 8 bytes |     (TKIP, WPA1: 8-byte IV header, then MIC+ICV)
+--------------+--------------------------------------+---------+
```

Without the key nothing can be read, and this library never decrypts. Be careful with captures made on the device itself (a phone ...): the driver may hand them over already decrypted while the flag stays set. The capture `ieee802.11_rx-stbc.pcap` of the tcpdump test suite is one: after the 8 bytes of CCMP header the LLC/SNAP header is readable. `parse_llc_snap()` on `Frame::body` as is then answers `not_supported`, on the body minus 8 bytes it works.

### The whole pipeline (`dissect.hpp`)

One captured packet, from the card to the content (each arrow is a function of this library):

```
pcap record (one packet, pcap::Sniffer::read_next() or pcap::PcapReader::read_next())
+-----------------+------------------------------------------------------------+-------+
| radiotap header | 802.11 frame                                               | FCS   |
+-----------------+------------------------------------------------------------+-------+
       |                  |
       |   parse_radiotap |   parse_frame -> Frame (type, flags, addresses ...) + `body`
       v                  v
     Radiotap       +-------------+----------------------------------------------------+
(signal, channel)   | 802.11 hdr  | body                                               |
                    +-------------+----------------------------------------------------+
                                       |
     management frame: parse_beacon / parse_assoc_request / ...  -> fixed fields + elements -> ies::*
     data frame:       parse_llc_snap -> EtherType + payload (the IP packet)        (when not protected)
     control frame:    ACK, CTS and RTS have no body; block ack ones carry fields left raw
```

The pcap file tells how the packets begin, its "datalink":

| Datalink | Name | Content | Function |
|---|---|---|---|
| 127 | `DLT_IEEE802_11_RADIO` | radiotap header then 802.11 frame (monitor mode captures) | `dissect_radiotap()`: drops the FCS, skips the padding radiotap announces |
| 105 | `DLT_IEEE802_11` | the 802.11 frame alone, no radiotap | `dissect_80211()` |
| 1 | `DLT_EN10MB` | ethernet: the driver already removed the 802.11 layer | nothing here applies |

A capture taken on a phone or a laptop in normal (not monitor) mode is usually datalink 1. `Packet::radiotap` is absent for `dissect_80211()`.

### Device signatures (`taxonomy.hpp`)

Two devices of the same model run the same firmware, so the information elements of their probe requests and association requests come in the same order with the same capabilities, whatever their MAC address, name or owner. A signature writes those elements down as one line of text, the format of hostapd's "taxonomy" and of Google's wifi device taxonomy:

```
wifi4|probe:0,1,50,127,107,221(0050f2,4),221(506f9a,9),extcap:00000080,wps:5042T|assoc:0,1,50,45,48,127,221(0050f2,2),htcap:1172,htagg:03,htmcs:000000ff,extcap:01
```

- each half lists the ids of the elements in the order they were sent; a vendor element (221) shows its OUI and type in parentheses. `probe:` comes from the probe request, `assoc:` from the association request.
- after the ids come the details that tell models apart, always in the same order: `htcap` (HT capability info), `htagg` (A-MPDU parameters), `htmcs` (supported MCS bitmap, low 16 bits), `vhtcap`, `vhtrxmcs`, `vhttxmcs` (VHT capability info and MCS maps), `txpow` (power capability), `extcap` (extended capabilities bytes) and `wps` (the model name of a WPS element, anything but letters and digits replaced by `_`). When an id is repeated, the last element gives the details.
- `element_signature()` writes one half (a damaged element ends the list), `signature()` the whole line, `invalid_argument` when one of the two halves is empty. The leading `wifi4` is the fixed tag of the format, whatever generation of wifi the device speaks.

The library only writes the line. The list of known models, which maps a signature to a device name, is a file of its own that the application supplies and compares with the lines it computes; none is embedded here.

Mind the privacy side: devices randomize their MAC address to be harder to follow, and a signature does not change with it. It says which model it is, not which device, but it narrows a crowd down.

## Vocabulary

| Word | Meaning |
|---|---|
| STA | a station: any device with a wifi radio |
| AP | access point: creates a network and relays between the air and the wired side |
| SSID | the network name people see, 0 to 32 bytes; empty in a hidden network |
| BSSID | identity of one AP radio = its MAC address; two radios, or two networks on one radio, give two BSSIDs |
| BSS / ESS | one AP with its stations / several APs sharing one SSID |
| IBSS | ad-hoc network without AP |
| DS | distribution system, the wired backbone behind the APs; `to_ds` / `from_ds` tell a frame goes to / comes from it |
| RA / TA | receiver / transmitter address: the next and the previous hop on the air |
| DA / SA | destination / source address: the real endpoints |
| MAC address | 6 bytes; bit 0 of byte 0 = group (multicast/broadcast), bit 1 = locally administered (random, not from the manufacturer) |
| TU | time unit, 1024 microseconds |
| FCS | frame check sequence, a 4-byte CRC at the end of the frame |
| Frame control | the first 2 bytes of every frame: version, type, subtype and 8 flags |
| Management / control / data | the three frame families: network life / channel access / payload |
| Beacon | the AP's periodic announcement, about every 100 ms |
| Probe request / response | a station looking for networks / an AP answering it |
| Authentication, association | the two steps of joining a network (authentication is a formality with open system or WPA2; association assigns the AID) |
| Deauthentication, disassociation | leaving the network, with a reason code |
| ACK | control frame: the previous frame arrived |
| RTS / CTS | control frames reserving the air before a long transmission |
| NAV | the duration field: how long the other stations must stay quiet |
| QoS, TID | priority queues of data frames; the traffic identifier names the queue |
| HT / VHT / HE | the 802.11n / ac / ax generations ("high throughput", "very high throughput", "high efficiency") |
| MCS | a modulation and coding index that replaces the plain rate in HT, VHT and HE |
| IE | information element: the id, length, data record at the end of a management body |
| OUI | 3 bytes naming a vendor or a standards body (`00:0f:ac` = IEEE, `00:50:f2` = Microsoft) |
| RSN | "robust security network": the element describing WPA2 / WPA3 |
| WPA, WPA2, WPA3 | security generations: WPA1 (TKIP, vendor element), WPA2 (RSN, CCMP, PSK), WPA3 (RSN, SAE) |
| CCMP, TKIP, GCMP | encryption algorithms: AES-based (WPA2), the old RC4-based one (WPA1), the newer AES-GCM |
| PSK, SAE, 802.1X, OWE | how keys are agreed: a shared password, WPA3's password exchange, an authentication server (enterprise), encryption without password |
| WEP | the broken first-generation encryption; shows as `privacy` with no RSN element |
| EAPOL | the frames carrying the key exchange (the "4-way handshake"), EtherType `0x888e` |
| LLC / SNAP | the 8-byte header in front of a data payload that says which protocol it is |
| EtherType | 2 bytes naming the protocol of a payload: `0x0800` IPv4, `0x0806` ARP, `0x86dd` IPv6 |
| Radiotap | the capture-time header: signal, noise, channel, rate, timestamp |
| dBm | power level; -30 is very strong, -90 barely audible |
| TSFT | the card's microsecond timer stamped on each frame |
| Monitor mode | the card passes every frame it hears to the capture, without belonging to a network |
| Guard interval | the pause between two symbols; the short one (400 ns instead of 800 ns) raises the rate by 10/9 |
| LDPC, STBC | an error-correcting code, and sending the same data on several antennas to be easier to receive |
| A-MSDU, A-MPDU | gluing several packets into one frame, or several frames into one transmission, to save airtime |
| Beamforming | focusing the radio signal towards the receiver; SU for one receiver, MU for several at once |
| Spatial stream | one independent data flow over its own antenna chain; 2x2 means two chains, two streams |
| DTIM | the beacons after which the AP sends its buffered multicast traffic |
| Signature | a one-line text of the elements of a device's requests, which identifies its model (see `taxonomy.hpp`) |

## What the decoders do not do

- **No decryption** of protected frames, no key handling, no check of the FCS (`bad_fcs` only repeats what the card reported).
- **No reassembly**: fragments and aggregated frames are shown as the card delivered them. A frame with `amsdu` set holds several packets glued together that are not split.
- **No 60 GHz or S1G (extension) frames**: `parse_frame()` answers `not_supported`.
- **VHT and HE rates are not computed**: the radiotap MCS field of 802.11n is decoded into `Radiotap::mcs` with its rate; the VHT and HE radiotap fields are skipped, so such a frame has no rate. The HE and EHT elements are only detected (`ies::standards()`), their content stays raw.
- **Control frame bodies, action frames and the HT control field are raw.**
- **Radiotap fields it cannot size stop the walk**: what was read before stays, and the frame is still reachable through `header_size`.
- **A snaplen-cut capture loses its FCS guess**: `fcs_at_end` drops the last 4 bytes, which belong to the frame when the capture was cut. `ies::readable_elements()` keeps the elements that are complete in a cut frame, `ies::elements()` refuses it.
- **No channel number from a frequency**, no MAC-to-manufacturer database and no device database: the application brings them. `taxonomy::signature()` only writes the line to look up.

## How the decoders were checked

- Unit tests build frames byte by byte (`modules/wifi/test`), including real packets taken from the tcpdump 4.99.6 test suite (BSD licensed): a probe request with an extended radiotap header, an ACK, a probe response, a protected QoS data frame sent with MCS 7, two null data frames sent with MCS 2 and MCS 11. The expected values were read from `tcpdump -e -nn`. Truncating each of these frames at every length gives either a result or an error, never a crash (the tests also run under AddressSanitizer).
- A one-off comparison with tcpdump over the radiotap captures of its test suite (some deliberately malformed): every packet decodes without crashing, and frequency, signal, noise, antenna, TSFT, addresses, SSID, channel and, since the MCS field is decoded, the rates agree with tcpdump's own reading. The suite is not vendored here.
- The HT and VHT capability decoders were checked against the masks of the Linux kernel headers (`ieee80211-ht.h`, `ieee80211-vht.h`) and by decoding an HT element and a VHT element by hand.
- `taxonomy::element_signature()` was compared with the original C function of hostapd's taxonomy (the one wifimapper compiled) on 124 elements lists of real management frames and 30000 random ones: the output was identical byte for byte. A test also rebuilds a full entry of Google's wifi taxonomy from the elements that produce it.
