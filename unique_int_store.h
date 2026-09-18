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

#ifndef unique_int_store_h
#define unique_int_store_h

#include <glib.h>

typedef struct {
    GHashTable *table;
} unique_int_store;

/**
 * Creates a HashSet of unique integers.
 *
 * @return Pointer to created object.
 */
unique_int_store* unique_int_store_new(void);

/**
 * Adds new int to store.
 *
 * @param store Pointer to store.
 * @param value Int value to add.
 */
void unique_int_store_add(unique_int_store* store, int value);

/**
 * Check if store contains value.
 *
 * @param store Pointer to store.
 * @param value Int value to check.
 * @return True if found, false otherwise.
 */
gboolean unique_int_store_contains(unique_int_store* store, int value);

/**
 * Release store memory.
 *
 * @param store Pointer to store.
 */
void unique_int_store_free(unique_int_store* store);

#endif
