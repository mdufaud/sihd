import threading
import time

import sihd

bt = sihd.bt
Property = bt.GattCharacteristic.Property

unreachable = "org.freedesktop.DBus.Error.FileNotFound: Failed to connect to socket /nonexistent/sihd_py_bt_socket: No such file or directory"


def raised(fn, *args):
    try:
        fn(*args)
    except sihd.Error as error:
        return error
    assert False, "expected an error"


def raises_unreachable(fn, *args):
    error = raised(fn, *args)
    assert error.code == sihd.ErrorCode.io_error
    assert str(error) == unreachable


assert bt.AddressType.public_address != bt.AddressType.random
assert bt.AddressType.random != bt.AddressType.unspecified
assert bt.Transport.any != bt.Transport.le
assert bt.Transport.le != bt.Transport.classic
assert bt.WriteType.with_response != bt.WriteType.without_response
assert bt.PairingRequest.Kind.confirm != bt.PairingRequest.Kind.enter_pin
assert bt.Device.Event.connected != bt.Device.Event.disconnected
assert bt.Device.Event.disconnected != bt.Device.Event.notification
assert bt.Scanner.Event.found != bt.Scanner.Event.updated
assert bt.Scanner.Event.updated != bt.Scanner.Event.lost
assert hasattr(bt, "PairingRequestKind") == False
assert hasattr(bt, "DeviceEvent") == False
assert hasattr(bt, "ScanEvent") == False
assert [int(prop) for prop in (Property.broadcast, Property.read, Property.write_without_response, Property.write,
                               Property.notify, Property.indicate, Property.signed_write,
                               Property.extended_properties)] == [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80]

adapter = bt.AdapterInfo()
assert adapter.identifier == "" and adapter.address == "" and adapter.name == "" and adapter.alias == ""
assert not adapter.powered and not adapter.discoverable and not adapter.pairable and not adapter.discovering

scan_filter = bt.ScanFilter()
assert scan_filter.transport == bt.Transport.any
assert scan_filter.uuids == [] and scan_filter.min_rssi is None
scan_filter.transport = bt.Transport.le
scan_filter.uuids = ["180f", "180a"]
scan_filter.min_rssi = -70
assert scan_filter.transport == bt.Transport.le
assert scan_filter.uuids == ["180f", "180a"]
assert scan_filter.min_rssi == -70
scan_filter.min_rssi = None
assert scan_filter.min_rssi is None

scan_result = bt.ScanResult()
assert scan_result.identifier == "" and scan_result.address == ""
assert scan_result.address_type == bt.AddressType.unspecified
assert scan_result.rssi == 0 and scan_result.tx_power is None and scan_result.connectable == False
assert scan_result.manufacturer_data == {} and scan_result.service_data == {}
assert scan_result.uuids == [] and scan_result.appearance is None and scan_result.class_of_device is None
assert isinstance(scan_result.timestamp, sihd.util.Timestamp)

descriptor = bt.GattDescriptor()
assert descriptor.uuid == "" and descriptor.handle == 0
descriptor.uuid = "2902"
descriptor.handle = 7
characteristic = bt.GattCharacteristic()
assert characteristic.uuid == "" and characteristic.handle == 0 and characteristic.properties == 0
assert characteristic.descriptors == []
characteristic.uuid = "2a19"
characteristic.handle = 6
characteristic.properties = Property.read | Property.notify
characteristic.descriptors = [descriptor]
assert characteristic.properties == 0x12
assert characteristic.properties & Property.notify
assert not characteristic.properties & Property.write
assert len(characteristic.descriptors) == 1
assert characteristic.descriptors[0].uuid == "2902" and characteristic.descriptors[0].handle == 7

service = bt.GattService()
assert service.uuid == "" and service.handle == 0 and service.primary == True and service.characteristics == []
service.characteristics = [characteristic]
assert service.characteristics[0].uuid == "2a19"

notification = bt.GattNotification()
assert notification.handle == 0 and notification.uuid == ""
assert notification.value == b"" and type(notification.value) is bytes
assert isinstance(notification.timestamp, sihd.util.Timestamp)

device_info = bt.DeviceInfo()
assert device_info.address == "" and device_info.name == ""
assert device_info.address_type == bt.AddressType.unspecified
assert not device_info.paired and not device_info.connected and not device_info.trusted
assert device_info.appearance is None and device_info.class_of_device is None and device_info.uuids == []

request = bt.PairingRequest()
assert request.address == "" and request.kind == bt.PairingRequest.Kind.confirm
assert request.passkey == 0 and request.pin == ""
answer = bt.PairingAnswer()
assert answer.accept == False and answer.passkey == 0 and answer.pin == ""
answer.accept = True
answer.passkey = 123456
answer.pin = "0000"
assert answer.accept and answer.passkey == 123456 and answer.pin == "0000"

advertisement = bt.Advertisement()
assert advertisement.local_name == "" and advertisement.service_uuids == []
assert advertisement.manufacturer_data == {} and advertisement.service_data == {}
assert advertisement.connectable == True
advertisement.local_name = "sensor"
advertisement.service_uuids = ["180f"]
# 0x004c: the Apple company id; 180f: the battery service, 0x64 a 100% level
advertisement.manufacturer_data = {0x004c: b"\x01\x02\x00"}
advertisement.service_data = {"180f": b"\x64"}
advertisement.connectable = False
assert advertisement.local_name == "sensor" and advertisement.service_uuids == ["180f"]
assert advertisement.manufacturer_data == {0x004c: b"\x01\x02\x00"}
assert type(advertisement.manufacturer_data[0x004c]) is bytes
assert advertisement.service_data == {"180f": b"\x64"}
assert advertisement.connectable == False

