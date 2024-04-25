#define MAX_INPUT_DIM 15

#include "lib/utils.h"
#include "lib/game/supervisor.h"
#include "lib/game/ui.h"

typedef enum {
    LOG_ERROR,
    LOG_CUSTOM_ERROR,
    LOG_OK,
    LOG_INFO,
    LOG_CUSTOM
} LOG_TYPE;
static void plog(LOG_TYPE type, const char* msg);

static int init_supervisor(int sd);
static bool show_main_menu(int sd);
static bool main_loop(int sd);
static bool askRetry();

int main(int argc, char* args[]) 
{
    int sd /* Socket di comunicazione */;
    int server_port;

    // Lettura della porta
    if (argc > 2) {
        plog(LOG_CUSTOM_ERROR, "Too many arguments");
        return 0;
    }
    if (argc < 2) {
        plog(LOG_CUSTOM_ERROR, "Please specify the server port");
        return 0;
    }
    if (!get_port(args[1], &server_port)) {
        plog(LOG_CUSTOM_ERROR, "Port not valid");
        return 0;
    }

    // Connessione al server
    sd = init_supervisor(server_port);

    do
    {
        // Visualizzazione del menu principale
        if (!show_main_menu(sd))
            break;
    }
    while(main_loop(sd));

    close(sd);
    return 0;
}

static int init_supervisor(int server_port) 
{
    int sd, ret;
    struct sockaddr_in server_addr;

    // Costruzione indirizzo del server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // Creazione del socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        plog(LOG_ERROR, "Init");
        exit(EXIT_FAILURE);
    }

    // Connessione al server
    while(true) 
    {
        ret = connect(sd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (ret >= 0)
            break;

        // Connessione fallita
        plog(LOG_ERROR, "Init");
        if (askRetry() == false) {
            close(sd);
            exit(EXIT_SUCCESS);
        }
    }

    return sd;
}

static bool askRetry() 
{
    char input[MAX_INPUT_DIM];
    printf("Riprovare? (y/n): ");
    read_line(input, MAX_INPUT_DIM);

    if (strcmp(input, "y") == 0)
        return true;
    return false;
}

static bool main_loop(int sd)
{
    const observed_session* session = getObservedSession();
    char buffer[MAX_INPUT_DIM * 3], cmd[MAX_INPUT_DIM];

    // Recupero le informazioni di base sulla sessione del giocatore
    plog(LOG_INFO, "Recupero informazioni sul giocatore...");
    sleep(1);
    switch (reqUserSessionData(sd))
    {
        case OK: {
            break;
        }
        case SU_ERR_USER_NOT_FOUND: {
            plog(LOG_INFO, "Mhh... probabilmente il giocatore ha terminato la sua sessione di gioco");
            pressEnterToContinue();
            return true;
        }
        case NET_ERR_REMOTE_SOCKET_CLOSED: {
            plog(LOG_CUSTOM_ERROR, "Server disconnesso");
            pressEnterToContinue();
            return false;
        }
        default: {
            plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
            pressEnterToContinue();
            return true;
        }
    }

    // Informazioni ottenute, si trovano nella struttura dati "user"
    while(true) 
    {
        system("clear");
        printf(SU_LOOK_MENU, session->username, session->room_name, session->remaining_time, session->user_token, session->room_token, session->n_bag_objs, session->dim_bag);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM);

        memset(cmd, '\0', 1);
        sscanf(buffer, "%s", cmd);

        if (strcmp("back", cmd) == 0)
        {
            break;
        }
        else if (strcmp("objs", cmd) == 0)
        {
            char** objs_list; int n_objs, i;
            switch (reqUserSessionObjs(sd, &objs_list, &n_objs))
            {
                case OK: 
                {
                    if (n_objs == 0) 
                    {
                        plog(LOG_INFO, "Nella stanza non ci sono oggetti");
                        pressEnterToContinue();
                    }
                    else 
                    {
                        char obj_name[MAX_NAME_DIM];
                        int locked_flag, hidden_flag, consumed_flag;

                        // Mostro gli oggetti a video
                        for (i = 0; i < n_objs; i++) 
                        {
                            sscanf(objs_list[i], "%s %d %d %d", obj_name, &locked_flag, &hidden_flag, &consumed_flag);
                            printf("↳ %-20s [%-10s | %-9s | %-14s]\n", 
                                obj_name,
                                locked_flag == 1 ? "bloccato" : "sbloccato",
                                hidden_flag == 1 ? "nascosto" : "visibile",
                                consumed_flag == 1 ? "consumato" : "non consumato");
                        }
                        pressEnterToContinue();
                    }
                    freeList(objs_list, n_objs);
                    break;
                }
                case SU_ERR_USER_NOT_FOUND: {
                    plog(LOG_INFO, "Mhh... probabilmente il giocatore ha terminato la sua sessione di gioco");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    break;
                }
            }
        }
        else if (strcmp("bag", cmd) == 0) 
        {
            char** objs_list; int n_objs, i;
            switch (reqUserSessionBag(sd, &objs_list, &n_objs))
            {
                case OK: 
                {
                    if (n_objs == 0) 
                    {
                        plog(LOG_INFO, "Lo zaino del giocatore è vuoto");
                        pressEnterToContinue();
                    }
                    else 
                    {
                        // Mostro gli oggetti a video
                        printf("↳ [ ");
                        for (i = 0; i < n_objs; i++) 
                        {
                            if (i == n_objs - 1)
                                printf("%s ]\n", objs_list[i]);
                            else
                                printf("%s, ", objs_list[i]);
                        }
                        pressEnterToContinue();
                    }
                    freeList(objs_list, n_objs);
                    break;
                }
                case SU_ERR_USER_NOT_FOUND: {
                    plog(LOG_INFO, "Mhh... probabilmente il giocatore ha terminato la sua sessione di gioco");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    break;
                }
            }
        }
        else if (strcmp("time", cmd) == 0) 
        {
            op_result ret;
            char opt[MAX_INPUT_DIM];
            int seconds = -1;
            sscanf(buffer, "%s %s %d", cmd, opt, &seconds);

            if (strcmp(opt, "add") == 0) {
                if (seconds < 0)
                    goto sec_err;
                ret = reqUserSessionAddTime(sd, seconds);
            }
            else if (strcmp(opt, "sub") == 0) {
                if (seconds < 0)
                    goto sec_err;
                ret = reqUserSessionSubTime(sd, seconds);
            }
            else if (strcmp(opt, "set") == 0) {
                if (seconds < 0)
                    goto sec_err;
                ret = reqUserSessionSetTime(sd, seconds);
            }
            else {
                plog(LOG_INFO, "Comando non valido");
                pressEnterToContinue();
                continue;
            }

            // Controllo il risultato dell'operazione
            switch (ret)
            {
                case OK: {
                    plog(LOG_OK, "Tempo rimanente modificato");
                    pressEnterToContinue();
                    break;
                }
                case SU_ERR_USER_NOT_FOUND: {
                    plog(LOG_INFO, "Mhh... probabilmente il giocatore ha terminato la sua sessione di gioco");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    break;
                }
            }
            continue;

        sec_err:
            plog(LOG_INFO, "Secondi non validi");
            pressEnterToContinue();
        }
        else if (strcmp("help", cmd) == 0) 
        {
            char help_msg[MAX_HELP_DIM];
            memset(help_msg, '\0', 1);
            printf("Messaggio (%d caratteri): ", MAX_HELP_DIM);
            read_line(help_msg, MAX_HELP_DIM);
            
            if (strlen(help_msg) == 0)
                continue;

            switch (reqUserSessionSetHelp(sd, help_msg))
            {
                case OK: 
                {
                    plog(LOG_OK, "Aiuto inviato!");
                    pressEnterToContinue();
                    break;
                }
                case SU_ERR_USER_NOT_FOUND: {
                    plog(LOG_INFO, "Mhh... probabilmente il giocatore ha terminato la sua sessione di gioco");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    break;
                }
            }
        }
    }

    return true;
}

