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

#include "path_data.h"

void path_data_destroy(path_data *pd) {
    g_free(pd->filename);
    g_free(pd);
}

path_data* path_data_create(const char *filename, bool is_directory) {

    path_data *pd = g_new(path_data, 1);    // g_new aborts on failure, never returns NULL
    pd->filename = g_strdup(filename);

    pd->size = 0;
    pd->is_directory = is_directory;

    pd->num_files = 0;
    pd->num_dirs = 0;

    return pd;

}
