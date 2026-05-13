#include "l7-common.h"

void usage(char *name)
{
    printf("%s <timeout>\n", name);
    printf("  timeout - max waiting time after receiving the last message/connection (in seconds)\n");
    exit(EXIT_FAILURE);
}

#define SWAP(a, b)                      \
    do                                  \
    {                                   \
        __typeof__(a) __a = (a);        \
        __typeof__(b) __b = (b);        \
        __typeof__(*__a) __tmp = *__a;  \
        *__a = *__b;                    \
        *__b = __tmp;                   \
    } while (0)

#define MAX_CLIENTS 10
#define MAX_PAIRS 3
#define UNIX_SK_NAME "Laurenty"
#define MAX_MSG_LEN 63
#define BACKLOG 10
#define MAX_EVENTS 10

void server_work(int local_listen_socket){
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = local_listen_socket;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, local_listen_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    int size;
    while(1){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, NULL)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int client = add_new_client(events[n].data.fd);
                // uzywamy printf bo bulkwrite jest do wysyłania danych do klienta przez socket 
                // a tu wypisujemy na standardowe  wyjscie 
                printf("Another young person (%d) needs my help!", client); 






            }
        }
    }


}


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int timeout = atoi(argv[1]);
    if (timeout < 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    sethandler(SIG_IGN, SIGPIPE);
    int local_listen_conn;
    local_listen_conn = bind_local_socket(UNIX_SK_NAME, BACKLOG);
    server_work(local_listen_socket);


    return EXIT_SUCCESS;
}
