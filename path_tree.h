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
 
#ifndef path_tree_h
#define path_tree_h

#include <glib.h>
#include <stdbool.h>

typedef struct path_tree {
    GNode *root;                // node data is path_data
    GMutex tree_access_mutex;   // guards the tree shape and all path_data while the scan runs
    gint finished;              // set atomically to 1 when the scan is complete
} path_tree;

/**
 * Initialize tree and return pointer.
 * @return Created tree pointer.
 */
path_tree *path_tree_init(void);

/**
 * Creates a path_tree for given path. Returns when the scan is complete
 * or after *cancel has been set (atomically) to non zero.
 * Node sizes and counts are kept up to date while the tree is built.
 * Sizes follow du: allocated blocks, directories included, hard links counted once.
 *
 * @param pt Tree to fill, from path_tree_init.
 * @param path The path will be the root of the tree.
 * @param cancel Set to non zero with g_atomic_int_set to abort the scan
 * @param threads The number of threads in build tree pool
 * @param one_file_system Skip entries on other filesystems than path (du -x)
 */
void path_tree_build(path_tree *pt, const char * path, gint *cancel, int threads, bool one_file_system);

/**
 * Frees the tree, all its nodes and their data. The build must have returned.
 *
 * @param tree Pointer to the tree.
 */
void path_tree_destroy(path_tree *tree);

#endif /* path_tree_h */
