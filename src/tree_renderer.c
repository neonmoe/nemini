// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "tree_renderer.h"
#include "browser.h"
#include "stretchy_buffer.h"

static int get_tree_width(struct loaded_page *page) {
    int children_count = sb_count(page->children);
    if (children_count > 0) {
        int total = 0;
        for (int i = 0; i < children_count; i++) {
            total += get_tree_width(page->children[i]);
        }
        return total;
    } else {
        return 1;
    }
}

static int node_width = 100;
static int node_padding = 10;

// Generates the coordinates for page previews in "list". Use -1 for
// "page_tree_width", 0 for "x" and "y".
void generate_preview_list(struct page_preview **list,
                           struct loaded_page *page,
                           int page_tree_width,
                           int x, int y) {
    if (page_tree_width == -1) {
        // For calling from the outside.
        page_tree_width = get_tree_width(page);
    }
    int node_space = node_width + node_padding * 2;
    int page_width = page_tree_width * node_space;
    struct page_preview current_preview = {0};
    current_preview.page = page;
    current_preview.rect.x = x - node_space / 2;
    current_preview.rect.y = y + node_padding;
    current_preview.rect.w = node_width;
    current_preview.rect.h = node_width;
    sb_push(*list, current_preview);

    int children_count = sb_count(page->children);
    int child_x_cursor = x - page_width / 2;
    for (int i = 0; i < children_count; i++) {
        struct loaded_page *child = page->children[i];
        int child_tree_width = get_tree_width(child);
        generate_preview_list(list, child, child_tree_width,
                              child_x_cursor, y + node_space);
        int child_width = child_tree_width * node_space;
        child_x_cursor += child_width;
    }
}
