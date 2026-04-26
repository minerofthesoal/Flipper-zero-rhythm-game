#pragma once

#include <stddef.h>
#include <stdbool.h>

/* Returns true if name resolves to one of the embedded songs. The pointer is
 * to a static buffer that lives for the program's lifetime. */
bool builtin_song_lookup(const char* name, const char** data, size_t* len);
