#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/bt.hpp>
#include <sihd/net/OuiRegistry.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::bt;
namespace fs = sihd::sys::fs;

namespace
{

constexpr const char *help = R"(options:
  --assigned-numbers <dir>         the assigned_numbers folder of the bluetooth sig repository
  --oui <file>                     an IEEE oui.txt, the vendor of a public address
commands:
  adapters                         list the adapters
  power on|off                     power the first adapter
  scan [seconds] [any|le|classic]  discover devices
  info <address>                   what the system knows of a device
  pair <address>                   pair, answering the questions here (android: the system asks)
  unpair <address>                 forget a paired device
  connect <address>                connect a device in the background
  services                         list the gatt services of the connected device
  read <handle>                    read a characteristic
  write <handle> <hex> [command]   write a characteristic, command skips the response
  notify <handle>                  subscribe to a characteristic
  unnotify <handle>                unsubscribe
  mtu                              the mtu of the connected device
  disconnect                       disconnect the device
  rfcomm <address> <uuid|channel>  open an rfcomm stream
  l2cap <address> <public|random> <psm>  open an L2CAP LE channel
  listen <uuid> [name]             accept one rfcomm stream
  listen-l2cap [psm]               accept one L2CAP LE channel
  send <text>                      send a line on the open stream
  close                            close the stream
  advertise [name]                 advertise this device (android: no name)
  serve                            publish the echo gatt service
  stop                             stop advertising and serving
  quit)";

// the echo service: what a remote device writes comes back as a notification
constexpr const char *echo_service = "12345678-1234-5678-1234-56789abcdef0";
constexpr const char *echo_characteristic = "12345678-1234-5678-1234-56789abcdef1";

void print(const std::string & text)
{
    fmt::print("{}\n", text);
    std::fflush(stdout);
}

void print_load(const std::string & path, const Result<void> & loaded)
{
    print(loaded ? "loaded " + path : "failed: " + loaded.error().message);
}

std::vector<std::string> words_of(const std::string & line)
{
    std::istringstream stream(line);
    std::vector<std::string> words;
    for (std::string word; stream >> word;)
        words.push_back(word);
    return words;
}

std::string hex_of(const std::vector<uint8_t> & bytes)
{
    return fmt::format("{:02x}", fmt::join(bytes, ""));
}

std::string printable(const std::vector<uint8_t> & bytes)
{
    std::string text;
    for (const uint8_t byte : bytes)
        text += byte >= 0x20 && byte < 0x7f ? static_cast<char>(byte) : '.';
    return text;
}

