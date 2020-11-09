// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "str.h"
#include "error.h"

enum nemini_error nemini_string_from(const char *cstring,
                                     struct nemini_string *output) {
    if (cstring == NULL) {
        return ERR_SRC_STR_NULL;
    }
    struct nemini_string result = {0};
    result.ptr = cstring;
    result.length = SDL_strlen(cstring);
    *output = result;
    return ERR_NONE;
}

enum nemini_error nemini_string_substring(struct nemini_string original,
                                          struct nemini_string *output,
                                          unsigned int index,
                                          unsigned int length) {
    if (index >= original.length) {
        return ERR_SUBSTR_OFFSET_OUT_OF_BOUNDS;
    } else if (index + length >= original.length) {
        return ERR_SUBSTR_LENGTH_OUT_OF_BOUNDS;
    }
    struct nemini_string result = {0};
    result.ptr = &original.ptr[index];
    result.length = length;
    *output = result;
    return ERR_NONE;
}

bool nemini_string_start_matches(struct nemini_string string, const char *cmp) {
    if (cmp == NULL) {
        return false;
    }
    for (unsigned int i = 0; i < string.length; i++) {
        char a = cmp[i];
        if (a == '\0') {
            return true;
        }
        char b = string.ptr[i];
        if (a != b) {
            return false;
        }
    }
    return false;
}