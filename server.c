#define MAX_INPUT_DIM 15

#include <sys/time.h>
#include <unistd.h>

#include "lib/utils.h"
#include "lib/game/server.h"
#include "lib/game/ui.h"

typedef enum {
    LOG_ERROR,
    LOG_CUSTOM_ERROR,
    LOG_STDIN,
    LOG_INFO,
    LOG_SOCKET,
    LOG_ARROW
} LOG_TYPE;
static void plog(LOG_TYPE type, const char* msg, int sd);

static void init_fd_set(fd_set* set);
static void insert_fd_into_set(int, fd_set*, int*);
static void remove_fd_from_set(int, fd_set*);

static int init_server(int, fd_set*, int*);
static int accept_request(int, fd_set*, int*);
static void compute(int sd, desc_msg* msg);

static bool check_user(const char* username, const char* password);
static bool register_user(const char* username, const char* password);
static bool username_exists(const char* username);

int main(int argc, char* args[]) 
{
    int listener /* Socket di ascolto */, i /* Indice per ciclo for */;
    int server_port;
    char buffer[MAX_INPUT_DIM];

    fd_set master, read_fds;
    int fdmax = -1;

    // Lettura della porta
    if (argc > 2) {
        printf("Error:\ttoo many arguments\n");
        return 0;
    }
    if (argc < 2) {
        printf("Error:\tplease specify the server port\n");
        return 0;
    }
    if (!get_port(args[1], &server_port)) {
        printf("Error:\tport not valid\n");
        return 0;
    }

    // Visualizzazione del menu di start
    while (true) 
    {
        system("clear");
        printf(SER_START_MENU, server_port);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM);

        if (strcmp("start", buffer) == 0) {
            // L'utente ha chiesto di far partire il server
            system("clear");
            break;
        } else if (strcmp("stop", buffer) == 0) {
            // L'utente ha chiesto di terminare il server
            return 0;
        }
    }

    // Inizializzazione del Server
    listener = init_server(server_port, &master, &fdmax);
    init_fd_set(&read_fds);

    // Main loop
    while (true) 
    {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        for (i = 0; i <= fdmax; i++) 
        {
            if (!FD_ISSET(i, &read_fds))
                continue;

            if (i == STDIN_FILENO) /* Standard input (tastiera) pronto */
            {
                read_line(buffer, MAX_INPUT_DIM);
                plog(LOG_STDIN, buffer, 0);
                if (strcmp("stop", buffer) == 0) 
                {
                    // L'utente ha richiesto l'arresto del server
                    if (!activeUsers())
                        goto quit;
                    
                    plog(LOG_CUSTOM_ERROR, "Impossibile arrestare il server, ci sono ancora utenti in gioco", 0);
                }
            }
            else if (i == listener) /* Nuova richiesta di connessione */
            {
                accept_request(listener, &master, &fdmax);
            }
            else /* Socket di comunicazione pronto */
            {
                desc_msg msg;
                switch(receive_from_socket(i, &msg))
                {
                    case OK: {
                        // Messaggio ricevuto correttamente
                        compute(i, &msg);
                        continue;
                    }
                    case NET_ERR_REMOTE_SOCKET_CLOSED: {
                        // Il client si è disconnesso
                        plog(LOG_SOCKET, "Disconnesso", i);

                        authUserDisconnected(i);
                        remove_fd_from_set(i, &master);
                        close(i);

                        printf("\n");
                        continue;
                    }
                    default:
                        continue;
                }
            }
        }
    }

quit:
    // Chiudo eventuali descrittori ancora aperti
    for (i = 0; i <= fdmax; i++) 
    {
        if (!FD_ISSET(i, &master))
            continue;
        close(i);
    }
    // Chiudo il descrittore del socket di ascolto
    close(listener);
    return 0;
}

