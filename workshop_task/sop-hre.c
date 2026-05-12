#include "w7-common.h"
#define BACKLOG 10
#define MAX_EVENTS 100

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s port\n", name);
    exit(EXIT_FAILURE);
}

void server_work(int local_tcp_conn){

    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;

    event.data.fd = local_tcp_conn;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, local_tcp_conn, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    ssize_t size;
    char buffer[1024];
    while(1){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, NULL)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int client;
                if(events[n].data.fd == local_tcp_conn){
                    client = add_new_client(events[n].data.fd);
                    bulk_write(client, "Welcome, elector!\n", 18);  
                    event.data.fd = client;
                    epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client, &event);
                } else {
                    size = bulk_read(events[n].data.fd, buffer, sizeof(buffer));
                    printf("%s\n", buffer);
                    if(size == 0){
                        epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, &event);
                        close(events[n].data.fd);
                    }
                }
            }
        }
        else
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_pwait");
        }
    }


    // int client = add_new_client(local_tcp_conn);
    // printf("conn established \n");
    // close(client);
    // close(local_tcp_conn);
}

int main(int argc, char **argv){
    if(argc != 2){
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int local_tcp_conn;
    //int flags;
    local_tcp_conn = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    //flags = fcntl(local_tcp_conn, F_GETFL) | O_NONBLOCK;
    //fcntl(local_tcp_conn, F_SETFL, flags);

    server_work(local_tcp_conn);

}  