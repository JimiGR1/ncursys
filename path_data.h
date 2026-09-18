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
#include "print_node.h"
#include "unique_int_store.h"

typedef struct path_data {
    gchar *fullpath;
    gchar *filename;
    unsigned long long size;
    bool is_directory;
    int node_id;
    unique_int_store *nodes_computed;
    int num_files;
    int num_dirs;
} path_data;

/**
 * Release path_data allocated memory.
 *
 * @param pd Pointer to the struct to release.
 */
void        path_data_destroy(path_data *pd);

/**
 * Creates new path_data.
 *
 * @param fullpath The fullpath.
 * @param filename The filename.
 * @param is_direcotry True if directory.
 * @return Pointer to created object.
 */
path_data*  path_data_create(const char *fullpath, const char *filename, bool is_direcotry);

/**
 * Compares the corresponding filenames of given GNodes.
 *
 * @param a Pointer to left GNode.
 * @param b Pointer to right GNode.
 * @return The result of strcmp between a and b filepaths
 */
gint        path_data_compare_filename(gconstpointer a, gconstpointer b);

/**
 * Compares the corresponding sizes of given GNodes.
 *
 * @param a Pointer to left GNode.
 * @param b Pointer to right GNode.
 * @return -1 if a < b, 1 if a > b, 0 otherwise
 */
gint        path_data_compare_filesize(gconstpointer a, gconstpointer b);

#endif /* path_data_h */

