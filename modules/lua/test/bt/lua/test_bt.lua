local bt = sihd.bt
local Property = bt.GattCharacteristic.Property
local is_windows = package.config:sub(1, 1) == "\\"
local unreachable = "org.freedesktop.DBus.Error.FileNotFound: Failed to connect to socket /nonexistent/sihd_lua_bt_socket: No such file or directory"

local function must_raise(fn, ...)
    local ok, err = pcall(fn, ...)
    assert(ok == false, "expected an error, got success")
    return err
end

local function empty(tbl)
    return next(tbl) == nil
end

assert(bt.AddressType.public_address ~= bt.AddressType.random)
assert(bt.AddressType.random ~= bt.AddressType.unspecified)
assert(bt.Transport.any ~= bt.Transport.le)
assert(bt.Transport.le ~= bt.Transport.classic)
assert(bt.WriteType.with_response ~= bt.WriteType.without_response)
assert(bt.PairingRequest.Kind.confirm ~= bt.PairingRequest.Kind.enter_pin)
assert(bt.Device.Event.connected ~= bt.Device.Event.disconnected)
assert(bt.Device.Event.disconnected ~= bt.Device.Event.notification)
assert(bt.Scanner.Event.found ~= bt.Scanner.Event.updated)
assert(bt.Scanner.Event.updated ~= bt.Scanner.Event.lost)
assert(bt.PairingRequestKind == nil)
assert(bt.DeviceEvent == nil)
assert(bt.ScanEvent == nil)
assert(Property.broadcast == 0x01 and Property.read == 0x02 and Property.write_without_response == 0x04)
assert(Property.write == 0x08 and Property.notify == 0x10 and Property.indicate == 0x20)
assert(Property.signed_write == 0x40 and Property.extended_properties == 0x80)

local adapter = bt.AdapterInfo()
assert(adapter.identifier == "" and adapter.address == "" and adapter.name == "" and adapter.alias == "")
assert(adapter.powered == false and adapter.discoverable == false)
assert(adapter.pairable == false and adapter.discovering == false)

