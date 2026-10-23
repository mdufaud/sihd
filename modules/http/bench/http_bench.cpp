#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::http;
using namespace sihd::util;

class BenchServer: public HttpServer
{
    public:
        BenchServer(): HttpServer("bench-server") { webservice = this->add_child<WebService>("api"); }

        WebService *webservice;
};

struct BenchServerScope
{
        BenchServer server;
        std::atomic<int64_t> handler_calls {0};
        Worker worker {[this] {
            server.start();
            return true;
        }};

        ~BenchServerScope()
        {
            server.set_service_wait_stop(true);
            server.stop();
        }

        bool start(int port, int service_threads)
        {
            server.webservice->set_entry_point("task", [this](const HttpRequest &, HttpResponse & resp) {
                ++handler_calls;
                resp.set_plain_content("pool-ok");
            });
            if (service_threads > 0)
                server.set_service_thread_count(static_cast<size_t>(service_threads));
            server.set_port(port);
            return worker.start_sync_worker("bench-server") && server.wait_ready(std::chrono::milliseconds(500));
        }
};

// outside the 30xx range the http tests (and their shards) listen on
constexpr int bench_port = 3900;

// one-shot requests: each one spawns a client thread and opens its own connection
void BM_http_get_batch(benchmark::State & state)
{
    if constexpr (build::is_run_with_tsan)
    {
        state.SkipWithMessage("lws and tsan interact badly");
        return;
    }
    // concurrent requests per iteration: stay well below ulimit -n (default 1024)
    constexpr int batch_size = 50;
    const int service_threads = static_cast<int>(state.range(0));
    const int port = bench_port + service_threads;

    BenchServerScope scope;
    if (!scope.start(port, service_threads))
    {
        state.SkipWithError("http server did not start");
        return;
    }

    const std::string url = "localhost:" + std::to_string(port) + "/api/task";
    std::vector<FutureHttpResponse> futures;
    futures.reserve(batch_size);
    bool ok = true;
    for (auto _ : state)
    {
        futures.clear();
        for (int i = 0; i < batch_size; ++i)
            futures.push_back(sihd::http::async_get(url));
        for (FutureHttpResponse & future : futures)
        {
            auto resp = future.get();
            ok = ok && resp.has_value() && resp->status() == HttpStatus::Ok;
        }
        if (!ok)
        {
            state.SkipWithError("a request failed");
            break;
        }
    }
    if (ok && scope.handler_calls.load() != state.iterations() * batch_size)
        state.SkipWithError("handler calls do not match the requests sent");
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// one kept-alive connection, one request at a time: the server's request path and a loopback round trip
void BM_http_get_keepalive(benchmark::State & state)
{
    if constexpr (build::is_run_with_tsan)
    {
        state.SkipWithMessage("lws and tsan interact badly");
        return;
    }
    const int port = bench_port + 10;

    BenchServerScope scope;
    if (!scope.start(port, 0))
    {
        state.SkipWithError("http server did not start");
        return;
    }

    Navigator navigator;
    navigator.clear_proxy();
    navigator.set_ssrf_guard(false);
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/api/task";
    for (auto _ : state)
    {
        auto resp = navigator.get(url);
        if (!resp.has_value() || resp->status() != HttpStatus::Ok)
        {
            state.SkipWithError("a request failed");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_http_get_batch)->ArgName("service_threads")->Arg(0)->Arg(4)->UseRealTime();
BENCHMARK(BM_http_get_keepalive)->UseRealTime();

} // namespace
