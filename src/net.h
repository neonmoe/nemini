// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_NET_H
#define NEMINI_NET_H

#include "error.h"
#include "gemini.h"

enum nemini_error net_init(void);
void net_free(void);
enum nemini_error net_request(const char *url, const char *parent_url,
                              struct gemini_response *result);

#endif
