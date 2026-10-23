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
#include <string.h>
#include "path_tree.h"
#include "path_data.h"

#define MAX_ABSOLUTE_PATH_NAME_SIZE 4096

typedef struct queue_node {
    gchar *path;
    GNode *parent;
} queue_node;

/* shared by all pool workers for the duration of one build */
struct build_context {
    GThreadPool *pool;
    GMutex *lock_mutex;     // guards the path tree
    GNode **root;
    gint *cancel;           // non zero: skip remaining work
    gint pending;           // tasks pushed but not yet finished - scan is done when it drops to 0
    GMutex done_mutex;
    GCond done_cond;
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

/* add size and counts of a newly inserted node to all its ancestors - caller must hold the tree lock */
static void add_to_ancestors(GNode *node) {

    path_data *pd = (path_data *)node->data;

    for(GNode *parent = node->parent; parent != (GNode *)NULL; parent = parent->parent) {
        path_data *parent_data = (path_data *)parent->data;
        parent_data->size += pd->size;
        if(pd->is_directory)
            parent_data->num_dirs++;
        else
            parent_data->num_files++;
    }

}

void free_queue_node(gpointer data) {
    queue_node* qn = (queue_node*)data;
    g_free(qn->path);
    g_free(qn);
}

static void push_task(struct build_context *ctx, queue_node *qn) {
    // count the task before it can possibly run, so pending never drops to 0 early
    g_atomic_int_inc(&ctx->pending);
    g_thread_pool_push(ctx->pool, qn, NULL);
}

static void finish_task(struct build_context *ctx) {
    if(g_atomic_int_dec_and_test(&ctx->pending)) {
        g_mutex_lock(&ctx->done_mutex);
        g_cond_signal(&ctx->done_cond);
        g_mutex_unlock(&ctx->done_mutex);
    }
}

static void scan_path(struct build_context *ctx, queue_node *q_node) {

    struct stat path_stat;

    if(lstat(q_node->path, &path_stat) != 0)
        return;

    /* create path_data - no lock needed, nobody else sees it yet */
    char path_copy[MAX_ABSOLUTE_PATH_NAME_SIZE];
    snprintf(path_copy, MAX_ABSOLUTE_PATH_NAME_SIZE, "%s", q_node->path);
    char *filename = basename(path_copy);

    bool is_dir = S_ISDIR(path_stat.st_mode);

    path_data *pd = path_data_create(q_node->path, filename, is_dir);
    g_free(pd->fullpath);
    pd->fullpath = (gchar *)NULL;

    if(!is_dir) {
        pd->size = (unsigned long long)path_stat.st_blocks * 512;
    }

    GNode *new_node = (GNode *)NULL;

    /* lock tree since we are adding a node */
    g_mutex_lock(ctx->lock_mutex);

    // check if tree is created
    if(*(ctx->root) == (GNode *)NULL) {
        new_node = *(ctx->root) = g_node_new(pd);
    }
    else {
        new_node = g_node_append_data(q_node->parent, pd);
        add_to_ancestors(new_node);
    }

    /* node is added - unlock tree */
    g_mutex_unlock(ctx->lock_mutex);

    if(!is_dir)
        return;

    DIR *d = opendir(q_node->path);
    if(d == NULL)
        return;

    struct dirent *dir;
    while((dir = readdir(d)) != NULL && !g_atomic_int_get(ctx->cancel)) {
        if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            queue_node* child_queue_node = g_new(queue_node, 1);
            child_queue_node->path = g_strdup_printf("%s/%s", q_node->path, dir->d_name);
            child_queue_node->parent = new_node;
            push_task(ctx, child_queue_node);
        }
    }
    closedir(d);

}

void process_queue_node(gpointer data, gpointer user_data) {

    struct build_context *ctx = (struct build_context *)user_data;
    queue_node *q_node = (queue_node *)data;

    // when cancelled, queued tasks are only drained
    if(!g_atomic_int_get(ctx->cancel))
        scan_path(ctx, q_node);

    free_queue_node(q_node);
    finish_task(ctx);

}

void add_in_tree_in_iter(const char* path, GNode **root, GMutex* lock_mutex, gint *cancel, int threads) {

    struct build_context ctx = { .lock_mutex = lock_mutex, .root = root, .cancel = cancel };
    g_mutex_init(&ctx.done_mutex);
    g_cond_init(&ctx.done_cond);

    // init thread pool - workers push discovered children straight back into it
    ctx.pool = g_thread_pool_new(process_queue_node, &ctx, threads > 0 ? threads : 1, TRUE, NULL);

    // start with root
    queue_node *qn = g_new(queue_node, 1);
    qn->path = g_strdup_printf("%s", path);
    qn->parent = (GNode *)NULL;
    push_task(&ctx, qn);

    // wait until every task has finished (on cancel they drain quickly)
    g_mutex_lock(&ctx.done_mutex);
    while(g_atomic_int_get(&ctx.pending) > 0)
        g_cond_wait(&ctx.done_cond, &ctx.done_mutex);
    g_mutex_unlock(&ctx.done_mutex);

    // no task is queued or running - nothing can push anymore
    g_thread_pool_free(ctx.pool, FALSE, TRUE);

    g_cond_clear(&ctx.done_cond);
    g_mutex_clear(&ctx.done_mutex);

}

path_tree *path_tree_init(void) {

    path_tree *pt = g_new(path_tree, 1);
    if(pt == NULL)
        return NULL;

    g_mutex_init(&(pt->tree_access_mutex));

    pt->root = (GNode *)NULL;
    pt->finished = 0;

    return pt;

}

void path_tree_build(path_tree **pt, const char * path, gint *cancel, int threads) {
    add_in_tree_in_iter(path, &((*pt)->root), &((*pt)->tree_access_mutex), cancel, threads);
    g_atomic_int_set(&(*pt)->finished, 1);
}

void path_tree_destroy(path_tree *tree) {
    g_mutex_clear(&(tree->tree_access_mutex));
    //g_node_traverse(tree->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, free_node_memory, NULL);
    //g_node_destroy(tree->root);
    //g_free(tree);
    //tree = NULL;
}
