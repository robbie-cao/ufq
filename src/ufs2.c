#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

#include <sqlite3.h>

#include <sys/select.h>
#include <sys/queue.h>
#include <curl/curl.h>
#include <sys/stat.h>

#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "log.h"


#define BUFFER_SIZE         256

#define DB_DOWNLOAD         "/root/fqd.db"
#define DB_UPLOAD           "/root/fqu.db"

static char buf[BUFFER_SIZE];

pthread_t thread_stdin, thread_download, thread_upload;
pthread_mutex_t mutex_download = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_upload = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv_download = PTHREAD_COND_INITIALIZER;
pthread_cond_t cv_upload = PTHREAD_COND_INITIALIZER;

sqlite3 *db_download, *db_upload;

struct progress {
    double lastruntime;
    CURL *curl;
};

#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3

/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
static int xferinfo(void *p,
        curl_off_t dltotal, curl_off_t dlnow,
        curl_off_t ultotal, curl_off_t ulnow)
{
    struct progress *myp = (struct progress *)p;
    CURL *curl = myp->curl;
    double curtime = 0;

    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

    /* under certain circumstances it may be desirable for certain functionality
       to only run every N seconds, in order to do this the transaction time can
       be used */
    if((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
        myp->lastruntime = curtime;
        fprintf(stderr, "TOTAL TIME: %f \r\n", curtime);
    }

    fprintf(stderr, "UP: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
            "  DOWN: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
            "\r\n",
            ulnow, ultotal, dlnow, dltotal);

    return 0;
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
upload_complete_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;

    printf("\r\nUpload done. RESPONSE:%s\r\n", (char *)contents);

    return realsize;
}

int file_upload(char *file_path)
{
    CURL *curl;
    CURLcode res;
    struct stat file_info;
    double speed_upload, total_time;
    FILE *fd;

    struct MemoryStruct chunk;
    struct progress prog;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    fd = fopen(file_path, "rb"); /* open file to upload */
    if (!fd) {
        return 1; /* can't continue */
    }

    /* to get the file size */
    if (fstat(fileno(fd), &file_info) != 0) {
        return 1; /* can't continue */
    }

    curl = curl_easy_init();

    if (!curl) {
        fclose(fd);
        return 1;
    }

    /*   */
    struct curl_httppost* post = NULL;
    struct curl_httppost* last = NULL;

    curl_formadd(&post, &last, CURLFORM_COPYNAME, "filename", CURLFORM_FILE, file_path, CURLFORM_END);

    /* upload to this place */
    curl_easy_setopt(curl, CURLOPT_URL,"http://test.muabaobao.com/record/upload");

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, upload_complete_cb);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    /* tell it to "upload" to the URL */
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);

    /* set where to read from (on Windows you need to use READFUNCTION too) */
    //curl_easy_setopt(curl, CURLOPT_READDATA, fd);

    /* and give the size of the upload (optional) */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (curl_off_t)file_info.st_size);

    /* enable verbose for easier tracing */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

#if 0
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
    /* pass the struct pointer into the xferinfo function, note that this is
       an alias to CURLOPT_PROGRESSDATA */
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
#endif

    res = curl_easy_perform(curl);

    /* Check for errors */
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

    } else {
        /* now extract transfer info */
        curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

        fprintf(stderr, "Speed: %.3f bytes/sec during %.3f seconds\n",
                speed_upload, total_time);
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
    curl_formfree(post);
    fclose(fd);

    return 0;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);

    return written;
}

int file_download(char *url)
{
    CURL *curl;
    char filename[256];
    FILE *fd;

    /* tmep use rand number as file name */
    time_t t;
    srand((unsigned) time(&t));
    sprintf(filename, "dl-%08d", rand());

    /* init the curl session */
    curl = curl_easy_init();

    /* set URL to get here */
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* switch on full protocol/debug output while testing */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* disable progress meter, set to 0L to enable and disable debug output */
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    /* open the file */
    fd = fopen(filename, "wb");
    if(fd) {
        /* write the page body to this file handle */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);

        /* get it! */
        curl_easy_perform(curl);

        /* close the header file */
        fclose(fd);
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl);

    return 0;
}

static struct ubus_context *ctx;
static struct blob_buf b;

enum {
    DOWNLOAD_URL_PATH,
    __DOWNLOAD_MAX
};

