// Code for an asynchronous cache server. Started from this example code:
// https://www.boost.org/doc/libs/1_72_0/libs/beast/example/http/server/async/http_server_async.cpp
// We made major edits in handle_request (which controls how the server operates with clients) but the listener and the session handler are largely unchanged.

#include "cache.hh"
#include "lru_evictor.hh"
#include "fifo_evictor.hh"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/beast/http/fields.hpp>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <string.h>
#include <cassert>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//std::mutex mutx;
// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Allocator,
    class Send>
void
handle_request(
    http::request<http::string_body, http::basic_fields<Allocator>>&& req,
    Send&& send, Cache& cache)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
      http::response<http::string_body> res{http::status::bad_request, req.version()};
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "text/html");
      res.keep_alive(req.keep_alive());
      res.body() = std::string(why);
      res.prepare_payload();
      return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
      req.method() != http::verb::head &&
      req.method() != http::verb::put &&
      req.method() != http::verb::delete_ &&
      req.method() != http::verb::post)
      return send(bad_request("Unknown HTTP-method"));

    {
      if (req.method() == http::verb::head)
      {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        const auto used = std::to_string(cache.space_used());
        res.set("Space-Used", used);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      // Respond to GET request
      else if (req.method() == http::verb::get)
      {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        const auto used = std::to_string(cache.space_used());
        res.set("Space-Used", used);

        key_type key = req.target().to_string().substr(1);
        Cache::size_type size = 0;
        const auto got = cache.get(key, size);
        if (got == nullptr)
        {
          res.result(http::status::not_found);
          res.body() = "Key not in cache\n"; // or some other error message
        } 
        else 
        {
          assert(got != nullptr);
          res.body() = "{ \"key\" : \"" + key + "\", \"value\" : \"" + got + "\"}";
        }
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }


      else if (req.method() == http::verb::put)
      {
        // get the key-value pair
        std::string kvp = req.target().to_string().substr(1);

        // get the key
        key_type key = kvp.substr(0, kvp.find("/"));

        // get the value
        const std::string strval = kvp.substr(kvp.find("/")+1);
        const Cache::size_type size = strval.length()+1;
        const auto sval = strval.c_str();
        assert(size == (strlen(sval)+1));
        auto val = new char[size];
        std::copy(sval,sval+size, val);
        //{
        //std::scoped_lock guard(mutx);
        cache.set(key, val, size);
        //}
        delete[] val;
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        
        const auto used = std::to_string(cache.space_used());
        res.set("Space-Used", used);

        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      else if (req.method() == http::verb::delete_)
      {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");

        key_type key = req.target().to_string();
        key.erase(key.begin());
        std::string strBool;
        //{
        //std::scoped_lock guard(mutx);
        const bool b = cache.del(key);
        strBool = "false";
        if (b) strBool = "true";
        res.set("Delete-Bool", strBool);
        //}
        const auto used = std::to_string(cache.space_used());
        res.set("Space-Used", used);
        
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      else if (req.method() == http::verb::post)
      // reset the cache
      {
        http::response<http::empty_body> res{http::status::not_found, req.version()};
        if (req.target() == "/reset") 
        { 
          //std::scoped_lock guard(mutx);
          cache.reset();
          res.result(http::status::ok);
        }
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        const auto used = std::to_string(cache.space_used());
        res.set("Space-Used", used);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }
      else { return send(bad_request("Do better next time")); }
    }

}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session>
{
    // This queue is used for HTTP pipelining.
    class queue
    {
        enum
        {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work
        {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        http_session& self_;
        std::vector<std::unique_ptr<work>> items_;

    public:
        explicit
        queue(http_session& self)
            : self_(self)
        {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool
        is_full() const
        {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool
        on_write()
        {
            BOOST_ASSERT(! items_.empty());
            auto const was_full = is_full();
            items_.erase(items_.begin());
            if(! items_.empty())
                (*items_.front())();
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg)
        {
            // This holds a work item
            struct work_impl : work
            {
                http_session& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(
                    http_session& self,
                    http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg))
                {
                }

                void
                operator()()
                {
                    http::async_write(
                        self_.stream_,
                        msg_,
                        beast::bind_front_handler(
                            &http_session::on_write,
                            self_.shared_from_this(),
                            msg_.need_eof()));
                }
            };

            // Allocate and store the work
            items_.push_back(
                boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if(items_.size() == 1)
                (*items_.front())();
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    Cache& cache_;
    queue queue_;
    http::request<http::string_body> req_;


public:
    // Take ownership of the socket
    http_session(
        tcp::socket&& socket,
        Cache& cache)
        : stream_(std::move(socket))
        , cache_(cache)
        , queue_(*this)
    {
    }

    // Start the session
    void
    run()
    {
        do_read();
    }

private:
    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));
        // Read a request using the parser-oriented interface
        http::async_read(
            stream_,
            buffer_,
            req_,
            beast::bind_front_handler(
                &http_session::on_read,
                shared_from_this()));
    }

    void
    on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return fail(ec, "read");


        // Send the response
        handle_request(std::move(req_), queue_, cache_);

        // If we aren't at the queue limit, try to pipeline another request
        if(! queue_.is_full())
            do_read();
    }

    void
    on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Inform the queue that a write completed
        if(queue_.on_write())
        {
            // Read another request
            do_read();
        }
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    Cache& cache_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        Cache& cache)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , cache_(cache)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the http session and run it
            std::make_shared<http_session>(
                std::move(socket),
                cache_)->run();
        }

        // Accept another connection
        do_accept();
    }
};
//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
  Cache::size_type maxmem = 10; 
  int nthreads = 1;
  unsigned short port = 65413; 
  auto server = net::ip::make_address("127.0.0.1");
  int opt;
  while ((opt = getopt(argc, argv, "m:s:p:t:")) != -1) 
  {
    switch (opt) 
    {
    case 'm':
      maxmem = std::atoi(optarg);
      break;
    case 's':
      server = net::ip::make_address(optarg);
      break;
    case 'p':
      port = static_cast<unsigned short>(std::atoi(optarg));
      break;
    case 't':
      nthreads = std::atoi(optarg);
      break;
    }
  }
  std::cout << "maxmem: " << maxmem 
              << ", threads: " << nthreads
              << ", server: " << server
              << ", port: " << port << std::endl;

  net::io_context ioc{nthreads}; // number of threads goes here {n}

  //Evictor* fifo = new Fifo_Evictor();
  Cache cache(maxmem, 0.75);


  std::make_shared<listener>(ioc,
                             tcp::endpoint{server, port},
                             cache)->run();

  net::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait(
        [&](beast::error_code const&, int)
        {
            // Stop the `io_context`. This will cause `run()`
            // to return immediately, eventually destroying the
            // `io_context` and all of the sockets in it.
            ioc.stop();
        });

  
  std::vector<std::thread> v;
  v.reserve(nthreads - 1);
  for(auto i = nthreads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
  ioc.run();

  for(auto& t : v)
        t.join();

  return 0;
}

