#pragma once

#include <stdint.h>

void ws_expect(int cond, const char *file, int line, const char *expr);
void ws_expect_eq_i32(int32_t got, int32_t want, const char *file, int line, const char *expr);
void ws_expect_near_i32(int32_t got, int32_t want, int32_t tol, const char *file, int line,
                        const char *expr);
void ws_expect_str(const char *got, const char *want, const char *file, int line);

#define EXPECT(cond) ws_expect(!!(cond), __FILE__, __LINE__, #cond)
#define EXPECT_EQ(got, want) ws_expect_eq_i32((int32_t)(got), (int32_t)(want), __FILE__, __LINE__, #got)
#define EXPECT_NEAR(got, want, tol) \
    ws_expect_near_i32((int32_t)(got), (int32_t)(want), (int32_t)(tol), __FILE__, __LINE__, #got)
#define EXPECT_STR(got, want) ws_expect_str((got), (want), __FILE__, __LINE__)

int ws_test_failures(void);
int ws_test_passes(void);
