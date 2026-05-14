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
#define MAX_CLIENTS 10
#define MAX_EVENTS 10
#define MAX_MSG_LEN 63
#define UNIX_SK_NAME "RadioStation"

typedef struct{
    int fd;
    char nick[MAX_MSG_LEN +1];
    int has_nick; // 0 doesnt have nick 1 has  nick 
}Client;

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

// odwraca stringi 
void reverse_string(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
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

    // stdin dodajemy raz przed petla zeby operator mogl wysylac wiadomosci
    struct epoll_event stdin_ev;
    stdin_ev.events = EPOLLIN;
    stdin_ev.data.fd = 0;
    epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, 0, &stdin_ev);

    int nfds;
    int number = 0;
    int done = 0;
    int size;
    char message[MAX_MSG_LEN + 1];
    Client clients[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS; i++){
        clients[i].fd = -1;
    }

    while(!done){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, NULL)) > 0){
            for(int n = 0; n < nfds; n++){

                if(events[n].data.fd == local_listen){
                    int client = add_new_client(local_listen);
                    number++;

                    char msg[] = "Welcome to Radio Free Verona!\nEnter your callsign:\n";
                    bulk_write(client, msg, strlen(msg));
                    printf("New listener connected (fd=%d)\n", client);
                    fflush(stdout);

                    if(number >= listeners_count){
                        close(client);
                        done = 1;
                        printf("Radio going off the air!\n");
                        fflush(stdout);
                        break;
                    }

                    for(int i = 0; i < MAX_CLIENTS; i++){
                        if(clients[i].fd == -1){
                            clients[i].fd = client;
                            clients[i].has_nick = 0;
                            memset(clients[i].nick, 0, sizeof(clients[i].nick));

                            // dodawanie bo inaczej epoll nie bedzie wiedzial ze ma sluchac na fd
                            // i epoll nie bedzie zglaszal zdarzen jak klient cos wysle
                            // zawsze jak mamy else to bedziemy mieli to
                            struct epoll_event ev;
                            ev.events = EPOLLIN;
                            ev.data.fd = client;
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client, &ev);
                            break;
                        }
                    }

                } else if(events[n].data.fd == 0){
                    char stdin_msg[MAX_MSG_LEN + 1];
                    size = read_line(0, stdin_msg, sizeof(stdin_msg));
                    if(size == 0) break;

                    if(strncmp(stdin_msg, "!air:", 5) == 0){
                        // broadcast do wszystkich
                        char broadcast[MAX_MSG_LEN + 1];
                        snprintf(broadcast, sizeof(broadcast), "[STUDIO BROADCAST]: %s\n", stdin_msg + 5);
                        for(int k = 0; k < MAX_CLIENTS; k++){
                            if(clients[k].fd != -1 && clients[k].has_nick == 1)
                                bulk_write(clients[k].fd, broadcast, strlen(broadcast));
                        }
                    } else {
                        // szukaj :
                        char *colon = strchr(stdin_msg, ':');
                        if(colon == NULL){
                            printf("Error: invalid format\n");
                            fflush(stdout);
                        } else {
                            *colon = '\0';
                            char *callsign = stdin_msg;
                            char *text = colon + 1;
                            int found = 0;
                            for(int k = 0; k < MAX_CLIENTS; k++){
                                if(clients[k].fd != -1 && clients[k].has_nick == 1
                                   && strcmp(clients[k].nick, callsign) == 0){
                                    char msg[MAX_MSG_LEN + 1];
                                    snprintf(msg, sizeof(msg), "[STUDIO]: %s\n", text);
                                    bulk_write(clients[k].fd, msg, strlen(msg));
                                    found = 1;
                                    break;
                                }
                            }
                            if(!found) printf("Error: unknown callsign '%s'\n", callsign);
                            fflush(stdout);
                        }
                    }

                // events[n].data.fd musi byc fd klienta bo jest to else sprawdzajace local_listen
                } else {
                    int client_fd = events[n].data.fd;
                    for(int i = 0; i < MAX_CLIENTS; i++){
                        // sprawdzamy ktory z nich to on
                        if(clients[i].fd == client_fd){
                            if(clients[i].has_nick == 0){
                                // czy juz ma nick
                                size = read_line(client_fd, message, sizeof(message));
                                if(size == 0){
                                    printf("Anonymous listener dropped off.\n");
                                    fflush(stdout);
                                    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                                    close(client_fd);
                                    // musimy sie rozlaczyc zeby nie zabierac miejsca
                                    clients[i].fd = -1;
                                    break;
                                }
                                // sprawdzenie czy nick jest dostepny
                                int taken = 0;
                                for(int k = 0; k < MAX_CLIENTS; k++){
                                    if(clients[k].fd != -1 && clients[k].has_nick == 1
                                       && strcmp(clients[k].nick, message) == 0){
                                        taken = 1;
                                        break;
                                    }
                                }
                                // jesli zajety to go rozlaczamy bez przywitania
                                if(taken){
                                    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                                    close(client_fd);
                                    clients[i].fd = -1;
                                    break;
                                }
                                // kopiujemy jego nick
                                strcpy(clients[i].nick, message);
                                clients[i].has_nick = 1;
                                // witamy go
                                char welcome[MAX_MSG_LEN + 1];
                                snprintf(welcome, sizeof(welcome), "You are on air, %s!\n", clients[i].nick);
                                bulk_write(client_fd, welcome, strlen(welcome));
                                printf("You are on air, %s!\n", clients[i].nick);
                                fflush(stdout);

                            } else {
                                // tu piszemy inne jego wiadomosci i broadcastujemy
                                size = read_line(client_fd, message, sizeof(message));
                                if(size == 0){
                                    printf("Listener %s disconnected\n", clients[i].nick);
                                    fflush(stdout);
                                    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_fd, NULL);
                                    close(client_fd);
                                    clients[i].fd = -1;
                                    break;
                                }
                                for(int j = 0; j < MAX_CLIENTS; j++){
                                    if(j != i && clients[j].fd != -1 && clients[j].has_nick == 1){
                                        char rev_mess[MAX_MSG_LEN + 1];
                                        char broadcast[MAX_MSG_LEN + 1];
                                        strcpy(rev_mess, message);
                                        reverse_string(rev_mess);
                                        snprintf(broadcast, sizeof(broadcast), "%s broadcasts: %s\n", clients[i].nick, rev_mess);
                                        bulk_write(clients[j].fd, broadcast, strlen(broadcast));
                                    }
                                }
                            }
                            break;
                        }
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