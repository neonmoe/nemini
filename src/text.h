// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_TEXT_H
#define NEMINI_TEXT_H

#include "error.h"

enum nemini_error text_renderer_init(void);
void text_renderer_free(void);
enum nemini_error text_render(SDL_Surface **result, const char *text,
                              int width, float scale);

#endif
