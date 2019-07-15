// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#undef NDEBUG // ensure that asserts are never compiled out
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#if defined(__LINUX__)
#include <unistd.h>
#else
#include <windows.h>
#endif


#include "local_cache.h"
#include "UnitTests/unit_test.h"

static time_t now() { return time(nullptr); }

//
// Add an item to the cache and retrieve it.
//
static void AddGetItem()
{
    TEST_START();

    static const std::vector<uint8_t> data = { 8, 6, 7, 5, 3, 0, 9};
    local_cache_add(__FUNCTION__, now() + 60, data.size(), data.data());

    auto retrieved = local_cache_get(__FUNCTION__);
    assert(retrieved != nullptr);
    assert(*retrieved == data);

    TEST_PASSED();
}

//
// Add an item to the cache, then overwrite it
//
static void OverwriteCacheEntry()
{
    TEST_START();

    static const std::vector<uint8_t> data1 = { 1, 1, 1, 1 };
    local_cache_add(__FUNCTION__, now() + 60, data1.size(), data1.data());

    static const std::vector<uint8_t> data2 = { 2 };
    local_cache_add(__FUNCTION__, now() + 60, data2.size(), data2.data());

    auto retrieved = local_cache_get(__FUNCTION__);
    assert(*retrieved == data2);

    TEST_PASSED();
}

//
// Add an item to the cache, clear the cache, then ensure the entry is gone
//
static void VerifyClearCache()
{
    TEST_START();

    static const uint8_t data[] = "stuff goes here";
    local_cache_add(__FUNCTION__, now() + 60, sizeof(data), data);

    assert(nullptr != local_cache_get(__FUNCTION__));
    local_cache_clear();

	assert(nullptr == local_cache_get(__FUNCTION__));

    TEST_PASSED();
}

//
// Add an item to the cache with a short expiry, then ensure it actually expires.
//
static void VerifyExpiryWorks()
{
    TEST_START();

    int expiry_seconds = 2;

    // add an entry with expiry shortly in the future
    static const uint8_t data[] = "stuff goes here";
    local_cache_add(__FUNCTION__, now() + expiry_seconds, sizeof(data), data);

    // ensure the data is there
    assert(nullptr != local_cache_get(__FUNCTION__));

    // wait for expiry, after which the data should be gone

#if defined(__LINUX__)
	sleep(expiry_seconds);
#else
	Sleep(expiry_seconds * 1000);
#endif

    assert(nullptr == local_cache_get(__FUNCTION__));

    TEST_PASSED();
}

template <typename ExceptionT>
static void AssertException(void (*function)())
{
    try
    {
        function();
    }
    catch (ExceptionT&)
    {
        return;
    }

    assert(!"Expected exception was not thrown");
}

//
// Verify invalid parameter handling
//
static void InvalidParams()
{
    TEST_START();

    static const uint8_t data[] = "test data";

    AssertException<std::runtime_error>(
        [] { local_cache_add("", now(), sizeof(data), data); });

    AssertException<std::runtime_error>(
        []{ local_cache_add(__FUNCTION__, now(), 0, data); });

    AssertException<std::runtime_error>(
        []{ local_cache_add(__FUNCTION__, now(), sizeof(data), nullptr); });

    AssertException<std::runtime_error>([]{ local_cache_get(""); });

    TEST_PASSED();
}

//
// Spawns multiple threads, intentionally creating a lot of contention for
// a single cache entry.
//
#if defined(__LINUX__)
void cache_writer(int id)
{
    std::vector<uint8_t> data(64 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = i & 0xff;
    }

    constexpr unsigned THREAD_LOOP_COUNT = 128;
    const std::string ID = "data identifier";

    for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
    {
        local_cache_add(ID, now() + 60, data.size(), data.data());
    }
}

