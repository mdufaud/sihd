#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Runnable.hpp>

#include <sihd/http/HttpServer.hpp>

//#include <uWebSockets/App.h>

#include <libwebsockets.h>

static const struct lws_http_mount mount = {
    /* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./test/resources/mount_point", /* serve from dir */
	/* .def */			    "index.html",	/* default filename */
	/* .protocol */			"http-only",
	/* .cgienv */			NULL,
	/* .extra_mimetypes */	NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */	0,
	/* .auth_mask */		0,
	/* .cache_reusable */	0,
	/* .cache_revalidate */	0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */	LWSMPRO_FILE, /* bind to callback */
	/* .mountpoint_len */	1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::http;
    using namespace sihd::util;
    class TestHttpServer: public ::testing::Test
    {
        protected:
            TestHttpServer()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "http",
                    "httpserver"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestHttpServer()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestHttpServer, test_httpserver)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");

        HttpServer server("http-server");
        OS::add_signal_handler(SIGINT, new Runnable([&server] () -> bool
        {
            server.stop();
            return true;
        }));
        server.set_root_dir("test/resources/mount_point");
        server.set_port(3000);
        server.run();
    }

    /*
    TEST_F(TestHttpServer, test_httpserver)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");

        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
        lws_set_log_level(logs, NULL);
        lwsl_user("LWS minimal http server TLS | visit https://localhost:3000\n");

        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));
        info.port = 3000;
        info.mounts = &mount;
        info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
        struct lws_context *context = lws_create_context(&info);
        GTEST_ASSERT_NE(context, nullptr);
        bool interrupt = false;
        OS::add_signal_handler(SIGINT, new Runnable([&interrupt] () -> bool
        {
            interrupt = true;
            return true;
        }));
        int n = 0;
        while (interrupt == false && n >= 0)
            n = lws_service(context, 0);
        lws_context_destroy(context);
    }
    */

    /*
    TEST_F(TestHttpServer, test_httpserver)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");
        uWS::App app = uWS::App();
        app.get("/ *", [](uWS::HttpResponse<false> *res, uWS::HttpRequest *req)
        {
            LOG(info, "Url: " << req->getUrl());
            LOG(info, "Method: " << req->getMethod());
            auto it = req->begin();
            while (it != req->end())
            {
                LOG(info, "Header: " << it.ptr->key << "=" << it.ptr->value);
                ++it;
            }
            LOG(info, "Header host: " << req->getHeader("host"));
            LOG(info, "Header user agent: " << req->getHeader("user-agent"));
            std::string_view parameter;
            int i = 0;
            while ((parameter = req->getParameter(i)) != "")
            {
                LOG(info, "Parameter[" << i << "]: " << parameter);
                ++i;
            }
            LOG(info, "Query: " << req->getQuery());
            LOG(info, "Yield: " << req->getYield());
            //
            if (Files::is_file(_cwd + req->getUrl()))
            {
                size_t bufsize = 4096;
                char buf[bufsize + 1];
                File file(_cwd + req->getUrl(), "r");
                if (file.is_open())
                {
                    if (file.read(buf, bufsize) > 0)
                    {
                        res->writeStatus(uWS::HTTP_200_OK);
                        res->writeHeader("Content-Type", "application/text");
                    }
                }
            }
            res->end("No file");
        });
        app.listen(3000, [] (us_listen_socket_t *listen_socket)
        {
            if (listen_socket)
            {
                std::cout << "Listening on port "
                    << us_socket_local_port(false, (struct us_socket_t *)listen_socket)
                    << std::endl;
            }
        });
        app.run();
	    std::cout << "Failed to listen on port 3000" << std::endl;
    }
    */

    /*
    TEST_F(TestHttpServer, test_httpserver)
    {
        EXPECT_EQ(true, true);
    }

    TEST_F(TestHttpServer, test_httpserver_interactive)
    {

    }

    TEST_F(TestHttpServer, test_httpserver_file)
    {
        std::string test_dir = sihd::util::Files::combine(_base_test_dir, "file");
        sihd::util::Files::remove_directories(test_dir);
        sihd::util::Files::make_directories(test_dir);
    }
    */
}