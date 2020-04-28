#include "cache.hh"
#include <cassert>
#include <iostream>
#include <cstring>
#include "catch.hpp"
using size_type = uint32_t;
/*
 * Some basic unit tests for Cache objects.
 * The documentation expected behavior of each method is copied directly from cache.hh
 * There's no test for the constructor / destructor, since these are basic for all other tests
 */

TEST_CASE("Begin testing the cache"){
    Cache c = Cache("127.0.0.1", "65413");
    c.reset();
    std::string key_1 = "Item1";
    std::string key_2 = "Item2";
    std::string key_3 = "Item3";
    const char *val_1 = "314159";
    const char *val_2 = "pi";
    const char *val_3 = "tau2";
    Cache::size_type val_1_size = strlen(val_1) + 1;
    Cache::size_type val_2_size = strlen(val_2) + 1;
    Cache::size_type val_3_size = strlen(val_3) + 1;
    c.set(key_1, val_1, val_1_size);
    c.set(key_2, val_2, val_2_size);

    // Expected behavior for Cache::set(key_type key, val_type val, size_type& val_size)
    // Add a <key, value> pair to the cache.
    // If key already exists, it will overwrite the old value.
    // Both the key and the value are to be deep-copied (not just pointer copied).
    // If maxmem capacity is exceeded, enough values will be removed
    // from the cache to accomodate the new value. If unable, the new value
    // isn't inserted to the cache.

    // Test: setting a key puts it in the cache
    SECTION("Set Key"){
        c.set(key_3, val_3, val_3_size);
        REQUIRE(strcmp(c.get(key_3, val_3_size), val_3) == 0);
    }
    // Test: setting a key already in the cache overwrites the original
    SECTION("Re-set key"){
        c.set(key_1, val_3, val_3_size);
        REQUIRE(strcmp(c.get(key_1, val_3_size), val_3) == 0);
        REQUIRE(strcmp(c.get(key_1, val_1_size), val_1) != 0);
    }

    //Test: set method deep-copies values
    SECTION("Deep Copy Values"){
        REQUIRE(c.get(key_2, val_2_size) != val_2);
    }

    // Test: set deep-copies keys
    SECTION("Deep Copy Keys"){
        key_2.replace(0, key_2.length() + 1, key_1);
        REQUIRE(strcmp(c.get("Item2", val_2_size), val_2) == 0);
    }

    //Test: get sets val_size to actual size found
    SECTION("Get Val Size"){
        c.set(key_3, val_3, val_3_size);
        Cache::size_type val_size = 0;
        c.get("Item3", val_size);
        REQUIRE(val_size == strlen(val_3) + 1);
    }

    // Expected behavior for Cache::get(key_type key, size_type& val_size):
    // Retrieve a pointer to the value associated with key in the cache,
    // or nullptr if not found.
    // Sets the actual size of the returned value (in bytes) in val_size.
    // Const method, so should never change the cache when called.

    // Test: get returns nullptr on a key that's not in the cache
    SECTION("No Key"){
        REQUIRE(c.get(key_3, val_3_size) == nullptr);
    }

    // Test: get retrieves the correct data when the key is in the cache
    SECTION("Correct Mapping"){
        REQUIRE(strcmp(c.get(key_1, val_1_size), val_1) == 0);
    }

    // Expected behavior for Cache::del(key_type key):
    // Delete an object from the cache, if it's still there
    // Should return True if the key was found and deleted

    // Test: delete returns false on a key not in the cache
    SECTION("Delete Non-key"){
        REQUIRE(c.del(key_3) == false);
    }

    // Test: delete returns true on a key in the cache
    SECTION("Bool Delete Key"){
        REQUIRE(c.del(key_1) == true);
    }

    // Test: delete removes a key from the cache
    SECTION("Delete Key"){
        c.del(key_2);
        REQUIRE(c.get(key_2, val_2_size) == nullptr);
    }

    // Expected behavior for Cache::space_used():
    // Compute the total amount of memory used up by all cache values (not keys)

    // Test: space_used should be the sum of the sizes of the two values in cache
    SECTION("Space Used"){
        REQUIRE(c.space_used() == val_1_size + val_2_size);
    }

    // Test: Removing a value reduces cache size by size of that value
    SECTION("Decrease Space Used"){
        c.del(key_1);
        REQUIRE(c.space_used() == val_2_size);
    }

    // Test: space_used changes if new items are set into the cache
    SECTION("Increase Space Used"){
        c.set(key_3, val_3, val_3_size);
        REQUIRE(c.space_used() == val_1_size + val_2_size + val_3_size);
    }

    // Expected behavior for Cache::reset():
    // Delete all data from the cache

    // Test: after reset, space_used is 0
    SECTION("Space Used After Reset"){
        c.reset();
        REQUIRE(c.space_used() == 0);
    }

    // Test: after reset, all items previously set are absent
    SECTION("Reset Empties Cache"){
        c.reset();
        REQUIRE(c.get(key_1, val_1_size) == nullptr);
        REQUIRE(c.get(key_2, val_2_size) == nullptr);
    }

    // Test: after reset, new keys can still be added to the cache
    SECTION("Set After Reset"){
        c.reset();
        c.set(key_3, val_3, val_3_size);
        REQUIRE(c.get(key_3, val_3_size) != nullptr);
    }
}
