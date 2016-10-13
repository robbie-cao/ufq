#ifndef __FILEQUEUE_H__
#define __FILEQUEUE_H__

enum {
    ATTR_UNREAD = 0,
    ATTR_READ,
};

#define ATTR_ALL    (-1)

struct file_object {
    int id;
    char name[32];      // TODO: extend constant length to be variable
    int type;           // 0 - unread, 1 - read, -1 - all
};

struct fq_item {
    struct file_object obj;
    struct list_head list;

};

int fq_init(char *path, uint32_t size);

// store / retrieve
struct file_object *fq_add(char *filename);
struct file_object *fq_retrieve(void);
struct file_object *fq_get(char *filename);
void fq_remove(char *filename);
void fq_flush(void);

// iterator
struct file_object *fq_first(int type);
struct file_object *fq_next(int type);

// statistics
int fq_is_empty(void);
int fq_count(int type);

// cache
// TBD

#endif