static int init_server(int port, fd_set* master, int* fdmax) 
{
    struct sockaddr_in my_addr;
    int listener, ret;

    // Costruzione indirizzo del server
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

    // Creazione del socket di ascolto
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        plog(LOG_ERROR, "Init", 0);
        exit(EXIT_FAILURE);
    }

    // Collegamento del socket all'indirizzo e alla porta
    ret = bind(listener, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        plog(LOG_ERROR, "Init", 0);
        close(listener);
        exit(EXIT_FAILURE);
    }

    // Mettiamo il socket in ascolto di connessioni
    ret = listen(listener, BACKLOG);
    if (ret < 0) {
        plog(LOG_ERROR, "Init", 0);
        close(listener);
        exit(EXIT_FAILURE);
    }

    // Azzera il master set
    init_fd_set(master);
    
    // Aggiunta del descrittore del socket di ascolto al master set
    insert_fd_into_set(listener, master, fdmax);

    // Aggiunta del descrittore dello standard input al master set
    insert_fd_into_set(STDIN_FILENO, master, fdmax);

    plog(LOG_INFO, "Server inizializzato", 0);
    return listener;
}
static int accept_request(int listener, fd_set* master, int* fdmax) 
{
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    int sd;
    sd = accept(listener, (struct sockaddr*)&cli_addr, &len);
    if (sd < 0) {
        plog(LOG_ERROR, "Accettazione richiesta", 0);
        return sd;
    }

    // Aggiunta del descrittore del nuovo socket di comunicazione al master set
    insert_fd_into_set(sd, master, fdmax);

    // Messaggio di Log
    plog(LOG_SOCKET, "Inizializzato", sd);

    return sd;
}
static void compute(int sd, desc_msg* msg) 
{
    switch (msg->type)
    {
        case MSG_REQ_LOGIN: 
        {
            char username[MAX_USR_DIM], password[MAX_PSW_DIM];
            memset(username, '\0', 1); memset(username, '\0', 1);
            sscanf(msg->payload, "%s %s", username, password);
            
            plog(LOG_SOCKET, "Richiesta di login", sd);
            if (check_user(username, password)) {
                if (authUserSuccess(sd) == OK) {
                    plog(LOG_ARROW, "OK\n", sd);
                    return;
                }
            }
            else {
                if (authUserFailed(sd) == OK) {
                    plog(LOG_ARROW, "OK\n", sd);
                    return;
                }
            }
            plog(LOG_ARROW, "ERR\n", sd);
            break;
        }
        case MSG_REQ_SIGNUP: 
        {
            char username[MAX_USR_DIM], password[MAX_PSW_DIM];
            memset(username, '\0', 1); memset(username, '\0', 1);
            sscanf(msg->payload, "%s %s", username, password);

            plog(LOG_SOCKET, "Richiesta di signup", sd);
            if (register_user(username, password)) {
                if (authUserSuccess(sd) == OK) {
                    plog(LOG_ARROW, "OK\n", sd);
                    return;
                }
            } else {
                if (authUsernameExists(sd) == OK) {
                    plog(LOG_ARROW, "OK\n", sd);
                    return;
                }
            }
            plog(LOG_ARROW, "ERR\n", sd);
            break;
        }
        case MSG_REQ_ROOM_NAMES:
        {
            plog(LOG_SOCKET, "Richiesta nomi delle stanze", sd);
            if (sendRoomNames(sd) == OK)
                plog(LOG_ARROW, "OK\n", sd);
            else
                plog(LOG_ARROW, "ERR\n", sd);
            break;
        }
        case MSG_REQ_START_GAME: 
        {
            char username[MAX_USR_DIM];
            int room = -1;
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s %d", username, &room);

            plog(LOG_SOCKET, "Richiesta di iniziare giocare", sd);
            if (startGame(sd, username, room) == OK)
                plog(LOG_ARROW, "OK\n", sd);
            else
                plog(LOG_ARROW, "ERR\n", sd);
            break;
        }
        case MSG_GAME_CMD_LOOK:
        {
            char what[MAX_NAME_DIM];
            memset(what, '\0', 1);
            sscanf(msg->payload, "%s", what);

            plog(LOG_SOCKET, "CMD Look", sd);
            switch(cmdLook(sd, what)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_OBJS:
        {
            plog(LOG_SOCKET, "CMD Objs", sd);
            switch(cmdObjs(sd)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_USE:
        {
            char obj1[MAX_NAME_DIM], obj2[MAX_NAME_DIM];
            memset(obj1, '\0', 1); memset(obj2, '\0', 1);
            sscanf(msg->payload, "%s %s", obj1, obj2);
            
            plog(LOG_SOCKET, "CMD Use", sd);
            switch(cmdUse(sd, obj1, obj2)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_TAKE: 
        {
            char obj[MAX_NAME_DIM];
            memset(obj, '\0', 1);
            sscanf(msg->payload, "%s", obj);
            
            plog(LOG_SOCKET, "CMD Take", sd);
            switch(cmdTake(sd, obj)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_DROP: 
        {
            char obj[MAX_NAME_DIM];
            memset(obj, '\0', 1);
            sscanf(msg->payload, "%s", obj);

            plog(LOG_SOCKET, "CMD Drop", sd);
            switch(cmdDrop(sd, obj)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_HELP:
        {
            int last_help_msg_id = 0;
            sscanf(msg->payload, "%d", &last_help_msg_id);

            plog(LOG_SOCKET, "CMD Help", sd);
            switch(cmdHelp(sd, last_help_msg_id)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_CMD_END: 
        {
            plog(LOG_SOCKET, "CMD End", sd);
            switch (cmdEnd(sd))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_GAME_PUZZLE_SOL: 
        {
            char obj[MAX_NAME_DIM], solution[MAX_PUZZLE_SOL_DIM];
            memset(obj, '\0', 1); memset(solution, '\0', 1);
            sscanf(msg->payload, "%s %s", obj, solution);

            plog(LOG_SOCKET, "Controllo soluzione enigma", sd);
            switch(checkPuzzleSolution(sd, obj, solution)) 
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                case GAME_END_TIMEOUT: {
                    plog(LOG_SOCKET, "Gioco finito per timeout\n", sd);
                    break;
                }
                case GAME_END_WIN: {
                    plog(LOG_SOCKET, "Gioco finito per token\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_ACTIVE_USERS_LIST: 
        {
            plog(LOG_SOCKET, "SU: Richiesta lista degli utenti in gioco", sd);
            switch(sendActiveUsers(sd))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_USER_SESSION_DATA: 
        {
            char username[MAX_USR_DIM];
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s", username);

            plog(LOG_SOCKET, "SU: Richiesta informazioni sessione", sd);
            switch(sendUserSessionData(sd, username))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_USER_SESSION_OBJS: 
        {
            char username[MAX_USR_DIM];
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s", username);

            plog(LOG_SOCKET, "SU: Richiesta stato degli oggetti di una sessione", sd);
            switch(sendUserSessionObjs(sd, username))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_USER_SESSION_BAG: 
        {
            char username[MAX_USR_DIM];
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s", username);

            plog(LOG_SOCKET, "SU: Richiesta oggetti nello zaino", sd);
            switch(sendUserSessionBag(sd, username))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_USER_SESSION_ALTER_TIME: 
        {
            char username[MAX_USR_DIM]; int seconds = -1; char opt = '\0';
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s %d", username, &seconds);
            opt = msg->payload[strlen(msg->payload) - 1];
            
            plog(LOG_SOCKET, "SU: Richiesta alterazione tempo rimanente", sd);
            switch(alterSessionTime(sd, username, opt, seconds))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        case MSG_SU_REQ_USER_SESSION_SET_HELP: 
        {
            char username[MAX_USR_DIM];
            char help_msg[MAX_HELP_DIM];
            memset(username, '\0', 1);
            sscanf(msg->payload, "%s", username);
            substring(msg->payload, strlen(username) + 1, MAX_HELP_DIM - 1, help_msg);

            plog(LOG_SOCKET, "SU: Richiesta di impostare un messaggio di aiuto", sd);
            switch(setUserSessionHelp(sd, username, help_msg))
            {
                case OK: {
                    plog(LOG_ARROW, "OK\n", sd);
                    break;
                }
                default: {
                    plog(LOG_ARROW, "ERR\n", sd);
                    break;
                }
            }
            break;
        }
        default:
            break;
    }
}

//--------Utils---------//

static void init_fd_set(fd_set* set) 
{
    FD_ZERO(set);
}
static void insert_fd_into_set(int fd, fd_set* set, int* fdmax) 
{
    FD_SET(fd, set);
    if (fd > *fdmax) 
        *fdmax = fd;
}
static void remove_fd_from_set(int fd, fd_set* set) 
{
    FD_CLR(fd, set);
}

static void plog(LOG_TYPE type, const char* msg, int sd)
{
    switch (type)
    {
        case LOG_ERROR: {
            printf("[%s] [Error] %s, ", getStringTimestamp(), msg);
            fflush(stdout);
            perror("");
            break;
        }
        case LOG_CUSTOM_ERROR: {
            printf("[%s] [Error] %s\n", getStringTimestamp(), msg);
            break;
        }
        case LOG_STDIN: {
            printf("[%s] [Comando] %s\n", getStringTimestamp(), msg);
            break;
        }
        case LOG_INFO: {
            printf("[%s] [Info] %s\n", getStringTimestamp(), msg);
            break;
        }
        case LOG_SOCKET: {
            printf("[%s] [Socket %d] %s\n", getStringTimestamp(), sd, msg);
            break;
        }
        case LOG_ARROW: {
            printf("↳ %s\n", msg);
            break;
        }
        default:
            break;
    }
    fflush(stdout);
}

//----------------------//

//---Users Management---//

static bool check_user(const char* username, const char* password) 
{
    char buffer[MAX_USR_DIM + MAX_PSW_DIM + 2];
    char _username[MAX_USR_DIM], _password[MAX_PSW_DIM];

    // Apertura del file utenti
    FILE *fd = fopen("users.txt", "r");
    if (fd == NULL)
    {
        // Il file potrebbe non esistere, proviamo a crearlo
        fd = fopen("users.txt", "a+");
        if (fd == NULL)
            plog(LOG_ERROR, "Apertura file utenti", 0);
        
        // Se sono riuscito a creare il file, questo è vuoto, 
        // quindi l'utente non è sicuramente registrato
        return false;
    }

    // Scorrimento di tutte le righe
    while (fgets(buffer, sizeof(buffer), fd) != NULL) 
    {
        sscanf(buffer, "%s %s", _username, _password);
        if(strcmp(username, _username) == 0 && strcmp(password, _password) == 0)
        {
            fclose(fd);
            return true;
        }
    }

    // Chiusura del file
    fclose(fd);
    return false;
}
static bool register_user(const char* username, const char* password) 
{
    // Controllo se l'utente già esiste
    if (username_exists(username))
        return false;

    // Apertura del file utenti
    FILE* fd = fopen("users.txt", "a");
    if (fd == NULL)
    {
        // Il file potrebbe non esistere, proviamo a crearlo
        fd = fopen("users.txt", "a+");
        if (fd == NULL) {
            plog(LOG_ERROR, "Apertura file utenti", 0);
            return false; /* Non sono riuscito a creare il file, non posso registrare l'utente */
        }
    }
    
    // Scrittura nel file
    fprintf(fd, "%s %s\n", username, password);

    // Chiusura del file
    fclose(fd);
    return true;
}
static bool username_exists(const char* username) 
{
    char buffer[MAX_USR_DIM + MAX_PSW_DIM + 2];
    char _username[MAX_USR_DIM];

    // Apertura del file utenti
    FILE *fd = fopen("users.txt", "r");
    if (fd == NULL)
    {
        // Il file potrebbe non esistere, proviamo a crearlo
        fd = fopen("users.txt", "a+");
        if (fd == NULL)
            plog(LOG_ERROR, "Apertura file utenti", 0);
        
        // Se sono riuscito a creare il file, questo è vuoto, 
        // quindi l'utente non è sicuramente registrato
        return false;
    }

    // Scorrimento di tutte le righe
    while (fgets(buffer, sizeof(buffer), fd) != NULL) 
    {
        sscanf(buffer, "%s", _username);
        if(strcmp(username, _username) == 0)
        {
            fclose(fd);
            return true;
        }
    }
    
    // Chiusura del file
    fclose(fd);
    return false;
}

//----------------------//