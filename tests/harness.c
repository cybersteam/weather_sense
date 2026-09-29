#include "harness.h"

#include <stdio.h>
#include <string.h>

static int g_failed;
static int g_passed;

void ws_expect(int cond, const char *file, int line, const char *expr)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
        g_failed++;
        return;
    }
    g_passed++;
}

void ws_expect_eq_i32(int32_t got, int32_t want, const char *file, int line, const char *expr)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s:%d: %s got %ld want %ld\n", file, line, expr, (long)got,
                (long)want);
        g_failed++;
        return;
    }
    g_passed++;
}

void ws_expect_near_i32(int32_t got, int32_t want, int32_t tol, const char *file, int line,
                        const char *expr)
{
    int32_t delta = got - want;
    if (delta < 0) {
        delta = -delta;
    }
    if (delta > tol) {
        fprintf(stderr, "FAIL %s:%d: %s got %ld want %ld tol %ld\n", file, line, expr, (long)got,
                (long)want, (long)tol);
        g_failed++;
        return;
    }
    g_passed++;
}

void ws_expect_str(const char *got, const char *want, const char *file, int line)
{
    if (got == NULL || want == NULL || strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s:%d:\n  got  [%s]\n  want [%s]\n", file, line,
                got != NULL ? got : "(null)", want != NULL ? want : "(null)");
        g_failed++;
        return;
    }
    g_passed++;
}

int ws_test_failures(void)
{
    return g_failed;
}

int ws_test_passes(void)
{
    return g_passed;
}
