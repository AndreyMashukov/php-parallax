#ifndef PARALLAX_TESTS_HARNESS_H
#define PARALLAX_TESTS_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((unused)) static int harness_failures = 0;
__attribute__((unused)) static int harness_checks   = 0;

#define ASSERT_TRUE(expr) do {                                                \
	harness_checks++;                                                         \
	if (!(expr)) {                                                            \
		fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);     \
		harness_failures++;                                                   \
	}                                                                         \
} while (0)

#define ASSERT_EQ_INT(a, b) do {                                              \
	harness_checks++;                                                         \
	long long _a = (long long)(a);                                            \
	long long _b = (long long)(b);                                            \
	if (_a != _b) {                                                           \
		fprintf(stderr, "  FAIL %s:%d  %s != %s (%lld vs %lld)\n",            \
			__FILE__, __LINE__, #a, #b, _a, _b);                              \
		harness_failures++;                                                   \
	}                                                                         \
} while (0)

#define ASSERT_EQ_STR(a, alen, b, blen) do {                                  \
	harness_checks++;                                                         \
	size_t _alen = (size_t)(alen);                                            \
	size_t _blen = (size_t)(blen);                                            \
	if (_alen != _blen) {                                                     \
		fprintf(stderr, "  FAIL %s:%d  length %zu != %zu\n",                  \
			__FILE__, __LINE__, _alen, _blen);                                \
		harness_failures++;                                                   \
	} else if (_alen > 0 && memcmp((a), (b), _alen) != 0) {                   \
		fprintf(stderr, "  FAIL %s:%d  bytes differ\n", __FILE__, __LINE__);  \
		harness_failures++;                                                   \
	}                                                                         \
} while (0)

#define ASSERT_NOT_NULL(p) ASSERT_TRUE((p) != NULL)
#define ASSERT_NULL(p)     ASSERT_TRUE((p) == NULL)

#define RUN_TEST(fn) do {                                                     \
	int before = harness_failures;                                            \
	fprintf(stderr, "* %s\n", #fn);                                           \
	fn();                                                                     \
	if (harness_failures > before) {                                          \
		fprintf(stderr, "  -> FAILED (%d new)\n",                             \
			harness_failures - before);                                       \
	}                                                                         \
} while (0)

#define HARNESS_REPORT() do {                                                 \
	fprintf(stderr, "\n%d checks, %d failures\n",                             \
		harness_checks, harness_failures);                                    \
	return harness_failures == 0 ? 0 : 1;                                     \
} while (0)

#endif /* PARALLAX_TESTS_HARNESS_H */
