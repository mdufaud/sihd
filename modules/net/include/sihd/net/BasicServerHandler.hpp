#ifndef __SIHD_NET_BASICSERVERHANDLER_HPP__
#define __SIHD_NET_BASICSERVERHANDLER_HPP__

#include <map>
#include <memory>
#include <mutex>
#include <optional>

#include <sihd/net/INetServerHandler.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/TlsSocket.hpp>
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
                enum class State
                {
                    handshaking,
                    ready
                };

                Client(int sock): socket(sock), time_connected(0), time_total(0), error(false), disconnected(false) {}
                ~Client() = default;

                int fd() const { return socket.socket(); }

                TlsSocket socket;
                mutable std::mutex mutex;
                sihd::util::ArrByte read_array;
                sihd::util::ArrByte write_array;
                size_t write_offset = 0;
                IpAddr addr;

                State state = State::ready;
                TlsHandshakeStep handshake_step = TlsHandshakeStep::complete;
                sihd::util::Timestamp handshake_deadline;

                sihd::util::Timestamp time_connected;
                sihd::util::Timestamp time_total;
                bool error;
                // set with the last payload left in read_array on a peer FIN
                bool disconnected;
        };

        using ClientPtr = std::shared_ptr<Client>;

        BasicServerHandler();
        virtual ~BasicServerHandler();

        void set_tls_context(sihd::crypto::TlsContext ctx);

        bool set_max_clients(size_t max);
        bool set_tls_accept_timeout(int milliseconds);

        bool send_to_client(const ClientPtr & client, const sihd::util::IArray & arr);
        bool remove_client(const ClientPtr & client);
        bool send_to_client(int socket, const sihd::util::IArray & arr);
        bool remove_client(int socket);

        std::vector<ClientPtr> clients() const;
        // activity lists are only valid inside the observer callback
        const std::vector<ClientPtr> & read_activity() const { return _read_event_lst; }
        const std::vector<ClientPtr> & write_activity() const { return _write_event_lst; }
        const std::vector<ClientPtr> & new_clients() const { return _connect_event_lst; }
        sihd::util::Duration poll_time() const { return _poll_time; }
        INetServer *server();
        size_t client_count() const;
        ClientPtr client(int socket);

    protected:
        void handle_no_activity(INetServer *server, sihd::util::time::UnixTime milliseconds);
        void handle_activity(INetServer *server, sihd::util::time::UnixTime milliseconds);
        void handle_new_client(INetServer *server);
        void handle_client_read(INetServer *server, int socket);
        void handle_client_write(INetServer *server, int socket);
        void handle_after_activity(INetServer *server);

    private:
        using ClientMap = std::map<int, ClientPtr>;

        void _reset();
        void _add_time_to_clients();
        bool _client_limit_reached() const;
        void _advance_tls_handshake(INetServer *server, ClientMap::iterator it, bool readable, bool writable);
        ClientMap::iterator _drop_client(INetServer *server, ClientMap::iterator it);
        void _expire_tls_handshakes(INetServer *server, sihd::util::Timestamp now);

        mutable std::recursive_mutex _mutex;
        ClientMap _client_map;
        sihd::util::SystemClock _clock;
        sihd::util::SteadyClock _steady_clock;

        std::vector<ClientPtr> _read_event_lst;
        std::vector<ClientPtr> _write_event_lst;
        std::vector<ClientPtr> _connect_event_lst;
        sihd::util::Duration _poll_time;
        sihd::util::Timestamp _last_time;
        INetServer *_server;

        std::optional<sihd::crypto::TlsContext> _tls_ctx;
        int _tls_accept_timeout;
        size_t _max_clients;
};

} // namespace sihd::net

#endif