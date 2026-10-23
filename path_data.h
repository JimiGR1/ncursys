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

#ifndef path_data_h
#define path_data_h

#include <stdbool.h>
#include <glib.h>

typedef struct path_data {
    gchar *filename;
    unsigned long long size;    // own blocks plus all descendants
    bool is_directory;
    int num_files;              // all descendant files
    int num_dirs;               // all descendant directories
} path_data;

/**
 * Release path_data and its allocated memory.
 *
 * @param pd Pointer to the struct to release.
 */
void        path_data_destroy(path_data *pd);

/**
 * Creates new path_data.
 *
 * @param filename The filename.
 * @param is_directory True if directory.
 * @return Pointer to created object.
 */
path_data*  path_data_create(const char *filename, bool is_directory);

#endif /* path_data_h */
