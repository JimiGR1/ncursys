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
    gchar unit[] = {(char)0, 'K', 'M', 'G', 'P', 'E', 'Z'};
    int unit_index = 0;
    double cur_size = (double)size;
    while(cur_size > 1024) {
        cur_size /= 1024;
        unit_index++;
    }
    
    gchar *res = g_strdup_printf("%.1f%cB", cur_size, unit[unit_index]);
    return res;
}

gchar* get_str_normalized_padded(const char * utf8_str, int level, int final_length) {
    gchar *truncated_str;
    // Normalize the string to NFC (Normalization Form C), which combines characters and their diacritics into a single composed character.
    // more here https://unicode.org/reports/tr15/
    gchar *normalized_str = g_utf8_normalize(utf8_str, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    
    if (g_utf8_strlen(normalized_str, -1) > final_length - ((level - 2) * 2)) {
        gchar *end_ptr = g_utf8_offset_to_pointer(normalized_str, final_length - ((level - 2) * 2));
        truncated_str = g_strndup(normalized_str, end_ptr - normalized_str);
    } else {
        truncated_str = g_strdup(normalized_str);
    }
    
    glong len = g_utf8_strlen(truncated_str, -1);
    glong padding = final_length - ((level - 2) * 2) - len;
    if (padding < 0) padding = 0;
    gchar *pad_str = g_strnfill(padding, ' ');
    
    gchar *padded_str = g_strconcat(truncated_str, pad_str, NULL);
    
    g_free(truncated_str);
    g_free(pad_str);
    g_free(normalized_str);
    
    return padded_str;
    
}
