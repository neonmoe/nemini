// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_TREE_RENDERER_H
#define NEMINI_TREE_RENDERER_H

#include <SDL.h>
#include "browser.h"

struct page_preview {
    struct loaded_page *page;
    SDL_Rect rect;
};

void generate_preview_list(struct page_preview **list,
                           struct loaded_page *page,
                           int page_tree_width,
                           int x, int y);

#endif
