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

#include <stdio.h>
#include "path_data.h"
#include <stdlib.h>
#include <string.h>

void path_data_destroy(path_data *pd) {
    g_free(pd->filename);
    //g_free(pd->fullpath); // commented out since freed during tree build time
    
    pd->filename = (char *)NULL;
    pd->fullpath = (char *)NULL;
    
    if(pd->is_directory)
        unique_int_store_free(pd->nodes_computed);
    
}

path_data* path_data_create(const char *fullpath, const char *filename, bool is_directory) {
    
    path_data *pd = g_new(path_data, 1);
    if(pd == NULL)
        return NULL;
    pd->fullpath = g_strdup_printf("%s", fullpath);
    pd->filename = g_strdup_printf("%s", filename);
    
    pd->size = 0;
    pd->is_directory = is_directory;
    if(pd->is_directory)
        pd->nodes_computed = unique_int_store_new();
    
    pd->num_files = 0;
    pd->num_dirs = 0;
    
    return pd;
    
}

gint path_data_compare_filename(gconstpointer a, gconstpointer b) {
    
    const GNode *nodeL = (const GNode *)a;
    const GNode *nodeR = (const GNode *)b;
    
    path_data * dataL = (path_data *)nodeL->data;
    path_data * dataR = (path_data *)nodeR->data;
    
    return strcasecmp(dataL->filename, dataR->filename);
    
}

gint path_data_compare_filesize(gconstpointer a, gconstpointer b) {
    
    const GNode *nodeL = (const GNode *)a;
    const GNode *nodeR = (const GNode *)b;
    
    path_data * dataL = (path_data *)nodeL->data;
    path_data * dataR = (path_data *)nodeR->data;
    
    if (dataL->size > dataR->size)
        return -1;
    else if (dataL->size < dataR->size)
        return 1;
    else
        return 0;
    
}
