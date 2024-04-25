#define MAX_INPUT_DIM 15

#include "lib/utils.h"
#include "lib/game/client.h"
#include "lib/game/ui.h"

typedef enum {
    LOG_ERROR,
    LOG_CUSTOM_ERROR,
    LOG_OK,
    LOG_INFO,
    LOG_CUSTOM
} LOG_TYPE;
static void plog(LOG_TYPE type, const char* msg);

static int init_client(int sd);

static bool show_auth_menu(int sd);
static bool show_main_menu(int sd);
static bool game_loop(int sd);
static void askPuzzle(int sd, const char* obj, const char* text);
static bool askRetry();
static bool askUsernamePassword(char* username, char* password);

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
    sd = init_client(server_port);

    // Visualizzazione del menu di autenticazione
    if (!show_auth_menu(sd))
        goto quit;

    do 
    {
        // Visualizzazione del menu principale
        if (!show_main_menu(sd))
            break;
    }
    while(game_loop(sd));

quit:
    close(sd);
    return 0;
}

static int init_client(int server_port) 
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

static bool game_loop(int sd)
{
    const game_state* state = getGameState();
    char buffer[MAX_INPUT_DIM * 3 + 3], cmd[MAX_INPUT_DIM], arg1[MAX_INPUT_DIM], arg2[MAX_INPUT_DIM];
    
    while (true)
    {
        if(state->game_finished != FINISHED_NO)
        {
            // Il gioco è finito
            system("clear");

            if (state->game_finished == FINISHED_WIN)
                printf(CLI_GAME_WIN, state->current_description);
            else if (state->game_finished == FINISHED_TIMEOUT)
                printf(CLI_GAME_TIMEOUT, state->user_token, state->room_token, state->current_description);
            else
                printf(CLI_GAME_END, state->user_token, state->room_token, state->current_description);

            pressEnterToContinue();
            return true;
        }

        system("clear");
        if (state->current_help_msg_id == 0) {
            printf(CLI_GAME_MENU_HEADER, state->remaining_time, state->user_token, state->room_token, state->objs_in_bag, state->bag_size, state->current_description);
        }
        else {
            printf(CLI_GAME_MENU_HELP_HEADER, state->remaining_time, state->user_token, state->room_token, state->objs_in_bag, state->bag_size, state->current_description, state->current_help_msg);
        }
        if (state->current_help_msg_id != state->last_help_msg_id) {
            // Nuovo messaggio di aiuto disponibile
            printf(CLI_GAME_MENU_NEW_HELP_MSG);
        }
        printf(CLI_GAME_MENU_CMDS);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM * 3 + 3);

        memset(cmd, '\0', 1); memset(arg1, '\0', 1); memset(arg2, '\0', 1);
        sscanf(buffer, "%s %s %s", cmd, arg1, arg2);
        
        if (strcmp("look", cmd) == 0)
        {
            switch (cmdLook(sd, arg1))
            {
                case OK:
                    break;
                case GAME_ERR_NOT_FOUND: {
                    plog(LOG_INFO, "Hmm... sembra che non ci sia nulla con il nome specificato");
                    pressEnterToContinue();
                    break;
                }
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }
        }
        else if (strcmp("take", cmd) == 0)
        {
            char* puzzle_text = NULL;
            if (strlen(arg1) == 0) 
            {
                plog(LOG_INFO, "Hmm... non hai specificato l'oggetto da raccogliere");
                pressEnterToContinue();
                continue;
            }

            switch (cmdTake(sd, arg1, &puzzle_text))
            {
                case OK: {
                    plog(LOG_OK, "Oggetto raccolto!");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_LOCK_PUZZLE: {
                    askPuzzle(sd, arg1, puzzle_text);
                    break;
                }
                case GAME_INF_BAG_FULL: {
                    plog(LOG_INFO, "Hmm... lo zaino è pieno!");
                    pressEnterToContinue();
                    break;
                }
                case GAME_ERR_NOT_FOUND: {
                    plog(LOG_INFO, "Hmm... non ho trovato l'oggetto specificato");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_OBJ_TAKEN: {
                    plog(LOG_INFO, "Hmm... l'oggetto è già nello zaino");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_OBJ_NO_TAKE:
                case GAME_INF_OBJ_CONSUMED:
                case GAME_INF_LOCK_ACTION: {
                    plog(LOG_INFO, "Hmm... non riesco a raccogliere l'oggetto");
                    pressEnterToContinue();
                    break;
                }
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }

            if (puzzle_text != NULL)
                free(puzzle_text);
        }
        else if (strcmp("use", cmd) == 0)
        {
            char* descr = NULL;
            if (strlen(arg1) == 0) 
            {
                plog(LOG_INFO, "Hmm... non hai specificato l'oggetto da usare");
                pressEnterToContinue();
                continue;
            }

            switch (cmdUse(sd, arg1, arg2, &descr))
            {
                case OK: {
                    // Mostro la descrizione dell'uso
                    plog(LOG_OK, descr);
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_LOCK_PUZZLE: {
                    if (strlen(arg2) == 0) // Gestione uso singolo
                        askPuzzle(sd, arg1, descr);
                    else // Gestione uso combinato
                        // L'oggetto 2 è bloccato da un enigma
                        askPuzzle(sd, arg2, descr);

                    break;
                }
                case GAME_ERR_NOT_FOUND: 
                {
                    if (strlen(arg2) == 0) // Gestione uso singolo
                        plog(LOG_INFO, "Hmm... non ho trovato l'oggetto specificato");
                    else // Gestione uso combinato
                        plog(LOG_INFO, "Hmm... non ho trovato gli oggetti specificati");

                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_OBJ_NOT_TAKEN: {
                    // Solo nel caso di uso combinato
                    printf("[Info] Non hai '%s' nello zaino\n", arg1);
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_OBJ_CONSUMED: 
                {
                    if (strlen(arg2) == 0) // Gestione uso singolo
                        plog(LOG_INFO, "Hmm... hai già usato quest'oggetto");
                    else // Gestione uso combinato
                        printf("[Info] '%s' già usato\n", arg2);

                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_LOCK_ACTION:
                case GAME_INF_OBJ_NO_USE: {
                    plog(LOG_INFO, "Hmm... non è successo niente");
                    pressEnterToContinue();
                    break;
                }
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }

            if (descr != NULL)
                free(descr);
        }
        else if (strcmp("objs", cmd) == 0)
        {
            char** objs_list; int n_objs, i;
            switch (cmdObjs(sd, &objs_list, &n_objs))
            {
                case OK: 
                {
                    if (n_objs == 0) 
                    {
                        plog(LOG_INFO, "Lo zaino è vuoto");
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
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }
        }
        else if (strcmp("drop", cmd) == 0)
        {
            if (strlen(arg1) == 0) 
            {
                plog(LOG_INFO, "Hmm... non hai specificato l'oggetto da lasciare");
                pressEnterToContinue();
                continue;
            }
            
            switch (cmdDrop(sd, arg1))
            {
                case OK: {
                    plog(LOG_OK, "Oggetto rimosso!");
                    pressEnterToContinue();
                    break;
                }
                case GAME_ERR_NOT_FOUND: {
                    plog(LOG_INFO, "Hmm... non ho trovato l'oggetto specificato");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_OBJ_NOT_TAKEN: {
                    plog(LOG_INFO, "Hmm... nel tuo zaino non c'è nulla con questo nome");
                    pressEnterToContinue();
                    break;
                }
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }
        }
        else if (strcmp("help", cmd) == 0) 
        {
            switch (cmdHelp(sd))
            {
                case OK: 
                {
                    plog(LOG_OK, "Messaggio aggiornato");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_HELP_NO_NEW_MSG:
                {
                    plog(LOG_INFO, "Non c'è un nuovo messaggio");
                    pressEnterToContinue();
                    break;
                }
                case GAME_INF_HELP_NO_MSG: {
                    plog(LOG_INFO, "Non c'è nessun messaggio");
                    pressEnterToContinue();
                    break;
                }
                case GAME_END_WIN:
                case GAME_END_TIMEOUT: {
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
                    break;
                }
            }
        }
        else if (strcmp("end", cmd) == 0)
        {
            switch (cmdEnd(sd))
            {
                case GAME_END_QUIT: {
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
                }
            }
        }
    }
}

static void askPuzzle(int sd, const char* obj, const char* text)
{
    char solution[MAX_PUZZLE_SOL_DIM];
    // Mostra l'engma a video
    printf("> %s è bloccato da un enigma\n", obj);
    printf("↳ %s\n  ↳ ", text);
    // Legge la risposta
    read_line(solution, MAX_PUZZLE_SOL_DIM);
    // Invia la risposta
    switch(sendPuzzleSolution(sd, obj, solution))
    {
        case OK: {
            // Oggetto sbloccato con successo
            plog(LOG_OK, "Oggetto sbloccato!");
            pressEnterToContinue();
            break;
        }
        case GAME_INF_PUZZLE_WRONG: {
            // Risposta sbagliata
            plog(LOG_INFO, "Risposta sbagliata!");
            pressEnterToContinue();
            break;
        }
        case GAME_END_WIN:
        case GAME_END_TIMEOUT: {
            break;
        }
        default: {
            plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
            pressEnterToContinue();
            break;
        }
    }
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

static bool show_main_menu(int sd) 
{
    char buffer[MAX_INPUT_DIM * 2], cmd[MAX_INPUT_DIM];
    char** room_names;
    int i, n;

    // Chiede le stanze disponibili al server
    plog(LOG_INFO, "Recupero la lista delle stanze...");
    sleep(1);
    switch(reqRoomNames(sd, &room_names, &n))
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
        printf(CLI_MAIN_MENU_HEADER);
        
        // Mostra le stanze disponibili
        printf("\nStanze disponibili:\n");
        for (i = 0; i < n; i++) {
            printf("%d) %s\n", i+1, room_names[i]);
        }
        printf("\n");

        printf(CLI_MAIN_MENU_TRAILER);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM);

        memset(cmd, '\0', 1);
        sscanf(buffer, "%s", cmd);

        if (strcmp("quit", cmd) == 0)
        {
            freeList(room_names, n);
            return false;
        } 
        else if (strcmp("start", cmd) == 0) 
        {
            int room_id = 0;
            sscanf(buffer, "%s %d", cmd, &room_id);

            if (room_id <= 0 || room_id > n) 
            {
                plog(LOG_CUSTOM_ERROR, "Numero della stanza non valido");
                pressEnterToContinue();
                continue;
            }

            // Comunica al server la stanza scelta
            room_id -= 1;
            switch (reqStartGame(sd, room_id))
            {
                case OK: {
                    freeList(room_names, n);
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    freeList(room_names, n);

                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                case GAME_ERR_INVALID_ROOM: {
                    plog(LOG_CUSTOM_ERROR, "Numero della stanza non valido");
                    pressEnterToContinue();
                    break;
                }
                case AUTH_ERR_NO_AUTH: {
                    plog(LOG_CUSTOM_ERROR, "Non sei autenticato");
                    pressEnterToContinue();
                    break;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    break;
                }
            }
        }
    }

    freeList(room_names, n);
    return true;
}

static bool show_auth_menu(int sd) 
{
    char buffer[MAX_INPUT_DIM];
    char username[MAX_USR_DIM], password[MAX_PSW_DIM];

    while(true)
    {
        system("clear");
        printf(CLI_AUTH_MENU);
        printf("> ");
        read_line(buffer, MAX_INPUT_DIM);

        if (strcmp("quit", buffer) == 0) 
        {
            return false;
        }
        else if (strcmp("login", buffer) == 0)
        {
            if (!askUsernamePassword(username, password))
                continue;

            switch(reqLogin(sd, username, password)) 
            {
                case OK: {
                    plog(LOG_OK, "Autenticazione riuscita");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                case AUTH_ERR_INVALID_CREDENTIALS: {
                    plog(LOG_CUSTOM_ERROR, "Username o password errati");
                    pressEnterToContinue();
                    continue;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    continue;
                }
            }
        }
        else if (strcmp("signup", buffer) == 0) 
        {
            if (!askUsernamePassword(username, password))
                continue;
            
            switch(reqSignup(sd, username, password)) 
            {
                case OK: {
                    plog(LOG_OK, "Registrazione riuscita");
                    pressEnterToContinue();
                    return true;
                }
                case NET_ERR_REMOTE_SOCKET_CLOSED: {
                    plog(LOG_CUSTOM_ERROR, "Server disconnesso");
                    pressEnterToContinue();
                    return false;
                }
                case AUTH_ERR_USERNAME_EXISTS: {
                    plog(LOG_CUSTOM_ERROR, "Username già esistente");
                    pressEnterToContinue();
                    continue;
                }
                default: {
                    plog(LOG_CUSTOM_ERROR, "Si è verificato un problema durante la richiesta");
                    pressEnterToContinue();
                    continue;
                }
            }
        }
    }

    return false;
}

static bool askUsernamePassword(char* username, char* password) 
{
    printf("↳ Username: ");
    read_line(username, MAX_USR_DIM);
    trim(username);
    if (strlen(username) == 0) {
        printf("  ↳ Username non valido...\n");
        pressEnterToContinue();
        return false;
    }

    printf("↳ Password: ");
    read_line(password, MAX_PSW_DIM);
    trim(password);
    if (strlen(password) == 0) {
        printf("  ↳ Password non valida...\n");
        pressEnterToContinue();
        return false;
    }

    return true;
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