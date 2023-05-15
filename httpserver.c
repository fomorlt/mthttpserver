// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/file.h>

#include <sys/stat.h>

void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);
queue_t *workerqueue;
pthread_mutex_t global;

void *worker() {
    //uintptr_t threadid = (uintptr_t)args;
    while (true) {
        uintptr_t sock;
        queue_pop(workerqueue, (void **) &sock); //q inherently provides sleeping
        handle_connection(sock); //lock stuff is done in GET and PUT
        close(sock); //close in here?
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int opt;
    int threadcount = 4;
    //getopt should only reassign threadcount if it sees a -t
    while ((opt = getopt(argc, argv, ":t:")) != -1) {
        switch (opt) {
        case 't':
            threadcount = atoi(optarg);
            if (threadcount <= 0) {
                fprintf(stderr, "Number of threads must be >= 0!");
                return EXIT_FAILURE;
            }
            break;
        case ':': fprintf(stderr, "No number of threads given! Defaulting to 4.\n"); break;
        case '?':
            fprintf(stderr, "Unknown option detected, %d\n", optopt);
            return EXIT_FAILURE;
            break;
        }
    }

    size_t port;
    //after checking for -t thread count, now...
    //Should just be 1 argument.
    for (int index = optind; index < argc; index++) {
        char *endptr = NULL;
        port = (size_t) strtoull(argv[index], &endptr, 10);
        if (endptr && *endptr != '\0') {
            warnx("invalid port number: %s", argv[index]);
            return EXIT_FAILURE;
        }
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);
    workerqueue = queue_new(threadcount);
    pthread_mutex_init(&global, NULL);

    //thread initialization
    pthread_t threads[threadcount];
    for (int i = 0; i < threadcount; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    //main dispatcher thread loop
    while (1) {
        uintptr_t connfd = listener_accept(&sock);
        //replace this with queue stuff
        //enqueue connfd? how would I let threads grab from the queue concurrently?
        queue_push(workerqueue, (void *) connfd);
        //handle_connection(connfd);
        //close(connfd); don't close!
    }

    return EXIT_SUCCESS;
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

void handle_get(conn_t *conn) {

    char *uri = conn_get_uri(conn); //accesses URI stored in the request struct
    //debug("GET request not implemented. But, we want to get %s", uri);

    // What are the steps in here?

    // 1. Open the file.
    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!
    const Response_t *res = NULL; //initializing a repsonse struct to send back to client
    debug("handling get request for %s", uri);
    char *reqidstr;
    int reqidint; //i have realized that this is unecessary, can just set reqidstr to 0. Too late though!
    //pthread_mutex_lock(&global);
    int fd = open(uri, O_RDONLY);
    //if issues opening
    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR) {
            res = &RESPONSE_FORBIDDEN;
            conn_send_response(conn, res);
            reqidstr = conn_get_header(conn, "Request-Id");
            if (reqidstr == NULL) {
                reqidint = 0;
            } else {
                reqidint = atoi(reqidstr);
            }
            fprintf(stderr, "GET,/%s,403,%d\n", uri, reqidint);
            return;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            conn_send_response(conn, res);
            reqidstr = conn_get_header(conn, "Request-Id");
            if (reqidstr == NULL) {
                reqidint = 0;
            } else {
                reqidint = atoi(reqidstr);
            }
            fprintf(stderr, "GET,/%s,404,%d\n", uri, reqidint);
            return;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            conn_send_response(conn, res);
            reqidstr = conn_get_header(conn, "Request-Id");
            if (reqidstr == NULL) {
                reqidint = 0;
            } else {
                reqidint = atoi(reqidstr);
            }
            fprintf(stderr, "GET,/%s,500,%d\n", uri, reqidint);
            return;
        }
    }
    //LOCK! gets released when closing. do I need to mutex this as well?
    if (flock(fd, LOCK_SH) == -1) {
        fprintf(stderr, "Error when flock in GET!\n");
        exit(EXIT_FAILURE);
    }
    //pthread_mutex_unlock(&global);

    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat getbuf;
    uint64_t filesize; //int or uint64_t?
    fstat(fd, &getbuf);

    // Get the size of the file.
    filesize = getbuf.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)

    //Should already be taken care of up above, but just to be safe...
    if (S_ISDIR(getbuf.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        conn_send_response(conn, res);
        reqidstr = conn_get_header(conn, "Request-Id");
        if (reqidstr == NULL) {
            reqidstr = "0";
        }
        fprintf(stderr, "GET,/%s,403,%s\n", uri, reqidstr);
        return;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)
    // should send the entire response - OK followed by message body

    res = conn_send_file(conn, fd, filesize);

    if (res != NULL) {
        //conn_send_file should return NULL if it is successful in sending message to socket
        //if not even after all of the other checks, then something went wrong
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        conn_send_response(conn, res);
        reqidstr = conn_get_header(conn, "Request-Id");
        if (reqidstr == NULL) {
            reqidstr = "0";
        }
        fprintf(stderr, "GET,/%s,500,%s\n", uri, reqidstr);
        return;
    }
    reqidstr = conn_get_header(conn, "Request-Id");
    if (reqidstr == NULL) {
        reqidint = 0;
    } else {
        reqidint = atoi(reqidstr);
    }
    fprintf(stderr, "GET,/%s,200,%d\n", uri, reqidint);
    //close should release locks
    close(fd);
}

void handle_unsupported(conn_t *conn) {
    debug("handling unsupported request");

    // send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    char *reqid;
    const Response_t *res = NULL;
    debug("handling put request for %s", uri);

    pthread_mutex_lock(&global);
    // Check if file already exists before opening it. Used below!
    bool existed = access(uri, F_OK) == 0;
    debug("%s existed? %d", uri, existed);

    // Lock before opening file, unlock the file and then truncate it
    int fd = open(uri, O_CREAT | O_WRONLY, 0600);
    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            reqid = conn_get_header(conn, "Request-Id");
            if (reqid == NULL) {
                reqid = "0";
            }
            fprintf(stderr, "PUT,/%s,403,%s\n", uri, reqid);
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            reqid = conn_get_header(conn, "Request-Id");
            if (reqid == NULL) {
                reqid = "0";
            }
            fprintf(stderr, "PUT,/%s,500,%s\n", uri, reqid);
            goto out;
        }
    }
    if (flock(fd, LOCK_EX) == -1) {
        fprintf(stderr, "Error when flock in PUT!\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&global);
    //now truncate after file is locked
    ftruncate(fd, 0);

    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
        reqid = conn_get_header(conn, "Request-Id");
        if (reqid == NULL) {
            reqid = "0";
        }
        fprintf(stderr, "PUT,/%s,200,%s\n", uri, reqid);
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
        reqid = conn_get_header(conn, "Request-Id");
        if (reqid == NULL) {
            reqid = "0";
        }
        fprintf(stderr, "PUT,/%s,201,%s\n", uri, reqid);
    }

    close(fd);

out:
    conn_send_response(conn, res);
    pthread_mutex_unlock(&global);
}
