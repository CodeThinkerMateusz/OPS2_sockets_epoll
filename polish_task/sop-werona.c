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

typedef struct{
    int fd;
    char name[MAX_MSG_LEN +1];
    char lover[MAX_MSG_LEN +1];
    int ready; // 0 nie ma. 1 ma imie. 2 gotowy na slub 
}Client;



//czyta bajt po bajcie aż do \n, używaj gdy klient wysyła tekst linia po linii
int read_line(int fd, char *buf, int max) {
    int i = 0, n;
    char c;
    while (i < max - 1) {
        n = TEMP_FAILURE_RETRY(read(fd, &c, 1));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return i; // brak danych na razie
            return -1;
        }
        if (n == 0) return 0; // rozlaczenie
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}


void server_work(int local_listen_socket, int timeout){
    // tworzymy deskryptor zeby serwer wiedzial kiedy wiedzial gdy ktos sie  laczy 
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
    Client clients[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS;i++){
        clients[i].fd = -1;
    }
    while(1){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, timeout , NULL)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int client;
                if(events[n].data.fd == local_listen_socket){
                    client = add_new_client(local_listen_socket);
                    // unika zawieszaniu systemu i koncu programu 
                    fcntl(client, F_SETFL, O_NONBLOCK);
                    printf("Another young person (%d) needs my help!\n", client); 
                    fflush(stdout);
                    for(int i = 0; i < MAX_CLIENTS ;i++){
                        if(clients[i].fd == -1){  // uzywamy nowej iteracji zeby randomowo nie nadpisac
                            //ustawiamy tablice struct  
                            clients[i].fd = client;
                            clients[i].ready = 0;

                            // zeby pierwsza osoba  nie  pokazywala  sie  ciagle  musimy wyczyscic ja 
                            memset(clients[i].name, 0, sizeof(clients[i].name));
                            memset(clients[i].lover, 0, sizeof(clients[i].lover));

                            // musimy nowa  strukture  zeby nie nadpisywac zawartosci 
                            // a tu dla  kazdego klineta tworzymy nowa strukture 
                            // bedzimy wiedziec kiedy konkretny klient cos napisze  
                            struct epoll_event ev;
                            ev.events = EPOLLIN;
                            ev.data.fd = client;
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client, &ev);
                            break;
                        }
                    }
                }
                // klient  cos wyslal trzeba  sprawdzic co 
                else{
                // uzywamy printf bo bulkwrite jest do wysyłania danych do klienta przez socket 
                // a tu wypisujemy na standardowe  wyjscie 
                    int fd = events[n].data.fd;
                    //szukamy klienta  po fd
                    for(int i = 0 ; i < MAX_CLIENTS;i++){
                        if(clients[i].fd == fd){
                            
                            if (clients[i].ready == 0) {
                                int s = read_line(fd, clients[i].name, sizeof(clients[i].name));
                                if (s > 0) 
                                // odczytalismy imie klienta 
                                    clients[i].ready = 1;
                                else if (s == 0) {
                                    // klient sie rozlaczyl sprztamy 
                                    printf("I lost contact with ??\n");
                                    goto cleanup_client;
                                }

                            }
                            else if (clients[i].ready == 1) {
                                // czytamy ukochanka klienta 
                                int s = read_line(fd, clients[i].lover, sizeof(clients[i].lover));
                                if (s > 0) {
                                    clients[i].ready = 2; // gotowi na slub 
                                    printf("[%s] wants to marry [%s]\n", clients[i].name, clients[i].lover);
                                    fflush(stdout);

                                    // szukamy kogos kto jest ready na slub
                                    // nie jest ta sama osoba 
                                    // jego imie pasuje do ukochanka a jego ukochanka i 
                                    // a ukochanka ukochany to nasz klient  
                                    for (int j = 0; j < MAX_CLIENTS; j++) {
                                        if (clients[j].fd != -1 && i != j && clients[j].ready == 2) {
                                            if (strcmp(clients[i].lover, clients[j].name) == 0 &&
                                                strcmp(clients[j].lover, clients[i].name) == 0) {
                                                
                                                // pasuja wiec biara slub 
                                                printf("%s and %s got married!\n", clients[i].name, clients[j].name);
                                                fflush(stdout);

                                                char msg[2 * MAX_MSG_LEN + 32];
                                                snprintf(msg, sizeof(msg), "Congratulations %s and %s !\n", clients[i].name, clients[j].name);
                                                
                                                // wysylamy do nich gratulacje 
                                                bulk_write(clients[i].fd, msg, strlen(msg));
                                                bulk_write(clients[j].fd, msg, strlen(msg));

                                                // rozlacznie  obu po slubie 
                                                epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, clients[j].fd, NULL);
                                                close(clients[j].fd);
                                                clients[j].fd = -1;

                                                epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, clients[i].fd, NULL);
                                                close(clients[i].fd);
                                                clients[i].fd = -1;
                                                break;
                                            }
                                        }
                                    }
                                // rozlaczanie  przed  slubem jesli ukochanek sie nie polaczyl 
                                } else if (s == 0) {
                                    printf("I lost contact with [%s]\n", clients[i].name);
                                    goto cleanup_client;
                                }
                            }
                            // zapobiega przedwczesnemu wejsciu w sekcje cleanup 
                            continue;


                            cleanup_client:
                            fflush(stdout);
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                            clients[i].fd = -1;
                        }   
                    }
                }    
            }
        }
    }
    // koncowe  sprzatanie  i zamykanie  
    close(local_listen_socket);
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

    int timeout = atoi(argv[1]) * 1000;
    if (timeout < 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    sethandler(SIG_IGN, SIGPIPE);
    unlink(UNIX_SK_NAME);

    int local_listen_conn;
    local_listen_conn = bind_local_socket(UNIX_SK_NAME, BACKLOG);
    server_work(local_listen_conn, timeout);

    

    return EXIT_SUCCESS;
}
