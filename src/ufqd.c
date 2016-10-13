#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include "libubus.h"

#include "log.h"
#include "filequeue.h"


static struct ubus_context *ctx;
static struct blob_buf b;


static int
ufqd_status(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        blob_buf_init(&b, 0);
        blobmsg_add_u16(&b, "total", fq_count(ATTR_ALL));
        blobmsg_add_u16(&b, "unread", fq_count(ATTR_UNREAD));
        blobmsg_add_u16(&b, "read", fq_count(ATTR_READ));

        ubus_send_reply(ctx, req, b.head);

        return 0;
}

enum {
        ADD_FILENAME,
        __ADD_MAX
};

static const struct blobmsg_policy add_policy[__ADD_MAX] = {
        [ADD_FILENAME] = { .name = "filename", .type = BLOBMSG_TYPE_STRING },
};

static int ufqd_add(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        struct blob_attr *tb[__ADD_MAX];
        char *filename;

        blobmsg_parse(add_policy, __ADD_MAX, tb, blob_data(msg), blob_len(msg));
        if (!tb[ADD_FILENAME]) {
                return UBUS_STATUS_INVALID_ARGUMENT;
        }

        filename = blobmsg_get_string(tb[ADD_FILENAME]);
        if (!filename) {
                return UBUS_STATUS_UNKNOWN_ERROR;
        }
        struct file_object *fo = fq_add(filename);

        blob_buf_init(&b, 0);
        if (fo) {
                blobmsg_add_string(&b, "result", "ok");
                blobmsg_add_u32(&b, "id", fo->id);
                blobmsg_add_string(&b, "name", fo->name);
                blobmsg_add_u16(&b, "type", fo->type);
        } else {
                blobmsg_add_string(&b, "result", "fail");
        }
        ubus_send_reply(ctx, req, b.head);

        return 0;
}

static int
ufqd_retrieve(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        struct file_object *fo = fq_retrieve();

        blob_buf_init(&b, 0);
        if (fo) {
                blobmsg_add_string(&b, "result", "ok");
                blobmsg_add_u32(&b, "id", fo->id);
                blobmsg_add_string(&b, "name", fo->name);
                blobmsg_add_u16(&b, "type", fo->type);
        } else {
                blobmsg_add_string(&b, "result", "null");
        }
        ubus_send_reply(ctx, req, b.head);

        return 0;
}

static int
ufqd_flush(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        fq_flush();

        blob_buf_init(&b, 0);
        blobmsg_add_string(&b, "result", "ok");
        ubus_send_reply(ctx, req, b.head);

        return 0;
}

static int
ufqd_list(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        void *arr;
        void *tbl;
        struct file_object *fo;

        blob_buf_init(&b, 0);
        blobmsg_add_string(&b, "result", "ok");
        arr = blobmsg_open_array(&b, "unread");
        for (fo = fq_first(ATTR_UNREAD); fo; fo = fq_next(ATTR_UNREAD)) {
                tbl = blobmsg_open_table(&b, NULL);
                blobmsg_add_u32(&b, "id", fo->id);
                blobmsg_add_string(&b, "name", fo->name);
                blobmsg_add_u16(&b, "type", fo->type);
                blobmsg_close_table(&b, tbl);

        }
        blobmsg_close_array(&b, arr);
        arr = blobmsg_open_array(&b, "read");
        for (fo = fq_first(ATTR_READ); fo; fo = fq_next(ATTR_READ)) {
                tbl = blobmsg_open_table(&b, NULL);
                blobmsg_add_u32(&b, "id", fo->id);
                blobmsg_add_string(&b, "name", fo->name);
                blobmsg_add_u16(&b, "type", fo->type);
                blobmsg_close_table(&b, tbl);

        }
        blobmsg_close_array(&b, arr);
        ubus_send_reply(ctx, req, b.head);

        return 0;
}

static const struct ubus_method ufq_methods[] = {
        { .name = "status" , .handler = ufqd_status } ,
        { .name = "list" , .handler = ufqd_list } ,
        { .name = "retrieve" , .handler = ufqd_retrieve } ,
        { .name = "flush" , .handler = ufqd_flush } ,
        UBUS_METHOD("add", ufqd_add, add_policy),
};

static struct ubus_object_type ufq_object_type = UBUS_OBJECT_TYPE("ufq", ufq_methods);

static struct ubus_object ufq_object = {
        .name = "ufq",
        .type = &ufq_object_type,
        .methods = ufq_methods,
        .n_methods = ARRAY_SIZE(ufq_methods),
};

int main(int argc, char **argv)
{
        const char *ubus_socket = NULL;
        int ch;
        int ret;

        while ((ch = getopt(argc, argv, "cs:")) != -1) {
                switch (ch) {
                        case 's':
                                ubus_socket = optarg;
                                break;
                        default:
                                break;
                }
        }

        argc -= optind;
        argv += optind;

        log_open("ufqd", LOG_CHANNEL_CONSOLE);
        log_set_level(LOG_DEBUG);
        fq_init("mua", 1024);

        uloop_init();
        signal(SIGPIPE, SIG_IGN);

        ctx = ubus_connect(ubus_socket);
        if (!ctx) {
                log_err("Failed to connect to ubus\n");
                return -1;
        }

        ubus_add_uloop(ctx);

        ret = ubus_add_object(ctx, &ufq_object);
        if (ret) {
                log_err("Failed to add object: %s\n", ubus_strerror(ret));
        }
        uloop_run();

        ubus_free(ctx);
        uloop_done();

        log_close();

        return 0;
}

/* vim: set ts=8 sw=8 tw=0 list : */
