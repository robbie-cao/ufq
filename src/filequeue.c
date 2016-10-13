#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libubox/list.h>

#include "filequeue.h"

static struct list_head fq_list;
static struct list_head *iterator = NULL;


struct file_object *fq_add(char *filename)
{
    struct fq_item *item;

    item = calloc(1, sizeof(*item));
    if (!item) {
        return NULL;
    }
    strncpy(item->obj.name, filename, sizeof(item->obj.name));
    item->obj.type = ATTR_UNREAD;
    srand(time(NULL));
    item->obj.id = rand();
    list_add(&item->list, &fq_list);

    return &item->obj;
}

void fq_remove(char *filename)
{
    struct list_head *p = &fq_list;

    while (p) {
        struct fq_item *q = container_of(p, struct fq_item, list);
        if (!strcmp(q->obj.name, filename)) {
            list_del(p);
            free(q);
            return ;
        }
        p = p->next;
    }
}

struct file_object *fq_retrieve(void)
{
    struct list_head *p = &fq_list;

    while (p) {
        struct fq_item *q = container_of(p, struct fq_item, list);
        if (q->obj.type == ATTR_UNREAD) {
            // mark it as READ after retrieving
            q->obj.type = ATTR_READ;
            return &q->obj;
        }
        p = p->next;
    }

    return NULL;
}

struct file_object *fq_get(char *filename)
{
    struct list_head *p = &fq_list;

    while (p) {
        struct fq_item *q = container_of(p, struct fq_item, list);
        if (!strcmp(q->obj.name, filename)) {
            return &q->obj;
        }
        p = p->next;
    }
    return NULL;
}

void fq_flush(void)
{
    struct list_head *p;

    list_for_each(p, &fq_list) {
        struct fq_item *q = container_of(p, struct fq_item, list);
        list_del(p);
        free(q);
    }
}


struct file_object *fq_first(int type)
{
    iterator = &fq_list;

    while (iterator) {
        struct fq_item *q = container_of(iterator, struct fq_item, list);
        if (q->obj.type == type) {
            return &q->obj;
        }
        iterator = iterator->next;
    }
    return NULL;
}

struct file_object *fq_next(int type)
{
    while (iterator) {
        struct fq_item *q = container_of(iterator, struct fq_item, list);
        if (q->obj.type == type) {
            return &q->obj;
        }
        iterator = iterator->next;
    }
    return NULL;
}


int fq_is_empty(void)
{
    return list_empty(&fq_list);
}

int fq_count(int type)
{
    int count = 0;
    struct list_head *p;

    list_for_each(p, &fq_list) {
        struct fq_item *q = container_of(p, struct fq_item, list);
        if (q->obj.type == type) {
            count++;
        }
    }
    return count;
}

int fq_init(char *path, uint32_t size)
{
    INIT_LIST_HEAD(&fq_list);

    return 0;
}