static bool show_main_menu(int sd) 
{
    char buffer[MAX_INPUT_DIM], cmd[MAX_INPUT_DIM];
    char** users_list;
    int i, n;

update:
    // Chiede i giocatori attivi al server
    plog(LOG_INFO, "Recupero la lista dei giocatori attivi...");
    sleep(1);
    switch(reqActiveUsers(sd, &users_list, &n))
    {
        case OK: {
            plog(LOG_OK, "Pronti!");
            sleep(2);
            break;
        }
        case NET_ERR_REMOTE_SOCKET_CLOSED: {
            plog(LOG_CUSTOM_ERROR, "Server disconnesso");
            pressEnterToContinue();
            return false;
        }
        default: {
            plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
            pressEnterToContinue();
            return false;
        }
    }

    while(true) 
    {
        system("clear");
        printf(SU_MAIN_MENU_HEADER);
        
        // Mostra gli utenti in gioco
        printf("\nUtenti in gioco:\n");
        for (i = 0; i < n; i++) {
            printf("%d) %s\n", i+1, users_list[i]);
        }
        if (n == 0)
            printf("Nessuno\n");
        printf("\n");

        printf(SU_MAIN_MENU_TRAILER);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM);

        memset(cmd, '\0', 1);
        sscanf(buffer, "%s", cmd);

        if (strcmp("quit", cmd) == 0)
        {
            freeList(users_list, n);
            return false;
        }
        else if (strcmp("look", cmd) == 0) 
        {
            int user_id = 0;
            sscanf(buffer, "%s %d", cmd, &user_id);

            if (user_id <= 0 || user_id > n) 
            {
                plog(LOG_CUSTOM_ERROR, "Numero utente non valido");
                pressEnterToContinue();
                continue;
            }
            user_id--;

            selectUser(users_list[user_id]);
            free(users_list);
            return true;
        }
        else if (strcmp("update", cmd) == 0) {
            freeList(users_list, n);
            goto update;
        }
    }
}

static void plog(LOG_TYPE type, const char* msg)
{
    switch (type)
    {
        case LOG_ERROR: {
            printf("[Error] %s, ", msg);
            fflush(stdout);
            perror("");
            break;
        }
        case LOG_CUSTOM_ERROR: {
            printf("[Error] %s\n", msg);
            break;
        }
        case LOG_INFO: {
            printf("[Info] %s\n", msg);
            break;
        }
        case LOG_OK: {
            printf("[OK] %s\n", msg);
            break;
        }
        case LOG_CUSTOM: {
            printf("%s\n", msg);
            break;
        }
        default:
            break;
    }
    fflush(stdout);
}