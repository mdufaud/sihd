#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/bt/Socket.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtSocket: public ::testing::Test
{
    protected:
        TestBtSocket(): _peer(SOCK_STREAM) { LoggerManager::stream(); }

        virtual ~TestBtSocket() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            serve_device(_daemon);
            _daemon.on(profile_manager_interface, "RegisterProfile", [this](const dbus::Message & call) {
                this->record_profile(call);
                _daemon.reply(call, {});
                if (_role == "server")
                    this->new_connection(false);
            });
            answer(_daemon, profile_manager_interface, {"UnregisterProfile"});
            _daemon.on(device_interface, "ConnectProfile", [this](const dbus::Message & call) {
                this->new_connection(true);
                _daemon.reply(call, {});
            });
        }

        // from a handler
        void record_profile(const dbus::Message & call)
        {
            dbus::Iter arguments(call);
            _profile_sender = call.sender();
            _profile_path = arguments.get<std::string>().value_or("");
            arguments.next();
            _profile_uuid = arguments.get<std::string>().value_or("");
            arguments.next();
            (void)arguments.for_each_entry<std::string>([this](const std::string & key, dbus::Iter & value) {
                if (key == "Role")
                    _role = value.get<std::string>().value_or("");
                else if (key == "Name")
                    _service_name = value.get<std::string>().value_or("");
                return Result<void> {};
            });
        }

        // bluez hands the connected socket to the profile: a connecting client answers before ConnectProfile ends
        void new_connection(bool wait_answer)
        {
            dbus::Message request;
            auto built = request.method_call(_profile_sender, _profile_path, profile_interface, "NewConnection");
            ASSERT_TRUE(built.has_value()) << built.error().message;
            dbus::Appender arguments(request);
            ASSERT_TRUE(arguments.add_object_path(device_path).has_value());
            ASSERT_TRUE(arguments.add_unix_fd(_peer.local()).has_value());
            ASSERT_TRUE(append_entries(arguments, {}).has_value());
            if (wait_answer)
                ASSERT_TRUE(request.call(_daemon.connection(), fake_timeout).has_value());
            else
                ASSERT_TRUE(request.send(_daemon.connection()).has_value());
        }

        static std::string receive_text(Socket & socket)
        {
            ArrUByte buffer;
            buffer.reserve(64);
            auto received = socket.receive(buffer);
            return received.has_value() ? buffer.cpp_str() : received.error().message;
        }

        static Result<size_t> send_text(Socket & socket, const std::string & text)
        {
            return socket.send(ArrUByteView(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
        }

        FakeDaemon _daemon;
        SocketPair _peer;
        std::string _profile_sender;
        std::string _profile_path;
        std::string _profile_uuid;
        std::string _role;
        std::string _service_name;
};

TEST_F(TestBtSocket, test_connect_rfcomm)
{
    _daemon.serve();

    Socket socket;
    auto connected = socket.connect_rfcomm("aa:bb:cc:dd:ee:ff", serial_port_uuid);
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    auto sent = send_text(socket, "ping");
    const std::string peer_received = _peer.read_remote();
    ASSERT_TRUE(_peer.write_remote("pong"));
    const std::string received = receive_text(socket);
    _daemon.stop();

    EXPECT_EQ(socket.address(), device_address);
    EXPECT_TRUE(socket.is_open());
    ASSERT_TRUE(sent.has_value()) << sent.error().message;
    EXPECT_EQ(*sent, 4u);
    EXPECT_EQ(peer_received, "ping");
    EXPECT_EQ(received, "pong");
    EXPECT_EQ(_role, "client");
    EXPECT_EQ(_profile_uuid, serial_port_uuid);
    EXPECT_TRUE(_daemon.called(profile_manager_interface, "UnregisterProfile"));
}

TEST_F(TestBtSocket, test_connect_connected_socket)
{
    _daemon.serve();

    Socket socket(device_address, serial_port_uuid);
    auto again = socket.connect_rfcomm(device_address, serial_port_uuid);
    _daemon.stop();

    EXPECT_TRUE(socket.is_open());
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code, ErrorCode::already_exists);
    EXPECT_EQ(again.error().message, "bluetooth socket already connected to AA:BB:CC:DD:EE:FF");
}

TEST_F(TestBtSocket, test_connect_rfcomm_refused)
{
    _daemon.on(device_interface, "ConnectProfile", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.NotAvailable", "Operation currently not available");
    });
    _daemon.serve();

    Socket socket;
    auto connected = socket.connect_rfcomm(device_address, serial_port_uuid);
    _daemon.stop();

    EXPECT_FALSE(socket.is_open());
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::not_found);
    EXPECT_EQ(connected.error().message, "org.bluez.Error.NotAvailable: Operation currently not available");
    EXPECT_TRUE(_daemon.called(profile_manager_interface, "UnregisterProfile"));
}

TEST_F(TestBtSocket, test_connect_rfcomm_unknown_device)
{
    _daemon.serve();

    Socket socket;
    auto connected = socket.connect_rfcomm("11:22:33:44:55:66", serial_port_uuid);
    _daemon.stop();

    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::not_found);
    EXPECT_EQ(connected.error().message, "bluez does not know 11:22:33:44:55:66, a scan finds it");
    EXPECT_FALSE(_daemon.called(profile_manager_interface, "RegisterProfile"));
}

