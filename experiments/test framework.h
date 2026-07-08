#pragma once
#include <iostream>
#include <string>
#include <vector>

// dont need a real test framework for this, just enough to get pass/fail
// and a message out. every TEST_CASE gets registered into a static list
// and run_all_tests() runs them in whatever order they got registered,
// which is just file order since this is all one translation unit

using TestFn = void(*)();

struct TestCase {
    std::string name;
    TestFn fn;
};

inline std::vector<TestCase>& allTests() {
    static std::vector<TestCase> t;
    return t;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, TestFn fn) {
        allTests().push_back({name, fn});
    }
};

#define TEST_CASE(name) \
    void name(); \
    static TestRegistrar registrar_##name(#name, name); \
    void name()

// not throwing exceptions or anything, just prints and bumps a global
// fail counter. good enough for a spike, would want something real if
// this grows past like 30 tests
inline int g_failCount = 0;
inline int g_passCount = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cout << "  FAIL: " << #cond << " (line " << __LINE__ << ")\n"; \
            g_failCount++; \
        } else { \
            g_passCount++; \
        } \
    } while (0)
