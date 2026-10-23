/*
 * ncursys - A terminal-based tool writen in curses to visualize hard disk usage.
 * Copyright (C) 2026 Dimitris Trivizakis <dim.trivizakis@yandex.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700  /* wcwidth - the ncursesw pkg-config flags usually define it already */
#endif
#include "helpers.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

gchar * get_pretty_size(unsigned long long size) {
    // unsigned long long tops out at 16 EB, so the index never runs past "EB"
    const char *unit[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int unit_index = 0;
    double cur_size = (double)size;
    while(cur_size >= 1024 && unit_index < (int)G_N_ELEMENTS(unit) - 1) {
        cur_size /= 1024;
        unit_index++;
    }

    gchar *res = g_strdup_printf("%.1f%s", cur_size, unit[unit_index]);
    return res;
}

gchar* get_str_normalized_padded(const char * utf8_str, int final_length) {
    if (final_length < 0) final_length = 0;
    // Filenames are arbitrary bytes, not necessarily UTF-8: replace invalid sequences with U+FFFD first,
    // otherwise g_utf8_normalize returns NULL.
    gchar *valid_str = g_utf8_make_valid(utf8_str, -1);
    // Normalize the string to NFC (Normalization Form C), which combines characters and their diacritics into a single composed character.
    // more here https://unicode.org/reports/tr15/
    gchar *normalized_str = g_utf8_normalize(valid_str, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    g_free(valid_str);

    // cut and pad by terminal columns, not characters: CJK and most emoji take 2 columns, combining marks 0.
    // wcwidth is what ncurses uses to place characters, so the columns after the name stay aligned.
    GString *padded = g_string_sized_new(strlen(normalized_str) + final_length);
    int width = 0;
    for (const gchar *p = normalized_str; *p != '\0'; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        int w = wcwidth((wchar_t)c);
        if (w < 0) {
            // not printable - e.g. a newline in a filename would break the row
            c = '?';
            w = 1;
        }
        if (width + w > final_length)
            break;
        g_string_append_unichar(padded, c);
        width += w;
    }
    // a 2 column character that did not fit leaves 1 column to pad
    for (; width < final_length; width++)
        g_string_append_c(padded, ' ');

    g_free(normalized_str);

    return g_string_free(padded, FALSE);

}
