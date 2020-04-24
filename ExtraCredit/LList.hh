/*
 * Declarations for linked list and node classes, for use in LRU evictor.
 */

#pragma once
#include "evictor.hh"
#include "cache.hh"
#include <memory>
#include <cassert>
class Node 
{
  public:
    key_type key_;
    std::shared_ptr<Node> prev_;
    std::shared_ptr<Node> next_;
    Node(key_type key, std::shared_ptr<Node> prev, std::shared_ptr<Node> next)
      : key_(key), prev_(prev), next_(next) {}
    ~Node() = default;
};

class LList{
  public:
    std::shared_ptr<Node> root_ = nullptr;
    std::shared_ptr<Node> back_ = nullptr;
    LList() = default;
    ~LList() = default;

};