static const struct blobmsg_policy download_policy[__DOWNLOAD_MAX] = {
    [DOWNLOAD_URL_PATH] = { .name = "url", .type = BLOBMSG_TYPE_STRING },
};

static int ufs_download(struct ubus_context *ctx, struct ubus_object *obj,
        struct ubus_request_data *req, const char *method,
        struct blob_attr *msg)
{
    const char *LOG_TAG = "UBUS";
    struct blob_attr *tb[__DOWNLOAD_MAX];
    char *url;

    sqlite3_stmt *res;
    char *err_msg = 0;
    char *sql;
    int step;
    int rc = 0;

    blobmsg_parse(download_policy, __DOWNLOAD_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[DOWNLOAD_URL_PATH]) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    url = blobmsg_get_string(tb[DOWNLOAD_URL_PATH]);
    if (!url) {
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    LOGD(LOG_TAG, "Wakeup T1\n");
    LOGD(LOG_TAG, "Add to dl queue - item: %s\n", buf + 1);
    pthread_mutex_lock(&mutex_download);
    sql = "INSERT INTO tbl_download(url) VALUES (@url)";
    rc = sqlite3_prepare_v2(db_download, sql, -1, &res, 0);
    if (rc == SQLITE_OK) {
        int idx = sqlite3_bind_parameter_index(res, "@url");
        sqlite3_bind_text(res, idx, url, -1, SQLITE_STATIC);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db_download));
    }
    step = sqlite3_step(res);
    sqlite3_finalize(res);
    pthread_cond_signal(&cv_download);
    pthread_mutex_unlock(&mutex_download);

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "result", "ok");
    blobmsg_add_string(&b, "url", url);
    ubus_send_reply(ctx, req, b.head);

    return 0;
}


enum {
    UPLOAD_URL_PATH,
    __UPLOAD_MAX
};

static const struct blobmsg_policy upload_policy[__UPLOAD_MAX] = {
    [UPLOAD_URL_PATH] = { .name = "url", .type = BLOBMSG_TYPE_STRING },
};

static int ufs_upload(struct ubus_context *ctx, struct ubus_object *obj,
        struct ubus_request_data *req, const char *method,
        struct blob_attr *msg)
{
    const char *LOG_TAG = "UBUS";
    struct blob_attr *tb[__UPLOAD_MAX];
    char *url;

    sqlite3_stmt *res;
    char *err_msg = 0;
    char *sql;
    int step;
    int rc = 0;

    blobmsg_parse(upload_policy, __UPLOAD_MAX, tb, blob_data(msg), blob_len(msg));
    if (!tb[UPLOAD_URL_PATH]) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    url = blobmsg_get_string(tb[UPLOAD_URL_PATH]);
    if (!url) {
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    LOGD(LOG_TAG, "Wakeup T2\n");
    LOGD(LOG_TAG, "Add to ul queue - item: %s\n", buf + 1);
    pthread_mutex_lock(&mutex_upload);
    sql = "INSERT INTO tbl_upload(path) VALUES (@path)";
    rc = sqlite3_prepare_v2(db_upload, sql, -1, &res, 0);
    if (rc == SQLITE_OK) {
        int idx = sqlite3_bind_parameter_index(res, "@path");
        sqlite3_bind_text(res, idx, url, -1, SQLITE_STATIC);
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db_upload));
    }
    step = sqlite3_step(res);
    sqlite3_finalize(res);
    pthread_cond_signal(&cv_upload);
    pthread_mutex_unlock(&mutex_upload);

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "result", "ok");
    blobmsg_add_string(&b, "url", url);
    ubus_send_reply(ctx, req, b.head);

    return 0;
}

static const struct ubus_method ufs_methods[] = {
    UBUS_METHOD( "upload"    , ufs_upload    , upload_policy     ),
    UBUS_METHOD( "download"  , ufs_download  , download_policy   ),
};

static struct ubus_object_type ufs_object_type = UBUS_OBJECT_TYPE("ufs2", ufs_methods);

static struct ubus_object ufs_object = {
    .name = "ufs2",
    .type = &ufs_object_type,
    .methods = ufs_methods,
    .n_methods = ARRAY_SIZE(ufs_methods),
};