local scan_filter = bt.ScanFilter()
assert(scan_filter.transport == bt.Transport.any)
assert(empty(scan_filter.uuids) and scan_filter.min_rssi == nil)
scan_filter.transport = bt.Transport.le
scan_filter.uuids = {"180f", "180a"}
scan_filter.min_rssi = -70
assert(scan_filter.transport == bt.Transport.le)
assert(#scan_filter.uuids == 2 and scan_filter.uuids[1] == "180f" and scan_filter.uuids[2] == "180a")
assert(scan_filter.min_rssi == -70)
scan_filter.min_rssi = nil
assert(scan_filter.min_rssi == nil)

local scan_result = bt.ScanResult()
assert(scan_result.identifier == "" and scan_result.address == "")
assert(scan_result.address_type == bt.AddressType.unspecified)
assert(scan_result.rssi == 0 and scan_result.tx_power == nil and scan_result.connectable == false)
assert(empty(scan_result.manufacturer_data) and empty(scan_result.service_data))
assert(empty(scan_result.uuids) and scan_result.appearance == nil and scan_result.class_of_device == nil)
assert(scan_result.timestamp:nanoseconds() == 0)

local descriptor = bt.GattDescriptor()
assert(descriptor.uuid == "" and descriptor.handle == 0)
descriptor.uuid = "2902"
descriptor.handle = 7
local characteristic = bt.GattCharacteristic()
assert(characteristic.uuid == "" and characteristic.handle == 0 and characteristic.properties == 0)
assert(empty(characteristic.descriptors))
characteristic.uuid = "2a19"
characteristic.handle = 6
characteristic.properties = Property.read | Property.notify
characteristic.descriptors = {descriptor}
assert(characteristic.properties == 0x12)
assert(characteristic.properties & Property.notify ~= 0)
assert(characteristic.properties & Property.write == 0)
assert(#characteristic.descriptors == 1)
assert(characteristic.descriptors[1].uuid == "2902" and characteristic.descriptors[1].handle == 7)

local service = bt.GattService()
assert(service.uuid == "" and service.handle == 0 and service.primary == true and empty(service.characteristics))
service.characteristics = {characteristic}
assert(service.characteristics[1].uuid == "2a19")

local notification = bt.GattNotification()
assert(notification.handle == 0 and notification.uuid == "" and notification.value == "")
assert(notification.timestamp:nanoseconds() == 0)

local device_info = bt.DeviceInfo()
assert(device_info.address == "" and device_info.name == "")
assert(device_info.address_type == bt.AddressType.unspecified)
assert(device_info.paired == false and device_info.connected == false and device_info.trusted == false)
assert(device_info.appearance == nil and device_info.class_of_device == nil and empty(device_info.uuids))

local request = bt.PairingRequest()
assert(request.address == "" and request.kind == bt.PairingRequest.Kind.confirm)
assert(request.passkey == 0 and request.pin == "")
local answer = bt.PairingAnswer()
assert(answer.accept == false and answer.passkey == 0 and answer.pin == "")
answer.accept = true
answer.passkey = 123456
answer.pin = "0000"
assert(answer.accept == true and answer.passkey == 123456 and answer.pin == "0000")

local advertisement = bt.Advertisement()
assert(advertisement.local_name == "" and empty(advertisement.service_uuids))
assert(empty(advertisement.manufacturer_data) and empty(advertisement.service_data))
assert(advertisement.connectable == true)
advertisement.local_name = "sensor"
advertisement.service_uuids = {"180f"}
-- 0x004c: the Apple company id; 180f: the battery service, "\100" a 100% level
advertisement.manufacturer_data = {[0x004c] = "\1\2\0"}
advertisement.service_data = {["180f"] = "\100"}
advertisement.connectable = false
assert(advertisement.local_name == "sensor" and advertisement.service_uuids[1] == "180f")
assert(advertisement.manufacturer_data[0x004c] == "\1\2\0")
assert(advertisement.service_data["180f"] == "\100")
assert(advertisement.connectable == false)

local local_characteristic = bt.LocalCharacteristic()
assert(local_characteristic.uuid == "" and local_characteristic.properties == 0)
local_characteristic.uuid = "2a19"
local_characteristic.properties = Property.read | Property.write
local_characteristic:set_on_read(function() return "\100" end)
local_characteristic:set_on_write(function(value) return true end)
local_characteristic:set_on_read(nil)
local_characteristic:set_on_write(nil)
local bad_reader = must_raise(local_characteristic.set_on_read, local_characteristic, "not a function")
assert(bad_reader.code == sihd.ErrorCode.invalid_argument)
assert(bad_reader.message == "set_on_read: expected a function or nil")
local bad_writer = must_raise(local_characteristic.set_on_write, local_characteristic, 5)
assert(bad_writer.code == sihd.ErrorCode.invalid_argument)
assert(bad_writer.message == "set_on_write: expected a function or nil")
local local_service = bt.LocalService()
assert(local_service.uuid == "" and local_service.primary == true and empty(local_service.characteristics))
local_service.uuid = "180f"
local_service.characteristics = {local_characteristic}
assert(#local_service.characteristics == 1 and local_service.characteristics[1].uuid == "2a19")
assert(local_service.characteristics[1].properties == 0x0a)

local device = bt.Device("device", nil)
assert(device:set_address("bad") == false)
assert(device:set_address("aa:bb:cc:dd:ee:ff") == true)
assert(device:set_adapter(1) == true)
assert(device:set_conf({address = "11:22:33:44:55:66", adapter = 0}) == true)
assert(device:last_event() == bt.Device.Event.disconnected)
assert(device:last_notification() == nil)

if not is_windows then
    local not_connected = must_raise(device.services, device)
    assert(not_connected.code == sihd.ErrorCode.closed)
    assert(not_connected.message == "bluetooth device is not connected")
    assert(must_raise(device.mtu, device).message == "bluetooth device is not connected")
    for _, call in ipairs({
        function() device:read_characteristic(characteristic) end,
        function() device:write_characteristic(characteristic, "\1", bt.WriteType.with_response) end,
        function() device:subscribe(characteristic) end,
        function() device:unsubscribe(characteristic) end,
    }) do
        local unlisted = must_raise(call)
        assert(unlisted.code == sihd.ErrorCode.not_found)
        assert(unlisted.message == "no gatt attribute of handle 6, services() lists them")
    end
    for _, call in ipairs({
        function() device:read_descriptor(descriptor) end,
        function() device:write_descriptor(descriptor, "\1\0") end,
    }) do
        local unlisted = must_raise(call)
        assert(unlisted.code == sihd.ErrorCode.not_found)
        assert(unlisted.message == "no gatt attribute of handle 7, services() lists them")
    end
end

device:set_pairing_handler(function(pairing_request) return answer end)
device:set_pairing_handler(nil)
local bad_handler = must_raise(device.set_pairing_handler, device, 5)
assert(bad_handler.code == sihd.ErrorCode.invalid_argument)
assert(bad_handler.message == "set_pairing_handler: expected a function or nil")
device:add_observer(function(observed) end)
device:add_observer(function(observed) end)
device:remove_observers()
device:remove_observers()
local bad_observer = must_raise(device.add_observer, device, "not a function")
assert(bad_observer.code == sihd.ErrorCode.invalid_argument)
assert(bad_observer.message == "add_observer: expected a function")

local scanner = bt.Scanner("scanner", nil)
assert(scanner:set_adapter(0) == true)
assert(scanner:set_filter(scan_filter) == true)
assert(scanner:set_conf({adapter = 0}) == true)
assert(scanner:set_lost_timeout(sihd.util.time.ms(500)) == true)
assert(scanner:set_lost_timeout(-1) == false)
assert(empty(scanner:results()) and scanner:last_result() == nil)
assert(scanner:last_event() == bt.Scanner.Event.found)
scanner:add_observer(function(observed) end)
scanner:remove_observers()

local server = bt.GattServer("server", nil)
assert(server:set_adapter(0) == true)
assert(server:add_service(local_service) == true)
local not_started = must_raise(server.notify, server, "180f", "2a19", "\100")
assert(not_started.code == sihd.ErrorCode.closed)
assert(not_started.message == "gatt server is not started")

local advertiser = bt.Advertiser("advertiser", nil)
assert(advertiser:set_adapter(0) == true)
assert(advertiser:set_advertisement(advertisement) == true)

for _, service_object in ipairs({device, scanner, server, advertiser}) do
    assert(service_object:is_running() == false)
    assert(service_object:is_ready() == false)
    service_object:set_service_wait_stop(true)
    assert(service_object:wait_ready(sihd.util.time.ms(1)) == false)
    assert(service_object:wait_ready(1000000) == false)
    local bad_timeout = must_raise(service_object.wait_ready, service_object, "soon")
    assert(bad_timeout.code == sihd.ErrorCode.invalid_argument)
    assert(bad_timeout.message == "a timeout is a number of nanoseconds or a Duration")
end
local bad_duration = must_raise(scanner.scan_for, scanner, true)
assert(bad_duration.code == sihd.ErrorCode.invalid_argument)
assert(bad_duration.message == "a timeout is a number of nanoseconds or a Duration")
local bad_lost_timeout = must_raise(scanner.set_lost_timeout, scanner, "soon")
assert(bad_lost_timeout.code == sihd.ErrorCode.invalid_argument)
assert(bad_lost_timeout.message == "a timeout is a number of nanoseconds or a Duration")

local root = sihd.util.Node("bt_root", nil)
local child = bt.Scanner("child", root)
assert(child:name() == "child" and child:parent():name() == "bt_root")

if not is_windows then
    local function raises_unreachable(fn, ...)
        local err = must_raise(fn, ...)
        assert(err.code == sihd.ErrorCode.io_error)
        assert(err.message == unreachable)
    end

    raises_unreachable(bt.bluetooth_enabled)
    raises_unreachable(bt.list_adapters)
    raises_unreachable(bt.set_adapter_powered, 0, true)
    raises_unreachable(bt.set_adapter_discoverable, 0, true)
    raises_unreachable(bt.set_adapter_pairable, 0, false)
    raises_unreachable(bt.set_adapter_alias, 0, "sihd")
    raises_unreachable(device.info, device)
    raises_unreachable(device.pair, device)
    raises_unreachable(device.cancel_pairing, device)
    raises_unreachable(device.unpair, device)
    raises_unreachable(device.set_trusted, device, true)
    for _, service_object in ipairs({device, scanner, server, advertiser}) do
        assert(service_object:start() == false)
    end
    assert(scanner:scan_for(sihd.util.time.ms(50)) == false)
end

local socket = bt.Socket()
assert(socket:is_open() == false and socket:is_listening() == false)
assert(socket:address() == "" and socket:psm() == 0)
local closed = must_raise(socket.send, socket, "early")
assert(closed.code == sihd.ErrorCode.closed)
assert(closed.message == "bluetooth socket is closed")
local not_listening = must_raise(socket.accept, socket, sihd.util.time.ms(10))
assert(not_listening.code == sihd.ErrorCode.closed)
assert(not_listening.message == "bluetooth socket is closed")
local bad_channel = must_raise(socket.connect_rfcomm_channel, socket, "AA:BB:CC:DD:EE", 1)
assert(bad_channel.code == sihd.ErrorCode.invalid_argument)