void cache_reader(int id)
{
    std::vector<uint8_t> data(64 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = i & 0xff;
    }

    constexpr unsigned THREAD_LOOP_COUNT = 128;
    const std::string ID = "data identifier";

    for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
    {
        auto retrieved = local_cache_get(ID);
        assert(retrieved != nullptr);
        assert(*retrieved == data);
    }
}
static void ThreadSafetyTest()
{
    TEST_START();

    // Pre-fill a data vector that's of sufficient size that we
    // are likely to get conflicts between threads.
    std::vector<uint8_t> data(64 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = i & 0xff;
    }

    const std::string ID = "data identifier";
/*
    auto cache_writer = [&](int id) {
        for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
        {
            local_cache_add(ID, now() + 60, data.size(), data.data());
        }
    };

    auto cache_reader = [&](int id) {
        for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
        {
            auto retrieved = local_cache_get(ID);
            assert(retrieved != nullptr);
            assert(*retrieved == data);
        }
    };
*/
    // prime the cache entry first (in case a read thread runs first)
    local_cache_add(ID, now() + 60, data.size(), data.data());

    std::array<std::thread, 8> threads;
    for (size_t i = 0; i < threads.size(); ++i)
    {
        if (i & 1)
        {
            threads[i] = std::thread(cache_writer, i);
        }
        else
        {
            threads[i] = std::thread(cache_reader, i);
        }
    }

    for(auto& t : threads)
    {
        t.join();
    }

    TEST_PASSED();
}

#else
void cache_writer(int id)
{
    std::vector<uint8_t> data(64 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = i & 0xff;
    }

    constexpr unsigned THREAD_LOOP_COUNT = 128;
    const std::string ID = "data identifier";

    for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
    {
        local_cache_add(ID, now() + 60, data.size(), data.data());
    }
}

void cache_reader(int id)
{
    std::vector<uint8_t> data(64 * 1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = i & 0xff;
    }

    constexpr unsigned THREAD_LOOP_COUNT = 128;
    const std::string ID = "data identifier";

    for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
    {
        auto retrieved = local_cache_get(ID);
        assert(retrieved != nullptr);
        assert(*retrieved == data);
    }
}
static void ThreadSafetyTest()
{
	TEST_START();

	// Pre-fill a data vector that's of sufficient size that we
	// are likely to get conflicts between threads.
	std::vector<uint8_t> data(64 * 1024);
	for (size_t i = 0; i < data.size(); ++i)
	{
		data[i] = i & 0xff;
	}
	//constexpr unsigned THREAD_LOOP_COUNT = 128;
	const std::string ID = "data identifier";
/*
	auto cache_writer = [&](int id) {
		for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
		{
			local_cache_add(ID, now() + 60, data.size(), data.data());
		}
	};

	auto cache_reader = [&](int id) {
		for (unsigned i = 0; i < THREAD_LOOP_COUNT; ++i)
		{
			auto retrieved = local_cache_get(ID);
			assert(retrieved != nullptr);
			assert(*retrieved == data);
		}
	};
*/
	// prime the cache entry first (in case a read thread runs first)
	local_cache_add(ID, now() + 60, data.size(), data.data());

	std::thread threads[8];

	threads[0] = std::thread(cache_reader, 0);
	threads[1] = std::thread(cache_writer, 1);
	threads[2] = std::thread(cache_reader, 2);
	threads[3] = std::thread(cache_reader, 3);
	threads[4] = std::thread(cache_writer, 4);
	threads[5] = std::thread(cache_reader, 5);
	threads[6] = std::thread(cache_writer, 6);
	threads[7] = std::thread(cache_reader, 7);

	/*
	int temp = sizeof(threads) / sizeof(*threads);
	for (size_t i = 0; i < temp; ++i)
	{
		if (i & 1)
		{
			threads[i] = std::thread(cache_writer, i);
		}
		else
		{
			threads[i] = std::thread(cache_reader, i);
		}
	}
	*/

	threads[0].join();
	threads[1].join();
	threads[2].join();
	threads[3].join();
	threads[4].join();
	threads[5].join();
	threads[6].join();
	threads[7].join();


	//for(auto& t : threads)
	//{
	//	t.join();
	//}

	TEST_PASSED();
}
#endif

extern void LocalCacheTests()
{
    local_cache_clear();

    AddGetItem();
    OverwriteCacheEntry();
    VerifyClearCache();
    VerifyExpiryWorks();
    InvalidParams();
    ThreadSafetyTest();
}
