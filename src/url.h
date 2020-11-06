// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_URL_H
#define NEMINI_URL_H

// Parses the url from input_url into:
// - host (the bit after :// and before the port or resource)
// - port (optionally included after the domain, separated by a :)
// - resource (the bit after the host (and port, if included) starting with /)
// The strings are allocated in this function, remember to free them.
enum nemini_error parse_gemini_url(const char *input_url, char **host,
                                   char **port, char **resource);

#endif
