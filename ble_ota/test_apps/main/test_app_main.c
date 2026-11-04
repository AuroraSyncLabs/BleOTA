/*
** Julien ROIRON, 2026
** test_app_main.c
** File description:
** Entry point of the unit test app
*/

#include <gcov.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

extern const struct gcov_info *const _gcov_info_start[];
extern const struct gcov_info *const _gcov_info_end[];

static void dump(const void *data, unsigned int len, void *arg)
{
    for (const unsigned char *byte = data; len > 0; len--)
        printf("%02x", *byte++);
}

static void dump_filename(const char *name, void *arg)
{
    __gcov_filename_to_gcfn(name, dump, arg);
}

static void *allocate(unsigned int len, void *arg)
{
    return malloc(len);
}

/* Prints the .gcda files as a hex stream for gcov-tool merge-stream (see pytest_ble_ota.py). */
TEST_CASE("dump the coverage data", "[coverage]")
{
    printf("gcov start\n");
    for (const struct gcov_info *const *info = _gcov_info_start; info < _gcov_info_end; info++)
        __gcov_info_to_gcda(*info, dump_filename, dump, allocate, NULL);
    printf("\ngcov end\n");
}

void app_main(void)
{
    unity_run_menu();
}
