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

#include <stdlib.h>
#include <strings.h>
#include "tree_view.h"
#include "path_data.h"

tree_view* tree_view_new(void) {
    tree_view *tv = g_new(tree_view, 1);
    tv->expanded = g_hash_table_new(g_direct_hash, g_direct_equal);
    tv->rows = g_array_new(FALSE, FALSE, sizeof(view_row));
    return tv;
}

void tree_view_free(tree_view *tv) {
    g_hash_table_destroy(tv->expanded);
    g_array_free(tv->rows, TRUE);
    g_free(tv);
}

/* qsort comparator over GNode* - largest first, same size alphabetically */
static int compare_for_display(const void *a, const void *b) {

    const path_data *pdA = (const path_data *)(*(GNode * const *)a)->data;
    const path_data *pdB = (const path_data *)(*(GNode * const *)b)->data;

    if (pdA->size > pdB->size)
        return -1;
    else if (pdA->size < pdB->size)
        return 1;
    else
        return strcasecmp(pdA->filename, pdB->filename);
}

static void layout_children(tree_view *tv, GNode *parent, int depth) {

    guint n = g_node_n_children(parent);
    if(n == 0)
        return;

    // sort a copy of the child pointers - the path tree itself is never reordered
    GNode **children = g_new(GNode *, n);
    guint i = 0;
    for(GNode *child = parent->children; child != NULL; child = child->next)
        children[i++] = child;
    qsort(children, n, sizeof(GNode *), compare_for_display);

    for(i = 0; i < n; i++) {
        view_row row = { children[i], depth };
        g_array_append_val(tv->rows, row);
        if(g_hash_table_contains(tv->expanded, children[i]))
            layout_children(tv, children[i], depth + 1);
    }

    g_free(children);
}

void tree_view_layout(tree_view *tv, GNode *root) {
    g_array_set_size(tv->rows, 0);
    if(root != NULL)
        layout_children(tv, root, 1);
}

void tree_view_toggle(tree_view *tv, GNode *node) {

    if(!g_hash_table_contains(tv->expanded, node)) {
        g_hash_table_add(tv->expanded, node);
        return;
    }

    // collapse: drop node and every expanded directory below it,
    // so expanding it again shows its children collapsed
    GHashTableIter iter;
    gpointer key;
    g_hash_table_iter_init(&iter, tv->expanded);
    while(g_hash_table_iter_next(&iter, &key, NULL)) {
        GNode *expanded_node = (GNode *)key;
        if(expanded_node == node || g_node_is_ancestor(node, expanded_node))
            g_hash_table_iter_remove(&iter);
    }
}

int tree_view_find(tree_view *tv, GNode *node) {
    for(guint i = 0; i < tv->rows->len; i++)
        if(tree_view_row(tv, (int)i)->node == node)
            return (int)i;
    return -1;
}
