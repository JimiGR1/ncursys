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

#include "helpers.h"
#include <stdio.h>

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
    gchar *truncated_str;
    if (final_length < 0) final_length = 0;
    // Filenames are arbitrary bytes, not necessarily UTF-8: replace invalid sequences with U+FFFD first,
    // otherwise g_utf8_normalize returns NULL.
    gchar *valid_str = g_utf8_make_valid(utf8_str, -1);
    // Normalize the string to NFC (Normalization Form C), which combines characters and their diacritics into a single composed character.
    // more here https://unicode.org/reports/tr15/
    gchar *normalized_str = g_utf8_normalize(valid_str, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    g_free(valid_str);

    if (g_utf8_strlen(normalized_str, -1) > final_length) {
        gchar *end_ptr = g_utf8_offset_to_pointer(normalized_str, final_length);
        truncated_str = g_strndup(normalized_str, end_ptr - normalized_str);
    } else {
        truncated_str = g_strdup(normalized_str);
    }

    glong len = g_utf8_strlen(truncated_str, -1);
    glong padding = final_length - len;
    if (padding < 0) padding = 0;
    gchar *pad_str = g_strnfill(padding, ' ');
    
    gchar *padded_str = g_strconcat(truncated_str, pad_str, NULL);
    
    g_free(truncated_str);
    g_free(pad_str);
    g_free(normalized_str);
    
    return padded_str;
    
}
