#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/select.h>
#include <sys/queue.h>

#include "log.h"


#define BUFFER_SIZE         256

static char buf[BUFFER_SIZE];

pthread_t thread_stdin, thread_download, thread_upload;
pthread_mutex_t mutex_download = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_upload = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv_download = PTHREAD_COND_INITIALIZER;
pthread_cond_t cv_upload = PTHREAD_COND_INITIALIZER;

struct tailhead *headp;                 /*  Tail queue head. */
struct entry {
    TAILQ_ENTRY(entry) entries;         /*  Tail queue. */
    char data[BUFFER_SIZE];
};

TAILQ_HEAD(tailhead, entry) head_dq, head_uq;

void thread_stdin_handler(int *arg)
{
    const char *LOG_TAG = "IN";
    struct entry *item;

    fd_set readfds;

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Input\n");

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

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
                        item = malloc(sizeof(*item));
                        strncpy(item->data, buf + 1, BUFFER_SIZE);
                        LOGD(LOG_TAG, "Add to dl queue - item: %s\n", item->data);
                        pthread_mutex_lock(&mutex_download);
                        TAILQ_INSERT_TAIL(&head_dq, item, entries);
                        pthread_cond_signal(&cv_download);
                        pthread_mutex_unlock(&mutex_download);
                    }
                    break;
                case '2':
                    LOGD(LOG_TAG, "Wakeup T2\n");
                    if (buf[1] == '2') {
                        LOGD(LOG_TAG, "Wakeup T2\n");
                        item = malloc(sizeof(*item));
                        strncpy(item->data, buf, BUFFER_SIZE);
                        LOGD(LOG_TAG, "Add to ul queue - item: %s\n", item->data);
                        pthread_mutex_lock(&mutex_upload);
                        TAILQ_INSERT_TAIL(&head_uq, item, entries);
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
    fprintf(stdout, "<- T - Input\n");
}

void thread_download_handler(int *arg)
{
    const char *LOG_TAG = "DL";
    struct entry *item;

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Download\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    while (1) {
        pthread_mutex_lock(&mutex_download);
        if (TAILQ_EMPTY(&head_dq)) {
            LOGD(LOG_TAG, "Sleeping\n");
            pthread_cond_wait(&cv_download, &mutex_download);
        }
        LOGD(LOG_TAG, "Wakeup\n");
        item = TAILQ_FIRST(&head_dq);
        LOGD(LOG_TAG, "Retrieve from dl queue - item: %s\n", item->data);
        TAILQ_REMOVE(&head_dq, head_dq.tqh_first, entries);
        pthread_mutex_unlock(&mutex_download);

        // TODO
        {
            // Test
            // URL input from stdin
            // eg: 1http://chrishumboldt.com/
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "wget %s", item->data);
            system(cmd);
        }
        free(item);
    }
    fprintf(stdout, "<- T - Download\n");
}

void thread_upload_handler(int *arg)
{
    const char *LOG_TAG = "UL";
    struct entry *item;

    (void) arg;     // Make compiler happy

    fprintf(stdout, "-> T - Upload\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    while (1) {
        pthread_mutex_lock(&mutex_upload);
        if (TAILQ_EMPTY(&head_uq)) {
            LOGD(LOG_TAG, "Sleeping\n");
            pthread_cond_wait(&cv_upload, &mutex_upload);
        }
        LOGD(LOG_TAG, "Wakeup\n");
        item = TAILQ_FIRST(&head_uq);
        LOGD(LOG_TAG, "Retrieve from ul queue - item: %s\n", item->data);
        TAILQ_REMOVE(&head_uq, head_uq.tqh_first, entries);
        free(item);
        pthread_mutex_unlock(&mutex_upload);

        // TODO
        sleep(5);       // Simulate time for upload
    }
    fprintf(stdout, "<- T - Upload\n");
}

int main (void)
{
    fprintf(stdout, "Welcome - %s %s\n", __DATE__, __TIME__);

    TAILQ_INIT(&head_dq);                   /*  Initialize the queue. */
    TAILQ_INIT(&head_uq);                   /*  Initialize the queue. */

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

    pthread_join(thread_stdin, NULL);
    pthread_join(thread_download, NULL);
    pthread_join(thread_upload, NULL);

    pthread_exit(NULL);

    return 0;
}
