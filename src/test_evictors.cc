#include "fifo_evictor.hh"
#include "lru_evictor.hh"
#include "catch.hpp"
#include <iostream>
#include <cstring>
using size_type = uint32_t;
/*
 * Some basic unit tests for a FIFO evictor
 */


TEST_CASE("fifo"){
    // Expected behavior for Cache::set when using a fifo evictor:
    // When not enough memory remains to store a value, the oldest items will be thrown out one by one until
    // enough space is freed to store the new value.
    // When attempting to store a value that is too large for the cache to store even with no other space used,
    // should fail silently without removing anything.
    Evictor* fifo = new Fifo_Evictor();
    std::string key_1 = "Item 1";
    std::string key_2 = "Item 2";
    std::string key_3 = "Item 3";
    
    // Test: evicting with no keys touched returns empty key
    SECTION("Evict On Empty"){
        REQUIRE(fifo->evict() == "");
    }

    // Test: putting a key in first evicts key
    SECTION("Evict One Key"){
        fifo->touch_key(key_1);
        fifo->touch_key(key_2);
        fifo->touch_key(key_3);
        REQUIRE(fifo->evict() == key_1);
    }

    // Test: putting second key in results in second key evicted after first eviction
    SECTION("Evict Two Keys"){
        fifo->touch_key(key_1);
        fifo->touch_key(key_2);
        fifo->touch_key(key_3);
        fifo->evict();
        REQUIRE(fifo->evict() == key_2);
    }

    // Test: touching first key in evictor a secind time does not change order of eviction
    SECTION("Touch After Insertion"){
        fifo->touch_key(key_1);
        fifo->touch_key(key_2);
        fifo->touch_key(key_3);
        fifo->touch_key(key_1);
        REQUIRE(fifo->evict() == key_1);
    }
    delete fifo;
}

/*
 * Some basic unit tests for an LRU evictor.
 */

TEST_CASE("lru"){
    // Expected behavior for an lru evictor:
    // Least recently touched item should be offered when evict() is called
    // empty evictor should return "" from evict()
    auto lru = LRU_Evictor();
    std::string key_1 = "Item 1";
    std::string key_2 = "Item 2";
    std::string key_3 = "Item 3";
    std::string key_4 = "Item 4";
    lru.touch_key(key_1);
    lru.touch_key(key_2);
    lru.touch_key(key_3);
    lru.touch_key(key_4);
    // Test: Evict One Key
    SECTION("the first key inserted is evicted first if no other touches are made"){
        REQUIRE(lru.evict() == key_1);
    }
    // Test: Last Touched
    SECTION("the last key touched is the last evicted"){
        lru.evict();
        lru.evict();
        lru.evict();
        REQUIRE(lru.evict() == key_4);
    }
    // Test: evictor returns empty key when there is nothing to evict
    SECTION("Evict On Empty"){
        lru.evict();
        lru.evict();
        lru.evict();
        lru.evict();
        REQUIRE(lru.evict() == "");
    }
    // Test: touching all but one key sets the remaining key as next to evict
    SECTION("Many Touches"){
        lru.touch_key(key_1);
        lru.touch_key(key_3);
        lru.touch_key(key_4);
        REQUIRE(lru.evict() == key_2);
    }
    // Test: touching first key again causes second to be evicted first
    SECTION("Touch First"){
      lru.touch_key(key_1);
      REQUIRE(lru.evict() == key_2);
    }
}
