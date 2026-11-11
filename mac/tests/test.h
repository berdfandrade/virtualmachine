/*
 * test.h - a tiny, dependency free unit test framework.
 *
 *   TEST(name) { CHECK(cond); CHECK_EQ(a, b); }
 *   int main(void) { RUN(name); return test_summary(); }
 */
#ifndef MAC_TEST_H
#define MAC_TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures;
static int test_checks;
static int test_count;
static const char *test_current;

#define TEST(name) static void name(void)

#define RUN(name) do {                         \
        int before_ = test_failures;           \
        test_current = #name;                  \
        test_count++;                          \
        name();                                \
        if (test_failures == before_)          \
            printf("  ok    %s\n", #name);     \
    } while (0)

#define FAIL_AT(...) do {                                          \
        printf("  FAIL  %s (%s:%d): ", test_current, __FILE__, __LINE__); \
        printf(__VA_ARGS__);                                       \
        printf("\n");                                              \
        test_failures++;                                           \
    } while (0)

#define CHECK(cond) do {                       \
        test_checks++;                         \
        if (!(cond))                           \
            FAIL_AT("%s", #cond);              \
    } while (0)

#define CHECK_EQ(a, b) do {                                        \
        long long a_ = (long long)(a), b_ = (long long)(b);        \
        test_checks++;                                             \
        if (a_ != b_)                                              \
            FAIL_AT("%s == %s (%lld != %lld)", #a, #b, a_, b_);    \
    } while (0)

#define CHECK_STR(a, b) do {                                       \
        const char *a_ = (a), *b_ = (b);                           \
        test_checks++;                                             \
        if (strcmp(a_, b_) != 0)                                   \
            FAIL_AT("%s == \"%s\" (got \"%s\")", #a, b_, a_);      \
    } while (0)

static int test_summary(const char *suite)
{
    printf("%s: %d tests, %d checks, %d failures\n",
           suite, test_count, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

#endif /* MAC_TEST_H */
