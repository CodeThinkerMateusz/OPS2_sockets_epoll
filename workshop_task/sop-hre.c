#include "w7-common.h"
#define BACKLOG 10
#define MAX_EVENTS 100

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

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
    int elecotr_state[1024] = {0}; // jesli 0 to niezidentyfikowany 
    event.events = EPOLLIN;

    event.data.fd = local_tcp_conn;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, local_tcp_conn, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    int nfds;
    ssize_t size = 0;
    char buffer[1024];
    char el_num[1];
    int number;
    char vote[1];
    int real_vote[1024] = {0};       // indeksujemy po fd a to system nam przydzila  
    int connected_electors[8] = {0};  // polaczeni po indexach 
    while(do_work){
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, NULL)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                int client;
                if(events[n].data.fd == local_tcp_conn){
                    client = add_new_client(events[n].data.fd); 
                    event.data.fd = client;
                    epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client, &event);
                    

                } else {  // klient  cos wyslal trzeba  sprawdzic co 
                    if(elecotr_state[events[n].data.fd] == 0) {  // czytaj identyfikacje
                        number = bulk_read(events[n].data.fd, el_num, sizeof(el_num));
                        if(el_num[0] == '\n' || el_num[0] == '\r') continue;    // zeby nie  rozlaczalo sie bo tablica 1 bajt 
                        elecotr_state[events[n].data.fd] = el_num[0] - '0';
                        if(elecotr_state[events[n].data.fd] <1 || elecotr_state[events[n].data.fd] > 7){
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, &event);
                            close(events[n].data.fd);
                            continue;
                        }
                        if(connected_electors[elecotr_state[events[n].data.fd]] == 1){
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, &event);
                            close(events[n].data.fd);
                            continue;
                        }
                        printf("Welcome, elector of %d! \n", elecotr_state[events[n].data.fd]); 
                        fflush(stdout); 
                        connected_electors[elecotr_state[events[n].data.fd]] = 1;

                    }else { // czytaj glos 
                        size = bulk_read(events[n].data.fd, vote, sizeof(vote));
                        if(size == 0){    // rozlacznie bo bulk read zwraca 0 
                            // czyszczenie zeby elektor mog wejsc ponownie 
                            connected_electors[elecotr_state[events[n].data.fd]] = 0;   
                            elecotr_state[events[n].data.fd] = 0;

                            // zamykanie połaczenia 
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, &event);
                            close(events[n].data.fd);
                        }
                        if(vote[0] == '\n' || vote[0] == '\r') continue;  // zeby nie  rozlaczalo sie bo tablica 1 bajt 

                        if(vote[0] >= '1' || vote[0] <= '3')
                            real_vote[events[n].data.fd] = vote[0] - '0';
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

    // calculating votes  
    int canditates[4];
    for(int i = 0; i < 1024;i++){
        if(real_vote[i] >= 1 && real_vote[i] <=3)
            canditates[real_vote[i]]++;
    }
    printf("\t1. Francis I, King of France got: %d \n\t2. Charles V, Archduke of Austria and King of Spain got: %d\n\t3. Henry VIII, King of England got: %d \n", canditates[1] , canditates[2], canditates[3]);

}

int main(int argc, char **argv){
    if(argc != 2){
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT))
        ERR("Seting SIGINT:");

    int local_tcp_conn;
    //int flags;
    local_tcp_conn = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    //flags = fcntl(local_tcp_conn, F_GETFL) | O_NONBLOCK;
    //fcntl(local_tcp_conn, F_SETFL, flags);

    server_work(local_tcp_conn);

}  