#include "l7-common.h"

void usage(char *name)
{
    printf(" Maksymalna liczba sluchaczy \n");
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


#define BACKLOG 10
#define MAX_EVENTS 10
#define MAX_MSG_LEN 63
#define UNIX_SK_NAME "RadioStation"

int read_line(int fd, char *buf, int max) {
    int i = 0, n;
    char c;
    while (i < max - 1) {
        n = TEMP_FAILURE_RETRY(read(fd, &c, 1));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return i; 
            return -1;
        }
        if (n == 0) return 0; 
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}




void server_work(int local_listen, int listeners_count){
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = local_listen;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, local_listen, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    int number = 0;
    int done = 0;
    int size;
    char message[MAX_MSG_LEN +1];
    while(!done){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, NULL)) > 0){
            for(int n = 0; n < nfds ; n++){
                if(events[n].data.fd == local_listen){
                    int client = add_new_client(events[n].data.fd);
                    number++;

                    char msg[] = "Welcome to Radio Free Verona! \n";
                    bulk_write(client, msg, strlen(msg));

                    printf("New listener connected (fd=%d) \n", client);
                    if(number >= listeners_count){
                        close(client);
                        done = 1;
                        printf("Radio going off the air! \n");
                        break;
                    }

                    // dodawanie  bo inaczej epoll nie bedzie wiedzial ze ma sluchac na fd 
                    // i epoll nie  bedzie zglaszal zdarzen jak klient cos  wysle 
                    // zawsze jak mamy else to bedzimey mieli to 
                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = client;
                    epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client, &ev);

                }
                // events[n].data.fd musi byc  fd  klienta bo jest to else z sprawdzajace local_listen 
                else{ // tu bedzie  logika jak wysle cos  
                    int clinet_fd = events[n].data.fd;
                    size = read_line(clinet_fd, message, sizeof(message));
                    printf("%d transmitted: %s \n",clinet_fd , message);

                    if(size == 0){
                        printf("Listener (%d) disconnected \n", clinet_fd);
                        epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, clinet_fd ,  NULL);
                        close(clinet_fd);
                    }
                }
            }
        }
    }
    close(local_listen);
    unlink(UNIX_SK_NAME);
    close(epoll_descriptor);
}








int main(int argc, char **argv)
{
    if (argc != 2)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int listeners_count = atoi(argv[1]);
    if(listeners_count < 1 || listeners_count > 20)
        usage(argv[0]);

    sethandler(SIG_IGN, SIGPIPE);
    unlink(UNIX_SK_NAME);

    int local_listen_conn;
    local_listen_conn = bind_local_socket(UNIX_SK_NAME, BACKLOG);
    server_work(local_listen_conn, listeners_count);

    

    return EXIT_SUCCESS;
}