void thread_stdin_handler(int *arg)
{
    const char *LOG_TAG = "IN";
    sqlite3 *db_dl, *db_ul;
    sqlite3_stmt *res;
    char *err_msg = 0;
    char *sql;
    int step;
    int rc = 0;

    fd_set readfds;

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Input\n");

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    rc = sqlite3_open(DB_DOWNLOAD, &db_dl);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_dl));
        sqlite3_close(db_dl);

        exit(1);
    }
    rc = sqlite3_open(DB_UPLOAD, &db_ul);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_ul));
        sqlite3_close(db_ul);

        exit(1);
    }
    while (1) {
        if (select(1, &readfds, NULL, NULL, NULL)) {
            int ret = scanf("%s", buf);

            if (ret) {
                LOGD(LOG_TAG, "%s\n", buf);
            }

            switch (buf[0]) {
                case '1':
                    if (strlen(buf + 1)) {
                        LOGD(LOG_TAG, "Wakeup T1\n");
                        LOGD(LOG_TAG, "Add to dl queue - item: %s\n", buf + 1);
                        pthread_mutex_lock(&mutex_download);
                        sql = "INSERT INTO tbl_download(url) VALUES (@url)";
                        rc = sqlite3_prepare_v2(db_dl, sql, -1, &res, 0);
                        if (rc == SQLITE_OK) {
                            int idx = sqlite3_bind_parameter_index(res, "@url");
                            sqlite3_bind_text(res, idx, buf + 1, -1, SQLITE_STATIC);
                        } else {
                            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db_dl));
                        }
                        step = sqlite3_step(res);
                        sqlite3_finalize(res);
                        pthread_cond_signal(&cv_download);
                        pthread_mutex_unlock(&mutex_download);
                    }
                    break;
                case '2':
                    LOGD(LOG_TAG, "Wakeup T2\n");
                    if (strlen(buf + 1)) {
                        LOGD(LOG_TAG, "Wakeup T2\n");
                        LOGD(LOG_TAG, "Add to ul queue - item: %s\n", buf + 1);
                        pthread_mutex_lock(&mutex_upload);
                        sql = "INSERT INTO tbl_upload(path) VALUES (@path)";
                        rc = sqlite3_prepare_v2(db_ul, sql, -1, &res, 0);
                        if (rc == SQLITE_OK) {
                            int idx = sqlite3_bind_parameter_index(res, "@path");
                            sqlite3_bind_text(res, idx, buf + 1, -1, SQLITE_STATIC);
                        } else {
                            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db_ul));
                        }
                        step = sqlite3_step(res);
                        sqlite3_finalize(res);
                        pthread_cond_signal(&cv_upload);
                        pthread_mutex_unlock(&mutex_upload);
                    }
                    break;
                default:
                    break;
            }
        }

        printf("...\n");
    }
    sqlite3_close(db_dl);
    sqlite3_close(db_ul);
    fprintf(stdout, "<- T - Input\n");
}

void thread_download_handler(int *arg)
{
    const char *LOG_TAG = "DL";
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    char *sql;
    int step;
    int rc = 0;

    char buffer[BUFFER_SIZE];

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Download\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    rc = sqlite3_open(DB_DOWNLOAD, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }

    while (1) {
        int remained = 0;
        int id = 0;

        pthread_mutex_lock(&mutex_download);
        sql = "SELECT COUNT(*) FROM tbl_download";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            remained = sqlite3_column_int(res, 0);
            printf("Remained %d to download\n", remained);
        }
        sqlite3_finalize(res);
        if (!remained) {
            LOGD(LOG_TAG, "Sleeping\n");
            pthread_cond_wait(&cv_download, &mutex_download);
        }
        LOGD(LOG_TAG, "Wakeup\n");
        sql = "SELECT id, url FROM tbl_download ORDER BY id LIMIT 1";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            const char *url = sqlite3_column_text(res, 1);
            id = sqlite3_column_int(res, 0);
            LOGD(LOG_TAG, "Retrieve from dl queue - item: %s\n", url);
            strncpy(buffer, url, BUFFER_SIZE);
        }
        sqlite3_finalize(res);
        pthread_mutex_unlock(&mutex_download);

        file_download(buffer);

        pthread_mutex_lock(&mutex_download);
        sql = "DELETE FROM tbl_download WHERE id = ?";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(res, 1, id);
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        sqlite3_finalize(res);
        pthread_mutex_unlock(&mutex_download);
    }
    sqlite3_close(db);
    fprintf(stdout, "<- T - Download\n");
}

