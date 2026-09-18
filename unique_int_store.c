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

#include "unique_int_store.h"

unique_int_store* unique_int_store_new(void) {
    unique_int_store* store = g_new(unique_int_store, 1);
    store->table = g_hash_table_new(g_direct_hash, g_direct_equal);
    return store;
}

void unique_int_store_add(unique_int_store* store, int value) {
    g_hash_table_add(store->table, GINT_TO_POINTER(value));
}

gboolean unique_int_store_contains(unique_int_store* store, int value) {
    return g_hash_table_contains(store->table, GINT_TO_POINTER(value));
}

void unique_int_store_free(unique_int_store* store) {
    g_hash_table_destroy(store->table);
    g_free(store);
}
