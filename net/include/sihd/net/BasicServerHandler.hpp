#ifndef __SIHD_NET_BASICSERVERHANDLER_HPP__
# define __SIHD_NET_BASICSERVERHANDLER_HPP__

# include <sihd/net/INetServerHandler.hpp>
# include <sihd/net/Socket.hpp>
# include <sihd/net/IpAddr.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Observable.hpp>
# include <sihd/util/Configurable.hpp>

namespace sihd::net
{

class BasicServerHandler:   public INetServerHandler,
                            public sihd::util::Configurable,
                            public sihd::util::Observable<BasicServerHandler>
{
    public:
        class Client
        {
            public:
                Client(int sock):
                    _socket(sock),
                    read_array(nullptr),
                    write_array(nullptr),
                    time_connected(0),
                    time_total(0),
                    error(false),
                    disconnected(false)
                {
                }
                ~Client() {}

                int socket() { return _socket.socket(); }

                Socket _socket;
                sihd::util::ArrByte *read_array;
                sihd::util::ArrByte *write_array;
                IpAddr addr;

                time_t time_connected;
                time_t time_total; //todo
                bool error;
                bool disconnected;
        };

        BasicServerHandler();
        virtual ~BasicServerHandler();

		void handle_no_activity(INetServer *server, time_t nano);
		void handle_activity(INetServer *server, time_t nano);
		void handle_new_client(INetServer *server);
		void handle_client_read(INetServer *server, int socket);
		void handle_client_write(INetServer *server, int socket);
        void handle_after_activity(INetServer *server);

        bool set_max_clients(size_t max);

        bool send_to_client(int socket, const sihd::util::IArray & arr);
        bool remove_client(int socket);

        std::vector<Client *> & clients() { return _client_lst; }
        std::vector<Client *> & read_activity() { return _read_event_lst; }
        std::vector<Client *> & write_activity() { return _write_event_lst; }
        std::vector<Client *> & new_clients() { return _connect_event_lst; }
        time_t poll_time() { return _poll_time; }
        INetServer *server() { return _server; }
        Client *client(int socket) { return _client_map[socket]; }

    private:
        void _reset();
        void _add_time_to_clients();
        void _delete_client(Client *c);

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

}

#endif