void thread_upload_handler(int *arg)
{
    const char *LOG_TAG = "UL";
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    char *sql;
    int step;
    int rc = 0;

    char buffer[BUFFER_SIZE];

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Upload\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    rc = sqlite3_open(DB_UPLOAD, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        exit(1);
    }

    while (1) {
        int remained = 0;
        int id = 0;

        pthread_mutex_lock(&mutex_upload);
        sql = "SELECT COUNT(*) FROM tbl_upload";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            remained = sqlite3_column_int(res, 0);
            printf("Remained %d to upload\n", remained);
        }
        sqlite3_finalize(res);
        if (!remained) {
            LOGD(LOG_TAG, "Sleeping\n");
            pthread_cond_wait(&cv_upload, &mutex_upload);
        }
        LOGD(LOG_TAG, "Wakeup\n");
        sql = "SELECT id, path FROM tbl_upload ORDER BY id LIMIT 1";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            const char *path = sqlite3_column_text(res, 1);
            id = sqlite3_column_int(res, 0);
            LOGD(LOG_TAG, "Retrieve from ul queue - item: %s\n", path);
            strncpy(buffer, path, BUFFER_SIZE);
        }
        sqlite3_finalize(res);
        pthread_mutex_unlock(&mutex_upload);

        file_upload(buffer);

        pthread_mutex_lock(&mutex_upload);
        sql = "DELETE FROM tbl_upload WHERE id = ?";
        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(res, 1, id);
        } else {
            fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        }
        step = sqlite3_step(res);
        sqlite3_finalize(res);
        pthread_mutex_unlock(&mutex_upload);
    }
    fprintf(stdout, "<- T - Upload\n");
}

int db_init(void)
{
    sqlite3 *db;
    char *err_msg = 0;
    char *sql;
    int rc = 0;

    printf("sqlite3 v%s\n", sqlite3_libversion());

    rc = sqlite3_open(DB_DOWNLOAD, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        return 1;
    }

    sql = "CREATE TABLE IF NOT EXISTS tbl_download (id INTEGER PRIMARY KEY, url TEXT);";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    sqlite3_close(db);

    rc = sqlite3_open(DB_UPLOAD, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);

        return 1;
    }

    sql = "CREATE TABLE IF NOT EXISTS tbl_upload (id INTEGER PRIMARY KEY, path TEXT);";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);

        sqlite3_free(err_msg);
        sqlite3_close(db);

        return 1;
    }

    sqlite3_close(db);

    return 0;
}

int main(int argc, char **argv)
{
    const char *ubus_socket = NULL;
    int ch;
    int ret;
    int rc;

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

    log_open("ufs2", LOG_CHANNEL_CONSOLE);
    log_set_level(LOG_DEBUG);

    fprintf(stdout, "Welcome - %s %s\n", __DATE__, __TIME__);

    if (db_init() != 0) {
        fprintf(stderr, "Fail to init database\n");
        return 1;
    }

    rc = sqlite3_open(DB_DOWNLOAD, &db_download);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_download));
        sqlite3_close(db_download);

        exit(1);
    }
    rc = sqlite3_open(DB_UPLOAD, &db_upload);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_upload));
        sqlite3_close(db_upload);

        exit(1);
    }

    pthread_create(&thread_stdin,
            NULL,
            (void *) thread_stdin_handler,
            (void *) NULL);
    pthread_create(&thread_download,
            NULL,
            (void *) thread_download_handler,
            (void *) NULL);
    pthread_create(&thread_upload,
            NULL,
            (void *) thread_upload_handler,
            (void *) NULL);

    uloop_init();
    signal(SIGPIPE, SIG_IGN);

    ctx = ubus_connect(ubus_socket);
    if (!ctx) {
        log_err("Failed to connect to ubus\n");
        return -1;
    }

    ubus_add_uloop(ctx);

    ret = ubus_add_object(ctx, &ufs_object);
    if (ret) {
        log_err("Failed to add object: %s\n", ubus_strerror(ret));
    }
    uloop_run();

    ubus_free(ctx);
    uloop_done();

    sqlite3_close(db_download);
    sqlite3_close(db_upload);

    log_close();

    pthread_join(thread_stdin, NULL);
    pthread_join(thread_download, NULL);
    pthread_join(thread_upload, NULL);

    pthread_exit(NULL);

    return 0;
}
