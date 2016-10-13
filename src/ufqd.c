#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include "libubus.h"

#include "log.h"


static struct ubus_context *ctx;
static struct blob_buf b;


static int
ufq_status(struct ubus_context *ctx, struct ubus_object *obj,
                struct ubus_request_data *req, const char *method,
                struct blob_attr *msg)
{
        blob_buf_init(&b, 0);
        blobmsg_add_u16(&b, "status", 0);

        ubus_send_reply(ctx, req, b.head);

        return 0;
}


static const struct ubus_method ufq_methods[] = {
        { .name = "status" , .handler = ufq_status } ,
};

static struct ubus_object_type ufq_object_type = UBUS_OBJECT_TYPE("led", ufq_methods);

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
