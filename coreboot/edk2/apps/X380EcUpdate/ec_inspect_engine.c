/* The released inspection binary cannot use the flash write primitives. */
#define X380_EC_INSPECT_ONLY 1
#include "ec_flash.c"
