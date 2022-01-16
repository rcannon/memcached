/*
 * Implementaion for networked interface with cache.hh
 * Uses the pImpl idiom to hide details from the user.
 */
#include <utility>
#include <memory>
#include <cassert>
#include <string.h>
#include <iostream>
#include "cache.hh"
#include "fifo_evictor.hh"
#include "lru_evictor.hh"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
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
#include <thread>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class Cache::Impl 
{
  private:
    const std::string host_;
    const std::string port_;
    unsigned version_ = 11;
    net::io_context ioc_;
    mutable tcp::resolver resolver_;
    mutable beast::tcp_stream stream_;

  public:

    Impl(std::string host, std::string port);
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    void set(key_type key, Cache::val_type val, Cache::size_type size);
    std::pair<Cache::val_type, Cache::size_type> parse_get(const std::string jstring) const;
    Cache::val_type get(key_type key, Cache::size_type& val_size) const;
    bool del(key_type key);
    Cache::size_type space_used() const;
    void reset();
};

Cache::Impl::Impl(std::string host, std::string port)
  : host_(host), port_(port), ioc_(net::io_context()), resolver_(tcp::resolver(ioc_)), stream_(beast::tcp_stream(ioc_))
{}


  // Constructor for networked cache client, only defined in cache_client.cc
Cache::Cache(std::string host, std::string port)
  : pImpl_(new Cache::Impl(host, port))
{}

Cache::Impl::~Impl()
{
  beast::error_code ec;
  stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
}

Cache::~Cache(){}


  // Add a <key, value> pair to the cache.
  // If key already exists, it will overwrite the old value.
  // Both the key and the value are deep-copied.
  // If maxmem capacity is exceeded, enough values will be removed
  // from the cache to accomodate the new value. If unable, the new value
  // isn't inserted to the cache and no values are removed.
void 
Cache::Impl::set(key_type key, Cache::val_type val, Cache::size_type size)
{
  auto const results = resolver_.resolve(host_, port_);
  stream_.connect(results);

  // Set up an HTTP PUT request message and send
  std::string target = "/" + key + "/" + val;

  http::request<http::string_body> req{http::verb::put, target, 11};
  req.set("Size", size);
  req.keep_alive(true);
  http::write(stream_, req);

  // get the response
  beast::flat_buffer buffer;
  http::response<http::string_body> res = {}; 
  http::read(stream_, buffer, res);

}

// parse the json string to get the val and size
std::pair<Cache::val_type, Cache::size_type> 
Cache::Impl::parse_get(const std::string jstring) const
{
  std::string strval = jstring.substr(jstring.rfind(":")+3);
  strval.pop_back();
  strval.pop_back();
  Cache::size_type size = strval.length()+1;
  const auto sval = strval.c_str();
  auto val = new char[size];
  std::copy(sval, sval+size, val);
  return std::make_pair(val, size);
}


  // Retrieve a pointer to the value associated with key in the cache,
  // or nullptr if not found.
  // Sets the actual size of the returned value (in bytes) in val_size.
Cache::val_type
Cache::Impl::get(key_type key, Cache::size_type& val_size) const
{
  auto const results = resolver_.resolve(host_, port_);
  stream_.connect(results);

  // Set up an HTTP GET request message and send
  std::string target = "/" + key;
  http::request<http::string_body> req{http::verb::get, target, 11};
  req.keep_alive(true);
  http::write(stream_, req);

  // 
  // get the response
  beast::flat_buffer buffer;
  http::response<http::string_body> res = {};
  http::read(stream_, buffer, res);

  // parse the jstring to get the value
  // now get the size
  if (res.result() == http::status::not_found)
  {
    assert(res.body() ==  "Key not in cache\n");
    return nullptr;
  }
  auto jstring = res.body();
  auto val_and_size_pair = parse_get(jstring);
  val_size = val_and_size_pair.second;
  Cache::val_type val = val_and_size_pair.first;
  return val;
}


  // Delete an object from the cache, if it's still there
bool 
Cache::Impl::del(key_type key)
{
  auto const results = resolver_.resolve(host_, port_);
  stream_.connect(results);

  // Set up an HTTP GET request message and send
  std::string target = "/" + key;
  http::request<http::string_body> req{http::verb::delete_, target, 11};
  req.keep_alive(true);
  http::write(stream_, req);

  // 
  // get the response
  beast::flat_buffer buffer;
  http::response<http::string_body> res = {};
  http::read(stream_, buffer, res);

  // bool tells us if value existed before deletion
  auto strBool = res.at("Delete-Bool");
  bool delBool = (strBool == "true");

  return delBool;
}

  // Compute the total amount of memory used up by all cache values (not keys)
Cache::size_type 
Cache::Impl::space_used() const
{
  auto const results = resolver_.resolve(host_, port_);
  stream_.connect(results);

  // Set up an HTTP GET request message and send
  std::string target = "/";
  http::request<http::string_body> req{http::verb::head, target, 11};
  req.keep_alive(true);
  http::write(stream_, req);

  // get the response
  beast::flat_buffer buffer;
  http::response<http::empty_body> res;
  http::read(stream_, buffer, res);

  // get the space used
  auto strInt = res.at("Space-Used").to_string();
  auto used = std::stoi(strInt);

  return used;
}

  // Delete all data from the cache
void
Cache::Impl::reset()
{
  // setup connection
  auto const results = resolver_.resolve(host_, port_);
  stream_.connect(results);

  // Set up an HTTP GET request message and send
  std::string target = "/reset";
  http::request<http::string_body> req{http::verb::post, target, 11};
  req.keep_alive(true);
  http::write(stream_, req);

  // get the response
  beast::flat_buffer buffer;
  http::response<http::empty_body> res;
  http::read(stream_, buffer, res);
  assert(res.result() == http::status::ok);
}

/* here are the cache methods, all they do is call the corresponding Impl methods */
void Cache::set(key_type key, Cache::val_type val, Cache::size_type size)
{
  return pImpl_->set(key, val, size);
}

Cache::val_type Cache::get(key_type key, Cache::size_type& val_size) const
{
  return pImpl_->get(key, val_size);
}

bool Cache::del(key_type key)
{
  return pImpl_->del(key);
}

Cache::size_type Cache::space_used() const
{
  return pImpl_->space_used();
}

void Cache::reset()
{
  return pImpl_->reset();
}
