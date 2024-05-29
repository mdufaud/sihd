#ifndef __SIHD_NET_BASICSERVERHANDLER_HPP__
#define __SIHD_NET_BASICSERVERHANDLER_HPP__

#include <sihd/net/INetServerHandler.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Observable.hpp>

namespace sihd::net
{

class BasicServerHandler: public INetServerHandler,
                          public sihd::util::Configurable,
                          public sihd::util::Observable<BasicServerHandler>
{
    public:
        class Client
        {
            public:
                Client(int sock): socket(sock), time_connected(0), time_total(0), error(false), disconnected(false) {}
                ~Client() {}

                int fd() { return socket.socket(); }

                Socket socket;
                sihd::util::ArrByte read_array;
                sihd::util::ArrByte write_array;
                IpAddr addr;

                sihd::util::Timestamp time_connected;
                sihd::util::Timestamp time_total;
                bool error;
                bool disconnected;
        };

        BasicServerHandler();
        virtual ~BasicServerHandler();

        bool set_max_clients(size_t max);

        bool send_to_client(Client *client, const sihd::util::IArray & arr);
        bool remove_client(Client *client);
        bool send_to_client(int socket, const sihd::util::IArray & arr);
        bool remove_client(int socket);

        const std::vector<Client *> & clients() const { return _client_lst; }
        const std::vector<Client *> & read_activity() const { return _read_event_lst; }
        const std::vector<Client *> & write_activity() const { return _write_event_lst; }
        const std::vector<Client *> & new_clients() const { return _connect_event_lst; }
        sihd::util::Timestamp poll_time() const { return _poll_time; }
        INetServer *server() { return _server; }
        Client *client(int socket) { return _client_map[socket]; }

    protected:
        void handle_no_activity(INetServer *server, time_t nano);
        void handle_activity(INetServer *server, time_t nano);
        void handle_new_client(INetServer *server);
        void handle_client_read(INetServer *server, int socket);
        void handle_client_write(INetServer *server, int socket);
        void handle_after_activity(INetServer *server);

    private:
        void _reset();
        void _add_time_to_clients();

        std::vector<Client *> _client_lst;
        std::map<int, Client *> _client_map;
        sihd::util::SystemClock _clock;

        std::vector<Client *> _read_event_lst;
        std::vector<Client *> _write_event_lst;
        std::vector<Client *> _connect_event_lst;
        time_t _poll_time;
        time_t _last_time;
        INetServer *_server;

        size_t _max_clients;
};

} // namespace sihd::net

#endif