#include <cstdlib>
#include <iostream>
#include <thread>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::net;

SIHD_NEW_LOGGER("tcp-echo-demo");

static int run_server(sihd::sys::App & app, TcpServer & server)
{
    BasicServerHandler handler;
    server.set_server_handler(&handler);

    Handler<BasicServerHandler *> echo_handler([](BasicServerHandler *srv) {
        for (auto & client : srv->new_clients())
            SIHD_LOG(info, "client connected: {}", client->fd());
        for (auto & client : srv->read_activity())
        {
            if (client->disconnected || client->error)
                SIHD_LOG(info, "client disconnected: {}", client->fd());
            else
                srv->send_to_client(client, client->read_array);
        }
    });
    handler.add_observer(&echo_handler);

    std::thread server_thread([&] {
        server.start();
        app.stop();
    });
    SIHD_LOG(notice, "echo server started, press ctrl+C to stop");
    app.loop();
    server.stop();
    server_thread.join();
    return EXIT_SUCCESS;
}

static int run_client(TcpClient & client, const std::string & host, int port, const std::string & message)
{
    if (!client.open_and_connect(host, port, 2000))
    {
        SIHD_LOG(error, "cannot connect to {}:{}", host, port);
        return EXIT_FAILURE;
    }
    SIHD_LOG(notice, "sending: {}", message);
    if (!client.send_all({message.data(), message.size()}))
    {
        SIHD_LOG(error, "send failed");
        return EXIT_FAILURE;
    }
    ArrChar recv(512);
    if (!client.poll(1000))
    {
        SIHD_LOG(error, "no reply");
        return EXIT_FAILURE;
    }
    ssize_t received = client.receive(recv);
    if (received <= 0)
    {
        SIHD_LOG(error, "no echo received");
        return EXIT_FAILURE;
    }
    std::cout << "echo: " << std::string_view(recv.data(), (size_t)received) << std::endl;
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    App app({
        .name = "tcp_echo_demo",
        .description = "TCP echo client/server with module net",
    });

    std::string mode = "server";
    std::string host = "127.0.0.1";
    std::string message = "hello echo";
    int port = 4242;

    app.root().bind("mode", mode, "server or client", "m");
    app.root().bind("port", port, "Port to listen on or connect to", "p");
    app.root().bind("host", host, "Host to bind (server) or connect to (client)");
    app.root().bind("message", message, "Message to send (client mode)");

    TcpServer server("echo-server");
    TcpClient client("echo-client");

    app.root().on_run([&] {
        if (mode == "server")
        {
            if (!server.open_and_bind(IpAddr(host, port)))
            {
                SIHD_LOG(error, "cannot bind {}:{}", host, port);
                app.exit(EXIT_FAILURE);
            }
            app.exit(run_server(app, server));
        }
        else if (mode == "client")
        {
            app.exit(run_client(client, host, port, message));
        }
        else
        {
            SIHD_LOG(error, "unknown mode: {} (server or client)", mode);
            app.exit(EXIT_FAILURE);
        }
    });

    return app.run(argc, argv);
}
