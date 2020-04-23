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
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/beast/http/fields.hpp>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <string.h>
#include <cassert>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


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
    Send&& send,
    std::shared_ptr<Cache> cache,
    std::mutex mutx_)
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
      std::shared_lock guard(mutx_)
      if (req.method() == http::verb::head)
      {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        auto used = std::to_string(cache->space_used());
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
        auto used = std::to_string(cache->space_used());
        res.set("Space-Used", used);

        key_type key = req.target().to_string().substr(1);
        Cache::size_type size = 0;
        auto got = cache->get(key, size);
        if (got == nullptr){
          res.result(http::status::not_found);
          res.body() = "Key not in cache\n"; // or some other error message
        } else {
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

        cache->set(key, val, size);
        delete[] val;
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        auto used = std::to_string(cache->space_used());
        res.set("Space-Used", used);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      else if (req.method() == http::verb::delete_)
      {
        key_type key = req.target().to_string();
        key.erase(key.begin());
        bool b = cache->del(key);
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        auto used = std::to_string(cache->space_used());
        res.set("Space-Used", used);
        std::string strBool = "false";
        if (b) strBool = "true";
        res.set("Delete-Bool", strBool);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
      }

      else if (req.method() == http::verb::post)
      // reset the cache
      {
        http::response<http::empty_body> res{http::status::not_found, req.version()};
        if (req.target() == "/reset") 
        { 
          cache->reset();
          res.result(http::status::ok);
        }
        res.set(http::field::content_type, "application/json");
        res.set(http::field::accept, "text/html");
        auto used = std::to_string(cache->space_used());
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
class session : public std::enable_shared_from_this<session>
{
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit
        send_lambda(session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                beast::bind_front_handler(
                    &session::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<Cache> cache_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;
    mutable std::mutex mutx_;

public:
    // Take ownership of the stream
    session(
        tcp::socket&& socket,
        std::shared_ptr<Cache> cache,
        std::mutex mutx)
        : stream_(std::move(socket))
        , cache_(cache)
        , lambda_(*this)
        , mutx_(mutx)
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(
                          &session::do_read,
                          shared_from_this()));
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return fail(ec, "read");

        // Send the response
        handle_request(std::move(req_), lambda_, cache_, mutx_);
    }

    void
    on_write(
        bool close,
        beast::error_code ec,
        std::size_t bytes_transferred)
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

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
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
    std::shared_ptr<Cache> cache_;
    unsigned messages_sent_ = 0; // edits for purposes of valgrind tests
    unsigned MAX_MESSAGES_ = 5; //
    mutable std::mutex mutx_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<Cache> cache,
        std::mutex mutx_)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , cache_(cache)
        , mutx_(mutx)
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

            // check if more messages have been sent than the specified maximum, and end if this is the case
            // (only for purposes of valgrind tests)
            // Dont forget to uncomment the } on line with ***************
            /*messages_sent_++;
            if (messages_sent_ > MAX_MESSAGES_){
              ioc_.stop();
              return;
            } else {*/


            // Create the session and run it
            std::make_shared<session>(
                std::move(socket),
                cache_,
                mutx_)->run();
            //} //don't forget this to un-comment } ******************
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
  Cache::size_type maxmem = 10; 
  int threads = 1;
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
      threads = std::atoi(optarg);
      break;
    }
  }
  std::cout << "maxmem: " << maxmem 
              << ", threads: " << threads
              << ", server: " << server
              << ", port: " << port << std::endl;

  net::io_context ioc(threads); // number of threads goes here {n}

  Evictor* fifo = new Fifo_Evictor();

  auto cache = std::make_shared<Cache>(maxmem, 0.75, fifo);

  std::mutex mutx;

  auto run_one_thread = [&]() 
  {
      std::make_shared<listener>(ioc,
                             tcp::endpoint{server, port},
                             cache, mutx)->run();
  };
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < nthreads; ++i) 
  {
    threads.push_back(std::thread(run_one_thread));
  }

  for (auto& t : threads) 
  {
    t.join();
  }

  ioc.run();

  return 0;
}

