/*
 * Executable with multiverse attributes, but without libmultiverse
 */

#include <stdio.h>
#ifdef MVINSTALLED
#include <multiverse.h>
#else
#include "multiverse.h"
#endif

typedef enum {false, true} bool;

__attribute__((multiverse)) bool config_first = false; // NOLINT
__attribute__((multiverse)) bool config_second = true; // NOLINT
__attribute__((multiverse)) bool config_third = false; // NOLINT

void __attribute__((multiverse)) func_first() // NOLINT
{
    if (config_first)
        puts("config_first = true");
    else
        puts("config_first = false");
}

void __attribute__((multiverse)) func_second() // NOLINT
{
    if (config_second)
        puts("config_second = true");
    else
        puts("config_second = false");
}

void __attribute__((multiverse)) func_third() // NOLINT
{
    if (config_third)
        puts("config_third = true");
    else
        puts("config_third = false");
}

int main()
{
    multiverse_init();

    func_first();
    func_second();
    func_third();

    multiverse_dump_info();

    return 0;
}