TEST_F(TestBtSocket, test_invalid_addresses)
{
    Socket socket;

    auto channel = socket.connect_rfcomm_channel("AA:BB:CC:DD:EE", 1);
    auto l2cap = socket.connect_l2cap(device_address, AddressType::unspecified, 0x0080);

    ASSERT_FALSE(channel.has_value());
    EXPECT_EQ(channel.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(channel.error().message, "invalid bluetooth address: AA:BB:CC:DD:EE");
    ASSERT_FALSE(l2cap.has_value());
    EXPECT_EQ(l2cap.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(l2cap.error().message, "an L2CAP LE channel needs the address type of AA:BB:CC:DD:EE:FF");
}

TEST_F(TestBtSocket, test_constructor_throws)
{
    EXPECT_THROW({ Socket socket("AA:BB:CC:DD:EE", uint8_t {1}); }, std::runtime_error);
    EXPECT_THROW({ Socket socket(device_address, AddressType::unspecified, 0x0080); }, std::runtime_error);
    // LE psms end at 0x00ff
    EXPECT_THROW({ Socket listener(uint16_t {0x0100}); }, std::runtime_error);
}

TEST_F(TestBtSocket, test_unconnected)
{
    Socket socket;
    Socket listener;

    auto sent = send_text(socket, "early");
    auto accepted = listener.accept(Duration(time::milli(10)));

    EXPECT_FALSE(socket.is_open());
    EXPECT_FALSE(socket.is_listening());
    EXPECT_EQ(socket.address(), "");
    EXPECT_EQ(socket.psm(), 0);
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error().code, ErrorCode::closed);
    EXPECT_EQ(sent.error().message, "bluetooth socket is closed");
    ASSERT_FALSE(accepted.has_value());
    EXPECT_EQ(accepted.error().code, ErrorCode::closed);
    EXPECT_EQ(accepted.error().message, "bluetooth socket is closed");
}

TEST_F(TestBtSocket, test_listen_rfcomm)
{
    _daemon.serve();

    Socket listener;
    auto listening = listener.listen_rfcomm(serial_port_uuid, "sihd serial");
    ASSERT_TRUE(listening.has_value()) << listening.error().message;
    auto socket = listener.accept(Duration(time::sec(5)));
    ASSERT_TRUE(socket.has_value()) << socket.error().message;
    ASSERT_TRUE(_peer.write_remote("hello"));
    const std::string received = receive_text(*socket);
    _daemon.stop();

    EXPECT_TRUE(listener.is_listening());
    EXPECT_EQ(listener.address(), "");
    EXPECT_TRUE(socket->is_open());
    EXPECT_FALSE(socket->is_listening());
    EXPECT_EQ(socket->address(), device_address);
    EXPECT_EQ(received, "hello");
    EXPECT_EQ(_role, "server");
    EXPECT_EQ(_service_name, "sihd serial");
}

TEST_F(TestBtSocket, test_roles_refused)
{
    _daemon.serve();
    Socket listener;
    ASSERT_TRUE(listener.listen_rfcomm(serial_port_uuid, "").has_value());
    Socket socket(device_address, serial_port_uuid);

    auto listened_again = listener.listen_rfcomm(serial_port_uuid, "");
    auto connected_listener = listener.connect_rfcomm(device_address, serial_port_uuid);
    auto sent = send_text(listener, "nope");
    auto accepted = socket.accept(Duration(time::milli(10)));
    _daemon.stop();

    ASSERT_FALSE(listened_again.has_value());
    EXPECT_EQ(listened_again.error().code, ErrorCode::already_exists);
    EXPECT_EQ(listened_again.error().message, "bluetooth socket already listening");
    ASSERT_FALSE(connected_listener.has_value());
    EXPECT_EQ(connected_listener.error().code, ErrorCode::already_exists);
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(sent.error().message, "bluetooth socket is listening: accept a client");
    ASSERT_FALSE(accepted.has_value());
    EXPECT_EQ(accepted.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(accepted.error().message, "bluetooth socket is not listening");
}

TEST_F(TestBtSocket, test_accept_times_out)
{
    _daemon.on(profile_manager_interface, "RegisterProfile", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
    });
    _daemon.serve();

    Socket listener;
    ASSERT_TRUE(listener.listen_rfcomm(serial_port_uuid, "").has_value());
    auto accepted = listener.accept(Duration(time::milli(50)));

    ASSERT_FALSE(accepted.has_value());
    EXPECT_EQ(accepted.error().code, ErrorCode::timeout);
    EXPECT_EQ(accepted.error().message, "no RFCOMM connection");
}

TEST_F(TestBtSocket, test_close_wakes_accept)
{
    _daemon.on(profile_manager_interface, "RegisterProfile", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
    });
    _daemon.serve();
    Socket listener;
    ASSERT_TRUE(listener.listen_rfcomm(serial_port_uuid, "").has_value());

    Result<Socket> accepted = std::unexpected(Error(ErrorCode::unknown, "not accepted"));
    std::jthread accepting([&listener, &accepted] { accepted = listener.accept(Duration(time::sec(30))); });
    listener.close();
    accepting.join();

    EXPECT_FALSE(listener.is_open());
    EXPECT_FALSE(listener.is_listening());
    ASSERT_FALSE(accepted.has_value());
    EXPECT_EQ(accepted.error().code, ErrorCode::closed);
    EXPECT_EQ(accepted.error().message, "bluetooth socket is closed");
}

TEST_F(TestBtSocket, test_closed_socket)
{
    _daemon.serve();
    Socket socket(device_address, serial_port_uuid);

    socket.close();
    auto sent = send_text(socket, "late");
    const std::string received = receive_text(socket);

    EXPECT_FALSE(socket.is_open());
    ASSERT_FALSE(sent.has_value());
    EXPECT_EQ(sent.error().code, ErrorCode::closed);
    EXPECT_EQ(received, "bluetooth socket is closed");
}

} // namespace test