std::optional<uint16_t> handle_of(const std::string & word)
{
    try
    {
        return static_cast<uint16_t>(std::stoul(word, nullptr, 0));
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::string describe(const ScanResult & result, const AssignedNumbers & numbers, const sihd::net::OuiRegistry & oui)
{
    const std::optional<taxonomy::Beacon> beacon = taxonomy::beacon(result);
    std::optional<std::string> vendor = numbers.vendor(result);
    if (!vendor && result.address_type == AddressType::public_address)
        vendor = oui.vendor(std::string_view(result.address));
    return fmt::format("{} {:>4} dBm '{}' kind={}{}{} uuids=[{}]",
                       result.address,
                       result.rssi,
                       result.identifier,
                       taxonomy::kind_str(taxonomy::kind(result)),
                       beacon ? fmt::format(" beacon={}", taxonomy::beacon_str(*beacon)) : "",
                       vendor ? fmt::format(" vendor='{}'", *vendor) : "",
                       fmt::join(result.uuids, ", "));
}

// what a std::jthread reset does: the android libc++ has no jthread
void join(std::thread & thread)
{
    if (thread.joinable())
        thread.join();
}

std::string ask(const std::string & question)
{
    print(question);
    std::string answer;
    std::getline(std::cin, answer);
    return answer;
}

PairingAnswer answer_pairing(const PairingRequest & request)
{
    switch (request.kind)
    {
        case PairingRequest::Kind::confirm_passkey:
            return {.accept = ask(fmt::format("passkey {:06} on {}: confirm? (y/n)", request.passkey, request.address))
                              == "y",
                    .passkey = 0,
                    .pin = {}};
        case PairingRequest::Kind::enter_passkey:
        {
            const std::string passkey = ask(fmt::format("passkey shown by {}:", request.address));
            return {.accept = !passkey.empty(),
                    .passkey = static_cast<uint32_t>(std::strtoul(passkey.c_str(), nullptr, 10)),
                    .pin = {}};
        }
        case PairingRequest::Kind::enter_pin:
        {
            const std::string pin = ask(fmt::format("pin of {}:", request.address));
            return {.accept = !pin.empty(), .passkey = 0, .pin = pin};
        }
        case PairingRequest::Kind::display_passkey:
            print(fmt::format("type {:06} on {}", request.passkey, request.address));
            return {.accept = true, .passkey = 0, .pin = {}};
        case PairingRequest::Kind::display_pin:
            print(fmt::format("type {} on {}", request.pin, request.address));
            return {.accept = true, .passkey = 0, .pin = {}};
        default:
            return {.accept = ask(fmt::format("pair with {}? (y/n)", request.address)) == "y", .passkey = 0, .pin = {}};
    }
}

class Session
{
    public:
        ~Session()
        {
            this->disconnect();
            this->close_stream();
            this->stop_peripheral();
        }

        void load_names(const std::string & option, const std::string & path)
        {
            if (option == "--oui")
                return print_load(path, _oui.load(path));
            if (option != "--assigned-numbers")
                return print("unknown option: " + option);
            const std::string companies = fs::combine({path, "company_identifiers", "company_identifiers.yaml"});
            const std::string members = fs::combine({path, "uuids", "member_uuids.yaml"});
            const std::string services = fs::combine({path, "uuids", "service_uuids.yaml"});
            if (fs::is_file(companies))
                print_load(companies, _numbers.load_company_identifiers(companies));
            if (fs::is_file(members))
                print_load(members, _numbers.load_member_uuids(members));
            if (fs::is_file(services))
                print_load(services, _numbers.load_service_uuids(services));
        }

        void scan(const std::vector<std::string> & words)
        {
            const int seconds = words.size() > 1 ? std::atoi(words[1].c_str()) : 5;
            ScanFilter filter;
            if (words.size() > 2)
                filter.transport = words[2] == "le"        ? Transport::le
                                   : words[2] == "classic" ? Transport::classic
                                                           : Transport::any;
            Scanner scanner("scanner");
            scanner.set_filter(filter);
            Handler<Scanner *> printer([this](Scanner *obj) {
                if (auto result = obj->last_result(); result.has_value())
                    print(describe(*result, _numbers, _oui));
            });
            scanner.add_observer(&printer);
            if (!scanner.scan_for(Duration(time::sec(seconds))))
                print("scan failed");
            print(fmt::format("{} devices", scanner.results().size()));
        }

        void pair(const std::string & address, bool forget)
        {
            Device device("pairing");
            if (!device.set_address(address))
                return print("invalid address");
            // android answers its pairings in the system dialogs only
            if (!build::is_android)
                device.set_pairing_handler(answer_pairing);
            auto done = forget ? device.unpair() : device.pair();
            print(done ? "done" : "failed: " + done.error().message);
        }

        void info(const std::string & address)
        {
            Device device("info");
            if (!device.set_address(address))
                return print("invalid address");
            auto info = device.info();
            if (!info)
                return print("failed: " + info.error().message);
            print(fmt::format("{} '{}' paired={} connected={} trusted={} uuids=[{}]",
                              info->address,
                              info->name,
                              info->paired,
                              info->connected,
                              info->trusted,
                              fmt::join(info->uuids, ", ")));
        }

        void connect(const std::string & address)
        {
            this->disconnect();
            _device = std::make_unique<Device>("device");
            if (!_device->set_address(address))
                return print("invalid address");
            _device_printer = std::make_unique<Handler<Device *>>([](Device *device) {
                switch (device->last_event())
                {
                    case Device::Event::connected:
                        return print("connected");
                    case Device::Event::disconnected:
                        return print("disconnected");
                    default:
                        if (auto notification = device->last_notification(); notification.has_value())
                            print(fmt::format("notify 0x{:04x}: {} '{}'",
                                              notification->handle,
                                              hex_of(notification->value),
                                              printable(notification->value)));
                }
            });
            _device->add_observer(_device_printer.get());
            _device_thread = std::thread([this] {
                if (!_device->start())
                    print("connection ended");
            });
        }

        void services()
        {
            if (!this->connected())
                return;
            auto services = _device->services();
            if (!services)
                return print("failed: " + services.error().message);
            _characteristics.clear();
            for (const GattService & service : *services)
            {
                print(fmt::format("service {} (0x{:04x})", service.uuid, service.handle));
                for (const GattCharacteristic & characteristic : service.characteristics)
                {
                    _characteristics.push_back(characteristic);
                    print(fmt::format("  characteristic 0x{:04x} {} properties=0x{:02x}",
                                      characteristic.handle,
                                      characteristic.uuid,
                                      characteristic.properties));
                    for (const GattDescriptor & descriptor : characteristic.descriptors)
                        print(fmt::format("    descriptor 0x{:04x} {}", descriptor.handle, descriptor.uuid));
                }
            }
        }

        void read(const std::string & word)
        {
            auto characteristic = this->characteristic_of(word);
            if (!characteristic.has_value())
                return;
            auto value = _device->read_characteristic(*characteristic);
            print(value ? fmt::format("0x{:04x}: {} '{}'", characteristic->handle, hex_of(*value), printable(*value))
                        : "failed: " + value.error().message);
        }

        void write(const std::vector<std::string> & words)
        {
            auto characteristic = this->characteristic_of(words[1]);
            auto bytes = str::from_hex(words[2]);
            if (!characteristic.has_value() || !bytes.has_value())
                return print("write <handle> <hex> [command]");
            const WriteType type = words.size() > 3 && words[3] == "command" ? WriteType::without_response
                                                                             : WriteType::with_response;
            auto written = _device->write_characteristic(*characteristic, *bytes, type);
            print(written ? "written" : "failed: " + written.error().message);
        }

        void notify(const std::string & word, bool subscribe)
        {
            auto characteristic = this->characteristic_of(word);
            if (!characteristic.has_value())
                return;
            auto done = subscribe ? _device->subscribe(*characteristic) : _device->unsubscribe(*characteristic);
            print(done ? "done" : "failed: " + done.error().message);
        }

        void mtu()
        {
            if (!this->connected())
                return;
            auto mtu = _device->mtu();
            print(mtu ? fmt::format("mtu {}", *mtu) : "failed: " + mtu.error().message);
        }

        void disconnect()
        {
            if (_device == nullptr)
                return;
            if (_device->is_running())
                _device->stop();
            join(_device_thread);
            _device.reset();
            _characteristics.clear();
        }

        void open_stream(const Result<void> & opened, Socket socket)
        {
            if (!opened)
                return print("failed: " + opened.error().message);
            this->close_stream();
            _socket = std::move(socket);
            print(fmt::format("stream open with {}", _socket.address()));
            _reader = std::thread([this] {
                ArrUByte buffer;
                buffer.reserve(1024);
                while (true)
                {
                    auto received = _socket.receive(buffer);
                    if (!received || *received == 0)
                        return print("stream closed");
                    const std::vector<uint8_t> bytes(buffer.data(), buffer.data() + *received);
                    print(fmt::format("< {} '{}'", hex_of(bytes), printable(bytes)));
                }
            });
        }

        void listen(Socket & listener, const Result<void> & listening)
        {
            if (!listening)
                return print("failed: " + listening.error().message);
            if (listener.psm() != 0)
                print(fmt::format("listening on psm {}", listener.psm()));
            else
                print("listening");
            auto accepted = listener.accept(Duration(time::sec(60)));
            if (!accepted)
                return print("failed: " + accepted.error().message);
            this->open_stream({}, std::move(*accepted));
        }

        void send(const std::string & text)
        {
            if (!_socket.is_open())
                return print("no open stream");
            const std::string line = text + "\n";
            auto sent = _socket.send(ArrUByteView(reinterpret_cast<const uint8_t *>(line.data()), line.size()));
            if (!sent)
                print("failed: " + sent.error().message);
        }

        void close_stream()
        {
            _socket.close();
            join(_reader);
        }

        void advertise(const std::string & name)
        {
            if (_advertiser != nullptr)
                return print("already advertising");
            _advertiser = std::make_unique<Advertiser>("advertiser");
            _advertiser->set_advertisement(Advertisement {
                .local_name = name,
                .service_uuids = {echo_service},
                .manufacturer_data = {},
                .service_data = {},
                .connectable = true,
            });
            _advertiser_thread = std::thread([this] {
                if (!_advertiser->start())
                    print("advertising ended");
            });
        }

        void serve()
        {
            if (_server != nullptr)
                return print("already serving");
            _server = std::make_unique<GattServer>("server");
            GattServer *server = _server.get();
            _server->add_service(LocalService {
                .uuid = echo_service,
                .primary = true,
                .characteristics = {LocalCharacteristic {
                    .uuid = echo_characteristic,
                    .properties = GattCharacteristic::read | GattCharacteristic::write | GattCharacteristic::notify,
                    .on_read = {},
                    .on_write =
                        [server](const std::vector<uint8_t> & value) {
                            print(fmt::format("written: {} '{}'", hex_of(value), printable(value)));
                            (void)server->notify(echo_service, echo_characteristic, value);
                            return true;
                        },
                }},
            });
            _server_thread = std::thread([this] {
                if (!_server->start())
                    print("serving ended");
            });
        }

        void stop_peripheral()
        {
            if (_advertiser != nullptr && _advertiser->is_running())
                _advertiser->stop();
            join(_advertiser_thread);
            _advertiser.reset();
            if (_server != nullptr && _server->is_running())
                _server->stop();
            join(_server_thread);
            _server.reset();
        }

    private:
        bool connected() const
        {
            if (_device == nullptr || !_device->is_running())
            {
                print("no connected device");
                return false;
            }
            return true;
        }

        std::optional<GattCharacteristic> characteristic_of(const std::string & word)
        {
            if (!this->connected())
                return std::nullopt;
            const std::optional<uint16_t> handle = handle_of(word);
            for (const GattCharacteristic & characteristic : _characteristics)
            {
                if (handle.has_value() && characteristic.handle == *handle)
                    return characteristic;
            }
            print("unknown characteristic: run services");
            return std::nullopt;
        }

        std::unique_ptr<Device> _device;
        std::unique_ptr<Handler<Device *>> _device_printer;
        std::thread _device_thread;
        std::vector<GattCharacteristic> _characteristics;
        Socket _socket;
        std::thread _reader;
        std::unique_ptr<Advertiser> _advertiser;
        std::thread _advertiser_thread;
        std::unique_ptr<GattServer> _server;
        std::thread _server_thread;
        AssignedNumbers _numbers;
        sihd::net::OuiRegistry _oui;
};

void list_adapters_command()
{
    auto adapters = list_adapters();
    if (!adapters)
        return print("failed: " + adapters.error().message);
    for (size_t index = 0; index < adapters->size(); ++index)
    {
        const AdapterInfo & adapter = (*adapters)[index];
        print(fmt::format("{}: {} [{}] '{}' powered={} discoverable={}",
                          index,
                          adapter.identifier,
                          adapter.address,
                          adapter.alias,
                          adapter.powered,
                          adapter.discoverable));
    }
}

std::string joined(const std::vector<std::string> & words, size_t from)
{
    std::string text;
    for (size_t i = from; i < words.size(); ++i)
        text += (i > from ? " " : "") + words[i];
    return text;
}

// each run_* answers false on a command it does not know
bool run_adapter(Session & session, const std::vector<std::string> & words)
{
    const std::string & command = words[0];
    if (command == "adapters")
        list_adapters_command();
    else if (command == "power" && words.size() > 1)
    {
        auto powered = set_adapter_powered(0, words[1] == "on");
        print(powered ? "done" : "failed: " + powered.error().message);
    }
    else if (command == "scan")
        session.scan(words);
    else
        return false;
    return true;
}

bool run_device(Session & session, const std::vector<std::string> & words)
{
    const std::string & command = words[0];
    if (command == "info" && words.size() > 1)
        session.info(words[1]);
    else if (command == "connect" && words.size() > 1)
        session.connect(words[1]);
    else if ((command == "pair" || command == "unpair") && words.size() > 1)
        session.pair(words[1], command == "unpair");
    else if (command == "services")
        session.services();
    else if (command == "read" && words.size() > 1)
        session.read(words[1]);
    else if (command == "write" && words.size() > 2)
        session.write(words);
    else if ((command == "notify" || command == "unnotify") && words.size() > 1)
        session.notify(words[1], command == "notify");
    else if (command == "mtu")
        session.mtu();
    else if (command == "disconnect")
        session.disconnect();
    else
        return false;
    return true;
}

bool run_stream(Session & session, const std::vector<std::string> & words)
{
    const std::string & command = words[0];
    if (command == "rfcomm" && words.size() > 2)
    {
        Socket socket;
        auto channel = handle_of(words[2]);
        auto connected = words[2].size() == 36 || !channel.has_value()
                             ? socket.connect_rfcomm(words[1], words[2])
                             : socket.connect_rfcomm_channel(words[1], static_cast<uint8_t>(*channel));

        session.open_stream(connected, std::move(socket));
    }
    else if (command == "l2cap" && words.size() > 3)
    {
        const AddressType type = words[2] == "public" ? AddressType::public_address : AddressType::random;
        Socket socket;
        auto connected = socket.connect_l2cap(words[1], type, handle_of(words[3]).value_or(0));

        session.open_stream(connected, std::move(socket));
    }
    else if (command == "listen" && words.size() > 1)
    {
        Socket listener;
        auto listening = listener.listen_rfcomm(words[1], words.size() > 2 ? words[2] : "sihd");

        session.listen(listener, listening);
    }
    else if (command == "listen-l2cap")
    {
        Socket listener;
        auto listening = listener.listen_l2cap(words.size() > 1 ? handle_of(words[1]).value_or(0) : 0);

        session.listen(listener, listening);
    }
    else if (command == "send" && words.size() > 1)
        session.send(joined(words, 1));
    else if (command == "close")
        session.close_stream();
    else
        return false;
    return true;
}

bool run_peripheral(Session & session, const std::vector<std::string> & words)
{
    const std::string & command = words[0];
    if (command == "advertise")
        session.advertise(words.size() > 1 ? words[1] : "");
    else if (command == "serve")
        session.serve();
    else if (command == "stop")
        session.stop_peripheral();
    else
        return false;
    return true;
}

bool run(Session & session, const std::vector<std::string> & words)
{
    if (words[0] == "quit")
        return false;
    if (!run_adapter(session, words) && !run_device(session, words) && !run_stream(session, words)
        && !run_peripheral(session, words))
        print(help);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    LoggerManager::stream();
    print(help);
    Session session;
    for (int i = 1; i + 1 < argc; i += 2)
        session.load_names(argv[i], argv[i + 1]);
    for (std::string line; std::getline(std::cin, line);)
    {
        const std::vector<std::string> words = words_of(line);
        if (words.empty())
            continue;
        print("> " + line);
        if (!run(session, words))
            break;
    }
    LoggerManager::clear_loggers();
    return 0;
}
