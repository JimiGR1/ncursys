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
#include <pthread.h>
#include <glib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <stdlib.h>
#include "path_tree.h"
#include "path_data.h"

#define MAX_ABSOLUTE_PATH_NAME_SIZE 4096

typedef struct queue_node {
    gchar *path;
    GNode *parent;
} queue_node;

struct extra_data_struct {
    GMutex* lock_mutex;
    GNode** root;
    GAsyncQueue** queue;
    int* node_id;
};

/* free heap memory of 'node' */
gboolean free_node_memory(GNode *node, gpointer data) {
    
    (void)data; // variable is not used
    
    path_data *pd = (path_data *)node->data;
    
    path_data_destroy(pd);
    
    g_free(pd);
    pd = (path_data *)NULL;
    
    return FALSE; // Continue traversal
}

gboolean update_ancestor_size(GNode * node, gpointer data) {
    
    (void)data;
    
    path_data *pd = (path_data *)node->data;
    int node_id_to_check = pd->node_id;
    
    GNode *parent = node->parent;
    while(parent != (GNode *)NULL) {
        
        path_data *parent_data = (path_data *)parent->data;
        unique_int_store *parent_nodes_computed = parent_data->nodes_computed;
        
        if(!unique_int_store_contains(parent_nodes_computed, node_id_to_check)) {
            parent_data->size += pd->size;
            if(pd->is_directory)
                parent_data->num_dirs++;
            else
                parent_data->num_files++;
            unique_int_store_add(parent_nodes_computed, node_id_to_check);
        }
        
        parent = parent->parent;
        
    }
    
    return false;
    
}

void free_queue_node(gpointer data) {
    queue_node* qn = (queue_node*)data;
    g_free(qn->path);
    g_free(qn);
}

void process_queue_node(gpointer data, gpointer user_data) {
    
    struct extra_data_struct *e_args = (struct extra_data_struct *)user_data;
    
    GAsyncQueue* queue = *(e_args->queue);
        
    queue_node *q_node = (queue_node *)data;
    
    struct stat path_stat;
    bool is_dir = false;
    
    if(lstat(q_node->path, &path_stat) != 0 ) {
        g_free(q_node->path);
        q_node->path = (char *)NULL;
        g_free(q_node);
        q_node = (queue_node *)NULL;
        return;
    }
        
    /* lock tree since we are adding a node */
    g_mutex_lock(e_args->lock_mutex);
    
    /* create path_data */
    char path_copy[MAX_ABSOLUTE_PATH_NAME_SIZE];
    snprintf(path_copy, MAX_ABSOLUTE_PATH_NAME_SIZE, "%s", q_node->path);
    char *filename = basename(path_copy);
    
    is_dir = S_ISDIR(path_stat.st_mode);
    
    path_data *pd = path_data_create(q_node->path, filename, is_dir);
    
    pd->node_id = *(e_args->node_id);
    *(e_args->node_id) = *(e_args->node_id) + 1;
    
    if(!is_dir) {
        pd->size = (unsigned long long)path_stat.st_blocks * 512;
    }
    
    GNode *new_node = (GNode *)NULL;
    
    // check if tree is created
    if(*(e_args->root) == (GNode *)NULL) {
        new_node = *(e_args->root) = g_node_new(pd);
    }
    else {
        new_node = g_node_append_data(q_node->parent, pd);
    }
    
    /* node is added - unlock tree */
    g_mutex_unlock(e_args->lock_mutex);
    
    if(is_dir) {
        
        struct dirent *dir;
        
        DIR *d = opendir(q_node->path);
        if (d == NULL) {
            g_free(q_node->path);
            q_node->path = (char *)NULL;
            g_free(q_node);
            q_node = (queue_node *)NULL;
            return;
        }
        else {
            while ((dir = readdir(d)) != NULL) {
                if(strcmp(dir->d_name, ".") !=0 && strcmp(dir->d_name, "..") != 0) {
                    queue_node* child_queue_node = g_new(queue_node, 1);
                    child_queue_node->path = g_strdup_printf("%s/%s", q_node->path, dir->d_name);
                    child_queue_node->parent = new_node;
                    g_async_queue_push(queue, child_queue_node);
                }
            }
            closedir(d);
        }
    }
    
    g_free(q_node->path);
    q_node->path = (char *)NULL;
    g_free(q_node);
    q_node = (queue_node *)NULL;
    
    g_free(pd->fullpath);
        
}

void add_in_tree_in_iter(const char* path, GNode **root, GMutex* lock_mutex, volatile int *thread_state, int threads) {
    
    gint *node_id = g_new(int, 1);
    *node_id = 1;
    
    // init async queue - this is where the thread pool is facing
    GAsyncQueue* queue = g_async_queue_new();
    // extra data necessary for threads processing
    struct extra_data_struct e_s = {lock_mutex, root, &queue, node_id };
    // init thread pool
    GThreadPool* pool = g_thread_pool_new_full(process_queue_node, &e_s, free_queue_node, threads, TRUE, NULL);
    // start adding first node in queue (root)
    
    queue_node *qn = g_new(queue_node, 1);
    qn->path = g_strdup_printf("%s", path);
    qn->parent = (GNode *)NULL;
    g_async_queue_push(queue, qn);
    
    // while not received thread finished sentinel, keep trying pop
    while (*thread_state != 0) {
        gpointer task = g_async_queue_try_pop(queue);
        if(task != NULL)
            g_thread_pool_push(pool, task, NULL);
    }
    
    // finish thread pool
    g_thread_pool_free(pool, TRUE, TRUE);
    
    // Release heap memory
    g_free(node_id);
    
    queue_node* f_node;
    while ((f_node = g_async_queue_try_pop(queue)) != NULL) {
        g_free(f_node->path);
        f_node->path = NULL;
        g_free(f_node);
        f_node = NULL;
    }
    g_async_queue_unref(queue);
    
}

void path_tree_update_sizes(path_tree **tree) {
    GNode *root = (*tree)->root;
    if(root == (GNode *)NULL) {
        return;
    }
    else {
        g_node_traverse(root, G_POST_ORDER, G_TRAVERSE_LEAVES, -1, update_ancestor_size, NULL); // mutex propably not needed since we are not affecting leaf nodes or/and adding nodes
        return;
    }
}

path_tree *path_tree_init(void) {
    
    path_tree *pt = g_new(path_tree, 1);
    if(pt == NULL)
        return NULL;
    
    g_mutex_init(&(pt->tree_access_mutex));
    
    pt->root = (GNode *)NULL;
    pt->finished = false;
    
    return pt;
    
}

void path_tree_build(path_tree **pt, const char * path, volatile int *thread_state, int threads) {
    add_in_tree_in_iter(path, &((*pt)->root), &((*pt)->tree_access_mutex), thread_state, threads);
    (*pt)->finished = true;
}

void path_tree_destroy(path_tree *tree) {
    g_mutex_clear(&(tree->tree_access_mutex));
    //g_node_traverse(tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, free_node_memory, NULL);
    //g_node_destroy(tree->root);
    //g_free(tree);
    //tree = NULL;
}
