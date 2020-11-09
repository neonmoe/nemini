// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_STR_H
#define NEMINI_STR_H

#include <SDL.h>
#include "error.h"
#include "ctx.h"

// A better string type than NUL terminated C strings. Created for the
// simple reason that I don't want to have to malloc/free new strings
// when handling substrings.
struct nemini_string {
    const char *ptr;
    unsigned int length;
};

enum nemini_error nemini_string_from(const char *cstring,
                                     struct nemini_string *result);

// Index is clamped between the start and end of the original string,
// and length is cut off at the end of the original string if it would
// overflow.
struct nemini_string nemini_substring(struct nemini_string original,
                                      unsigned int index,
                                      unsigned int length);

// Compares the nemini_string to the given c string, returning true if
// the start of the nemini_string matches.
bool nemini_string_start_matches(struct nemini_string string,
                                 const char *cmp);

#endif
