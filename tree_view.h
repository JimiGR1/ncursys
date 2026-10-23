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

#ifndef tree_view_h
#define tree_view_h

#include <glib.h>
#include <stdbool.h>

/* one visible line */
typedef struct view_row {
    GNode *node;        // path tree node - data is path_data
    int depth;          // 1 for direct children of the root
} view_row;

/*
 * What the user sees of the path tree: which directories are expanded,
 * flattened into the list of visible rows. Holds no copies of path data,
 * only pointers into the path tree (whose nodes live until exit).
 */
typedef struct tree_view {
    GHashTable *expanded;   // set of GNode* - expanded directories
    GArray *rows;           // view_row - visible entries in display order, root excluded
} tree_view;

/**
 * Creates an empty view.
 * @return Created view.
 */
tree_view*  tree_view_new(void);

/**
 * Frees the view (not the path tree).
 *
 * @param tv View to free.
 */
void        tree_view_free(tree_view *tv);

/**
 * Rebuilds the visible rows from the path tree: children of the root and of
 * every expanded directory, each level sorted by size (largest first) then name.
 * Caller must hold the path tree lock.
 *
 * @param tv View to update.
 * @param root Root of the path tree, may be NULL.
 */
void        tree_view_layout(tree_view *tv, GNode *root);

/**
 * Expands a collapsed directory, or collapses an expanded one together with
 * everything expanded below it. Takes effect on the next layout.
 *
 * @param tv View.
 * @param node Directory node.
 */
void        tree_view_toggle(tree_view *tv, GNode *node);

/**
 * Finds the row showing node.
 *
 * @param tv View.
 * @param node Node to look for.
 * @return Row index, or -1 if node is not visible.
 */
int         tree_view_find(tree_view *tv, GNode *node);

/**
 * @param tv View.
 * @param index Row index, must be < rows->len.
 * @return The row at index.
 */
static inline view_row* tree_view_row(tree_view *tv, int index) {
    return &g_array_index(tv->rows, view_row, index);
}

#endif /* tree_view_h */
