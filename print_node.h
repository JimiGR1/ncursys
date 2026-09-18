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
 
#ifndef print_node_h
#define print_node_h

#include <glib.h>

typedef enum state { EXPANDED, COLLAPSED } state;

typedef struct print_node {
    GNode *node;                                            // pointer to tree node
    state node_state;                                       // EXPANDED | COLLAPSED
    unsigned int level;                                     // node level - root is: 1
    unsigned int v_index;                                   // vertical index (this is cumulative)
} print_node;

/**
 * Creates a print_node given a path tree node.
 *
 * @param node Node of path tree.
 * @return Pointer to created object.
 */
print_node* print_node_create(GNode *node);

#endif /* print_node_h */