local_characteristic = bt.LocalCharacteristic()
assert local_characteristic.uuid == "" and local_characteristic.properties == 0
local_characteristic.uuid = "2a19"
local_characteristic.properties = Property.read | Property.write
local_characteristic.on_read = lambda: b"\x64"
local_characteristic.on_write = lambda value: True
local_characteristic.on_read = None
local_characteristic.on_write = None
local_service = bt.LocalService()
assert local_service.uuid == "" and local_service.primary == True and local_service.characteristics == []
local_service.uuid = "180f"
local_service.characteristics = [local_characteristic]
assert len(local_service.characteristics) == 1 and local_service.characteristics[0].uuid == "2a19"
assert local_service.characteristics[0].properties == 0x0a

raises_unreachable(bt.bluetooth_enabled)
raises_unreachable(bt.list_adapters)
raises_unreachable(bt.set_adapter_powered, 0, True)
raises_unreachable(bt.set_adapter_discoverable, 0, True)
raises_unreachable(bt.set_adapter_pairable, 0, False)
raises_unreachable(bt.set_adapter_alias, 0, "sihd")

device = bt.Device("device")
assert device.set_address("bad") == False
assert device.set_address("aa:bb:cc:dd:ee:ff") == True
assert device.set_adapter(1) == True
assert device.set_conf(address="11:22:33:44:55:66", adapter=0) == True
assert device.last_event() == bt.Device.Event.disconnected
assert device.last_notification() is None
raises_unreachable(device.info)
raises_unreachable(device.pair)
raises_unreachable(device.cancel_pairing)
raises_unreachable(device.unpair)
raises_unreachable(device.set_trusted, True)

closed = raised(device.services)
assert closed.code == sihd.ErrorCode.closed and str(closed) == "bluetooth device is not connected"
closed = raised(device.mtu)
assert closed.code == sihd.ErrorCode.closed and str(closed) == "bluetooth device is not connected"
for call in (lambda: device.read_characteristic(characteristic),
             lambda: device.write_characteristic(characteristic, b"\x01", bt.WriteType.with_response),
             lambda: device.read_descriptor(descriptor),
             lambda: device.write_descriptor(descriptor, b"\x01\x00"),
             lambda: device.subscribe(characteristic),
             lambda: device.unsubscribe(characteristic)):
    unlisted = raised(call)
    assert unlisted.code == sihd.ErrorCode.not_found
assert str(raised(device.read_characteristic, characteristic)) == "no gatt attribute of handle 6, services() lists them"
assert str(raised(device.read_descriptor, descriptor)) == "no gatt attribute of handle 7, services() lists them"

device.set_pairing_handler(lambda pairing_request: answer)
device.set_pairing_handler(None)
try:
    device.set_pairing_handler("not callable")
    assert False
except TypeError:
    pass
device.add_observer(lambda observed: None)
device.add_observer(lambda observed: None)
device.remove_observers()
device.remove_observers()

scanner = bt.Scanner("scanner")
assert scanner.set_adapter(0) == True
assert scanner.set_filter(scan_filter) == True
assert scanner.set_conf(adapter=0) == True
assert scanner.set_lost_timeout(sihd.util.time.ms(500)) == True
assert scanner.set_lost_timeout(-1) == False
assert scanner.results() == {} and scanner.last_result() is None
assert scanner.last_event() == bt.Scanner.Event.found
assert scanner.scan_for(sihd.util.time.ms(50)) == False
scanner.add_observer(lambda observed: None)
scanner.remove_observers()

server = bt.GattServer("server")
assert server.set_adapter(0) == True
assert server.add_service(local_service) == True
closed = raised(server.notify, "180f", "2a19", b"\x64")
assert closed.code == sihd.ErrorCode.closed and str(closed) == "gatt server is not started"

advertiser = bt.Advertiser("advertiser")
assert advertiser.set_adapter(0) == True
assert advertiser.set_advertisement(advertisement) == True

for service_object in (device, scanner, server, advertiser):
    assert service_object.start() == False
    assert service_object.is_running() == False
    assert service_object.is_ready() == False
    service_object.set_service_wait_stop(True)

root = sihd.util.Node("bt_root")
child = bt.Scanner("child", root)
assert child.name() == "child" and child.parent().name() == "bt_root"

ticks = []
stopped = threading.Event()


def tick():
    while not stopped.is_set():
        ticks.append(1)
        time.sleep(0.005)


ticker = threading.Thread(target=tick)
ticker.start()
while not ticks:
    time.sleep(0.001)
ticks_before = len(ticks)
assert scanner.wait_ready(sihd.util.time.ms(300)) == False
ticks_after = len(ticks)
stopped.set()
ticker.join()
assert ticks_after - ticks_before >= 5

socket = bt.Socket()
assert socket.is_open() == False and socket.is_listening() == False
assert socket.address() == "" and socket.psm() == 0
closed = raised(socket.send, b"early")
assert closed.code == sihd.ErrorCode.closed
assert str(closed) == "bluetooth socket is closed"
not_listening = raised(socket.accept, sihd.util.time.ms(10))
assert not_listening.code == sihd.ErrorCode.closed
assert str(not_listening) == "bluetooth socket is closed"
bad_channel = raised(socket.connect_rfcomm_channel, "AA:BB:CC:DD:EE", 1)
assert bad_channel.code == sihd.ErrorCode.invalid_argument
assert str(bad_channel) == "invalid bluetooth address: AA:BB:CC:DD:EE"
bad_constructor = raised(bt.Socket, "AA:BB:CC:DD:EE", 1)
assert bad_constructor.code == sihd.ErrorCode.invalid_argument
assert str(bad_constructor) == "invalid bluetooth address: AA:BB:CC:DD:EE"
# LE psms end at 0x00ff: the kernel refuses it, or has no bluetooth socket at all
raised(bt.Socket, 0x0100)
