#include "client.h"

static op_result receive_list(int sd, char*** list, int* n);
static op_result receiveState(int sd);

/*
 * Contiene le informazioni di stato sulla sessione di gioco corrente
 */
static game_state state = {
    .username = "\0",
    .current_description = "\0",
    .current_help_msg = "\0",
    .game_finished = FINISHED_NO
};

/*
 * Riceve una lista di elementi.
 * Restituisce la lista degli elementi e il numero totale di elementi ricevuti.
 * La memoria allocata per la lista deve essere liberata dall'utente dopo l'uso utilizzando la funzione freeList.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - list: Puntatore a un array di stringhe che conterrà gli elementi della lista.
 *   - n: Puntatore a una variabile che conterrà il numero totale di elementi.
 *
 * Restituisce:
 *   - OK se la ricezione è stata gestita correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_RECV in caso di errore nella ricezione.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 *   - ERR_OTHER se si verificano altri errori durante la ricezione o l'allocazione di memoria.
 */
static op_result receive_list(int sd, char*** list, int* n) 
{
    op_result ret;
    desc_msg msg;
    int i;

    // Riceve il numero totale di elementi
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Controllo del tipo di messaggio ricevuto
    if (msg.type != MSG_LIST_START)
        return ERR_UNEXPECTED_MSG_TYPE;

    // Memorizza il numero totale di elementi e la loro dimensione
    sscanf(msg.payload, "%d", n);

    // Alloca dinamicamente il vettore di stringhe per gli item della lista
    *list = (char **)malloc((*n) * sizeof(char*));

    // Verifica se l'allocazione di memoria è riuscita
    if (*list == NULL) {
        // Gestisci l'errore e esci
        return ERR_OTHER;
    }
    
    // Riceve gli item e li memorizza nell'array *list
    for (i = 0; i < *n; i++) 
    {
        ret = receive_from_socket(sd, &msg);
        if (ret != OK) {
            // Gestisci l'errore e libera la memoria allocata finora
            freeList(*list, i);
            return ret;
        }

        // Controlla se il messaggio ricevuto è del tipo corretto
        if (msg.type != MSG_LIST_ITEM) {
            // Gestisci l'errore e libera la memoria allocata finora
            freeList(*list, i);
            return ERR_UNEXPECTED_MSG_TYPE;
        }

        // Alloca memoria per l'item
        (*list)[i] = (char*)malloc((strlen(msg.payload) + 1) * sizeof(char));

        // Verifica se l'allocazione di memoria è riuscita
        if ((*list)[i] == NULL) {
            // Gestisci l'errore e libera la memoria allocata finora
            freeList(*list, i);
            return ERR_OTHER;
        }
        
        sprintf((*list)[i], "%s", msg.payload);
    }

    return ret;
}

// Libera la memoria allocata per la ricezione degli item di una lista
void freeList(char** list, int n) {
    int i;
    for (i = 0; i < n; i++) {
        free(list[i]);
    }
    free(list);
}

/*
 * Invia una richiesta di login al server attraverso il socket specificato,
 * fornendo il nome utente e la password. Attende la risposta del server.
 *
 * Parametri:
 *   - sd: Descrittore del socket attraverso il quale inviare la richiesta e ricevere la risposta.
 *   - username: Nome utente per l'autenticazione.
 *   - password: Password per l'autenticazione.
 *
 * Restituisce:
 *   - OK se la richiesta e la risposta sono riuscite e l'autenticazione è avvenuta con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - AUTH_ERR_INVALID_CREDENTIALS se il server ha rifiutato la richiesta di login a causa di credenziali errate.
 *   - ERR_OTHER se il server ha rifiutato la richiesta di login per motivi non specificati o sconosciuti.
 */
op_result reqLogin(int sd, const char* username, const char* password) 
{
    op_result ret;
    desc_msg msg;

    // Prepara un messaggio di richiesta di login con username e password forniti
    init_msg(&msg, MSG_REQ_LOGIN, username, password);

    // Invia il messaggio al server
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve la risposta dal server
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Verifica se il server ha accettato la richiesta di login
    if (msg.type == MSG_AUTH_ERR_INVALID_CREDENTIALS)
        ret = AUTH_ERR_INVALID_CREDENTIALS;
    else if (msg.type != MSG_SUCCESS)
        ret = ERR_OTHER;

    strncpy(state.username, username, MAX_USR_DIM);
    state.username[MAX_USR_DIM - 1] = '\0';
    return ret;
}

/*
 * Invia una richiesta di registrazione al server attraverso il socket specificato,
 * fornendo il nome utente e la password. Attende la risposta del server.
 *
 * Parametri:
 *   - sd: Descrittore del socket attraverso il quale inviare la richiesta e ricevere la risposta.
 *   - username: Nome utente per la registrazione.
 *   - password: Password per la registrazione.
 *
 * Restituisce:
 *   - OK se la richiesta e la risposta sono riuscite e la registrazione è avvenuta con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - AUTH_ERR_USERNAME_EXISTS se il server ha rifiutato la richiesta di registrazione perché il nome utente esiste già.
 *   - ERR_OTHER se il server ha rifiutato la richiesta di registrazione per motivi non specificati o sconosciuti.
 */
op_result reqSignup(int sd, const char* username, const char* password) 
{
    op_result ret;
    desc_msg msg;

    // Prepara un messaggio di richiesta di registrazione con username e password forniti
    init_msg(&msg, MSG_REQ_SIGNUP, username, password);

    // Invia il messaggio al server
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve la risposta dal server
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Verifica se il server ha accettato la richiesta di registrazione
    if (msg.type == MSG_AUTH_ERR_USERNAME_EXISTS)
        ret = AUTH_ERR_USERNAME_EXISTS;
    else if (msg.type != MSG_SUCCESS)
        ret = ERR_OTHER;

    strncpy(state.username, username, MAX_USR_DIM);
    state.username[MAX_USR_DIM - 1] = '\0';
    return ret;
}

/*
 * Richiede al server i nomi delle stanze disponibili.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - room_names: Puntatore a un array di stringhe per memorizzare i nomi delle stanze.
 *   - n: Puntatore al numero totale di stanze ricevute.
 *
 * Restituisce:
 *   - OK se la richiesta e la ricezione sono riuscite.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione dei messaggi dal server.
 *
 * La funzione recupera i nomi delle stanze inviando una richiesta al server.
 * Riceve il numero totale di stanze e procede a ricevere i nomi delle stanze, memorizzandoli in room_names.
 * Il numero totale di stanze ricevute è restituito attraverso il puntatore n.
 * La memoria allocata per room_names deve essere liberata dall'utente dopo l'uso.
 */
op_result reqRoomNames(int sd, char*** room_names, int* n) 
{
    op_result ret;
    desc_msg msg;

    // Invia la richiesta al server per ottenere i nomi delle stanze
    init_msg(&msg, MSG_REQ_ROOM_NAMES);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve i nomi delle stanze
    return receive_list(sd, room_names, n);
}

/*
 * Restituisce:
 *   - Puntatore costante alla struttura contenente informazioni di stato sulla sessione di gioco corrente
 */
const game_state* getGameState() 
{
    return &state;
}

/*
 * Invia al server la richiesta di avviare la sessione di gioco nella stanza indicata e riceve le informazioni necessarie per l'inizio del gioco.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - room: Numero identificativo della stanza in cui si desidera iniziare il gioco.
 *
 * Restituisce:
 *   - OK se la richiesta e la ricezione dei dati sono avvenute con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione dai messaggi del server.
 *   - AUTH_ERR_NO_AUTH se l'utente non si è autenticato.
 *   - GAME_ERR_INVALID_ROOM se la stanza specificata non è valida.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto dal server ha un tipo inaspettato.
 *
 * La funzione invia al server la richiesta di avviare la sessione di gioco nella stanza specificata.
 * Successivamente, riceve le informazioni iniziali necessarie per l'inizio del gioco.
 * I dati ricevuti vengono memorizzati nella struttura game_state.
 */
op_result reqStartGame(int sd, unsigned short room) 
{
    op_result ret;
    desc_msg msg;

    // Controlla che l'utente sia autenticato
    if (strlen(state.username) == 0) {
        // L'utente non si è autenticato
        return AUTH_ERR_NO_AUTH;
    }

    // Invia al server la richiesta di avviare la sessione di gioco nella stanza indicata
    init_msg(&msg, MSG_REQ_START_GAME, state.username, room);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Riceve i dati inziali
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Controlla se il messaggio ricevuto è del tipo corretto
    switch (msg.type)
    {
        case MSG_GAME_INIT: {
            break;
        }
        case MSG_GAME_ERR_INVALID_ROOM: {
            return GAME_ERR_INVALID_ROOM;
        }
        case MSG_GAME_ERR_INIT: {
            return GAME_ERR_INIT;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }

    // Recupero dei dati dal payload e memorizzazione nella struttura game_state
    sscanf(msg.payload, "%lld %d %d", (long long*) &state.remaining_time, &state.bag_size, &state.room_token);
    state.room = room;
    state.user_token = 0;
    state.objs_in_bag = 0;
    state.game_finished = FINISHED_NO;
    state.current_help_msg_id = 0;
    memset(state.current_help_msg, '\0', MAX_HELP_DIM);

    // Riceve la storia della room dal server
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Recupero dei dati dal payload e memorizzazione nella struttura game_state
    strncpy(state.current_description, msg.payload, MAX_DESCR_DIM);
    state.current_description[MAX_DESCR_DIM - 1] = '\0';

    return ret;
}

/*
 * Riceve lo stato attuale del gioco inviato dal server e aggiorna lo stato del client.
 * Gestisce anche i messaggi di fine gioco (vittoria o timeout).
 *
 * Parametri:
 *   - sd: Descrittore del socket utilizzato per la comunicazione con il server.
 *
 * Restituisce:
 *   - OK se lo stato è stato ricevuto con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il server non consente l'esecuzione di un comando
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
static op_result receiveState(int sd) 
{
    desc_msg msg;
    op_result ret;

    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Controllo del tipo di messaggio ricevuto
    switch (msg.type)
    {
        case MSG_GAME_STATE: {
            break;
        }
        case MSG_GAME_END_WIN:
        {
            // Il giocatore ha vinto, leggo il messaggio di fine gioco inviato dal server
            strncpy(state.current_description, msg.payload, MAX_DESCR_DIM);
            state.current_description[MAX_DESCR_DIM - 1] = '\0';

            state.game_finished = FINISHED_WIN;
            return GAME_END_WIN;
        }
        case MSG_GAME_END_TIMEOUT:
        {
            // Il tempo è scaduto, leggo il messaggio di fine gioco inviato dal server
            strncpy(state.current_description, msg.payload, MAX_DESCR_DIM);
            state.current_description[MAX_DESCR_DIM - 1] = '\0';
            
            state.game_finished = FINISHED_TIMEOUT;
            return GAME_END_TIMEOUT;
        }
        case MSG_GAME_ERR_CMD_NOT_ALLOWED: {
            return GAME_ERR_CMD_NOT_ALLOWED;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }

    // Aggiorna lo stato di gioco con i dati ricevuti
    sscanf(msg.payload, "%lld %d %d %d", (long long*) &state.remaining_time, &state.user_token, &state.objs_in_bag, &state.last_help_msg_id);
    return ret;
}

/*
 * Invia al server il comando "look" per ottenere la descrizione di un oggetto, una locazione o dell'intera stanza.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - what: Nome dell'oggetto o della locazione di cui si desidera ottenere la descrizione.
 *
 * Restituisce:
 *   - OK se l'invio del comando e la ricezione dei dati sono avvenuti con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del comando al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione dei messaggi dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_ERR_NOT_FOUND se l'oggetto o la locazione specificati non sono presenti nella stanza.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto dal server ha un tipo inaspettato.
 *
 * La funzione invia al server il comando "look" specificando l'oggetto o la locazione di interesse.
 * Se il comando è consentito, aggiorna lo stato di gioco con i dati ricevuti. 
 * Se l'oggetto o la locazione specificati non esistono, il server invia un messaggio di errore.
 */
op_result cmdLook(int sd, const char* what) 
{
    op_result ret;
    desc_msg msg;

    // Invia il comando "look" specificando l'oggetto o la locazione di interesse
    init_msg(&msg, MSG_GAME_CMD_LOOK, what);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;
    
    // Riceve la descrizione dell'oggetto, della locazione o dell'intera stanza
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Controllo del tipo di messaggio ricevuto
    if (msg.type == MSG_GAME_ERR_NOT_FOUND)
        return GAME_ERR_NOT_FOUND;
    else if (msg.type != MSG_GAME_DESCR)
        return ERR_UNEXPECTED_MSG_TYPE;

    // Aggiorna lo stato di gioco con la descrizione ricevuta
    strncpy(state.current_description, msg.payload, MAX_DESCR_DIM);
    state.current_description[MAX_DESCR_DIM - 1] = '\0';

    return ret;
}

/*
 * Invia al server il comando "objs" per richiedere la lista degli oggetti nello zaino del giocatore.
 * Aggiorna lo stato di gioco con i dati ricevuti in risposta.
 * Restituisce la lista degli oggetti nello zaino e il numero totale di oggetti.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - objs_names: Puntatore a un array di stringhe che conterrà i nomi degli oggetti.
 *   - n: Puntatore a una variabile che conterrà il numero totale di oggetti.
 *
 * Restituisce:
 *   - OK se la richiesta è stata gestita correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 * 
 * La funzione recupera i nomi degli oggetti inviando una richiesta al server.
 * Riceve il numero totale di oggetti e procede a ricevere i nomi, memorizzandoli in objs_names.
 * Il numero totale di oggetti ricevuti è restituito attraverso il puntatore n.
 * La memoria allocata per objs_names deve essere liberata dall'utente dopo l'uso, utilizzando la funzione freeList.
 */
op_result cmdObjs(int sd, char*** objs_names, int* n) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server il comando "objs" per richiedere la lista degli oggetti nello zaino
    init_msg(&msg, MSG_GAME_CMD_OBJS);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve gli oggetti
    return receive_list(sd, objs_names, n);
}

/*
 * Invia al server il comando "use" per utilizzare uno o due oggetti nel gioco.
 * Riceve lo stato aggiornato del giocatore e il risultato dell'operazione.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - obj1_name: Nome del primo oggetto da utilizzare.
 *   - obj2_name: Nome del secondo oggetto da utilizzare.
 *   - out: Puntatore a una stringa che conterrà la descrizione dell'uso o il testo dell'enigma, se presente.
 *
 * Restituisce:
 *   - OK se l'operazione è stata eseguita con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_ERR_NOT_FOUND se uno o entrambi gli oggetti specificati non sono stati trovati nel gioco.
 *   - GAME_INF_OBJ_CONSUMED se uno degli oggetti è stato consumato.
 *   - GAME_INF_LOCK_PUZZLE se l'oggetto 2 è bloccato da un enigma. Il testo dell'enigma è restituito tramite out
 *   - GAME_INF_LOCK_ACTION se uno dei due oggetti è bloccato e si sbloccherà in seguito all'uso di altri oggetti.
 *   - GAME_INF_OBJ_NOT_TAKEN se il giocatore non ha nel proprio zaino l'oggetto 1 che vuole combinare con l'oggetto 2.
 *   - GAME_INF_OBJ_NO_USE se non è possibile utilizzare gli oggetti in questo modo.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result cmdUse(int sd, const char* obj1_name, const char* obj2_name, char** out) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server il comando "use"
    init_msg(&msg, MSG_GAME_CMD_USE, obj1_name, obj2_name);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve il risultato dell'esecuzione del comando
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    switch (msg.type)
    {
        case MSG_GAME_ERR_CMD_NOT_ALLOWED: {
            return GAME_ERR_CMD_NOT_ALLOWED;
        }
        case MSG_GAME_ERR_NOT_FOUND: {
            // Uno o più oggetti non trovati
            return GAME_ERR_NOT_FOUND;
        }
        case MSG_GAME_INF_OBJ_NOT_TAKEN: {
            // Il giocatore non ha nello zaino l'oggetto 1 che vuole combinare con l'oggetto
            return GAME_INF_OBJ_NOT_TAKEN;
        }
        case MSG_GAME_INF_OBJ_NO_USE: {
            // Non è possibile utilizzare gli oggetti in questo modo
            return GAME_INF_OBJ_NO_USE;
        }
        case MSG_GAME_INF_OBJ_CONSUMED: {
            // Uno degli oggetti è stato consumato
            return GAME_INF_OBJ_CONSUMED;
        }
        case MSG_GAME_INF_LOCK_PUZZLE: {
            // L'oggetto è bloccato da un enigma
            ret = GAME_INF_LOCK_PUZZLE;
            break;
        }
        case MSG_GAME_INF_LOCK_ACTION: {
            // L'oggetto è bloccato. Si sbloccherà in seguito all'uso di altri oggetti
            return GAME_INF_LOCK_ACTION;
        }
        case MSG_GAME_DESCR: {
            // Esecuzione del comando avvenuta con successo
            ret = OK;
            break;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }

    // Estrae la descrizione o il testo dell'enigma dal payload del messaggio
    *out = (char*)malloc((strlen(msg.payload) + 1) * sizeof(char));
    if (*out == NULL)
        return ERR_OTHER;

    sprintf(*out, "%s", msg.payload);
    return ret;
}

/*
 * Invia al server il comando "take" per raccogliere un oggetto nel gioco.
 * Riceve lo stato aggiornato del giocatore e il risultato dell'operazione.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - obj_name: Nome dell'oggetto da raccogliere.
 *   - puzzle_text: Puntatore a una stringa che conterrà il testo dell'enigma, se presente.
 *
 * Restituisce:
 *   - OK se l'oggetto è stato raccolto con successo e il messaggio di conferma è stato ricevuto correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_ERR_NOT_FOUND se l'oggetto specificato non è stato trovato nel gioco.
 *   - GAME_INF_BAG_FULL se lo zaino del giocatore è pieno.
 *   - GAME_INF_OBJ_CONSUMED se l'oggetto è stato consumato e non può essere raccolto.
 *   - GAME_INF_OBJ_TAKEN se l'oggetto è già stato raccolto.
 *   - GAME_INF_LOCK_ACTION se l'oggetto è bloccato e si sbloccherà in seguito all'uso di altri oggetti.
 *   - GAME_INF_LOCK_PUZZLE se l'oggetto è bloccato da un enigma. Il testo dell'enigma è restituito tramite puzzle_text.
 *   - GAME_INF_OBJ_NO_TAKE se l'oggetto non può essere raccolto.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result cmdTake(int sd, const char* obj_name, char** puzzle_text) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server il comando "take"
    init_msg(&msg, MSG_GAME_CMD_TAKE, obj_name);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve il risultato dell'esecuzione del comando
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    switch (msg.type)
    {
        case MSG_GAME_ERR_CMD_NOT_ALLOWED: {
            return GAME_ERR_CMD_NOT_ALLOWED;
        }
        case MSG_GAME_ERR_NOT_FOUND: {
            // Oggetto non trovato
            return GAME_ERR_NOT_FOUND;
        }
        case MSG_GAME_INF_OBJ_NO_TAKE: {
            // L'oggetto non si può raccogliere
            return GAME_INF_OBJ_NO_TAKE;
        }
        case MSG_GAME_INF_BAG_FULL: {
            // Lo zaino è pieno
            return GAME_INF_BAG_FULL;
        }
        case MSG_GAME_INF_OBJ_CONSUMED: {
            // Non è possibile raccogliere un oggetto consumato
            return GAME_INF_OBJ_CONSUMED;
        }
        case MSG_GAME_INF_OBJ_TAKEN: {
            // Oggetto già raccolto
            return GAME_INF_OBJ_TAKEN;
        }
        case MSG_GAME_INF_LOCK_ACTION: {
            // L'oggetto è bloccato. Si sbloccherà in seguito all'uso di altri oggetti
            return GAME_INF_LOCK_ACTION;
        }
        case MSG_GAME_INF_LOCK_PUZZLE: {
            // L'oggetto è bloccato da un enigma
            ret = GAME_INF_LOCK_PUZZLE;
            break;
        }
        case MSG_SUCCESS: {
            // Esecuzione del comando avvenuta con successo
            return OK;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }

    // Estrae la descrizione o il testo dell'enigma dal payload del messaggio
    *puzzle_text = (char*)malloc((strlen(msg.payload) + 1) * sizeof(char));
    if (*puzzle_text == NULL)
        return ERR_OTHER;

    sprintf(*puzzle_text, "%s", msg.payload);
    return ret;
}

/*
 * Invia al server il comando "drop" per rimuovere un oggetto dallo zaino del giocatore.
 * Riceve lo stato aggiornato del giocatore e il risultato dell'operazione.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - obj_name: Nome dell'oggetto da rimuovere dallo zaino.
 *
 * Restituisce:
 *   - OK se l'oggetto è stato rimosso con successo dallo zaino.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_ERR_NOT_FOUND se l'oggetto specificato non è stato trovato.
 *   - GAME_INF_OBJ_NOT_TAKEN se l'oggetto non è presente nello zaino del giocatore.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result cmdDrop(int sd, const char* obj_name) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server il comando "drop"
    init_msg(&msg, MSG_GAME_CMD_DROP, obj_name);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve il risultato dell'esecuzione del comando
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    switch (msg.type)
    {
        case MSG_GAME_ERR_CMD_NOT_ALLOWED: {
            return GAME_ERR_CMD_NOT_ALLOWED;
        }
        case MSG_GAME_ERR_NOT_FOUND: {
            return GAME_ERR_NOT_FOUND;
        }
        case MSG_GAME_INF_OBJ_NOT_TAKEN: {
            return GAME_INF_OBJ_NOT_TAKEN;
        }
        case MSG_SUCCESS: {
            return OK;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }
}

/*
 * Richiede al server il messaggio di aiuto più recente e lo aggiorna se necessario.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 * 
 * Restituisce:
 *   - OK se il messaggio di aiuto è stato ricevuto con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il server non consente l'esecuzione di un comando
 *   - GAME_INF_HELP_NO_MSG se non c'è alcun messaggio di aiuto disponibile.
 *   - GAME_INF_HELP_NO_NEW_MSG se il messaggio di aiuto più recente è lo stesso dell'ultimo ricevuto.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result cmdHelp(int sd)
{
    op_result ret;
    desc_msg msg;

    // Invia il comando "help" specificando il numero dell'ultimo messaggio ricevuto
    init_msg(&msg, MSG_GAME_CMD_HELP, state.current_help_msg_id);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;
    
    // Riceve il risultato dell'operazione
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Controllo del tipo di messaggio ricevuto
    switch (msg.type)
    {
        case MSG_GAME_DESCR:
            break;
        case MSG_GAME_INF_HELP_NO_MSG:
            return GAME_INF_HELP_NO_MSG;
        case MSG_GAME_INF_HELP_NO_NEW_MSG:
            return GAME_INF_HELP_NO_NEW_MSG;
        default:
            return ERR_UNEXPECTED_MSG_TYPE;
    }

    // Aggiorna il messaggio di aiuto
    strncpy(state.current_help_msg, msg.payload, MAX_HELP_DIM);
    state.current_help_msg[MAX_HELP_DIM - 1] = '\0';
    state.current_help_msg_id = state.last_help_msg_id;

    return ret;
}

/*
 * Invia al server il comando per terminare volontariamente la partita.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 * 
 * Restituisce:
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_END_QUIT se la partita è stata terminata correttamente.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result cmdEnd(int sd) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server il comando "end"
    init_msg(&msg, MSG_GAME_CMD_END);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve il risultato dell'esecuzione del comando
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Controlla il tipo di messaggio ricevuto
    switch (msg.type)
    {
        case MSG_GAME_END_QUIT: {
            // Partita terminata con successo
            strncpy(state.current_description, msg.payload, MAX_DESCR_DIM);
            state.current_description[MAX_DESCR_DIM - 1] = '\0';

            state.game_finished = FINISHED_QUIT;
            return GAME_END_QUIT;
        }
        case MSG_GAME_ERR_CMD_NOT_ALLOWED: {
            return GAME_ERR_CMD_NOT_ALLOWED;
        }
        default:
            return ERR_UNEXPECTED_MSG_TYPE;
    }
}

/*
 * Invia al server la soluzione a un enigma associata a un oggetto specifico.
 * Restituisce l'esito dell'operazione.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - obj_name: Nome dell'oggetto a cui è associato l'enigma.
 *   - solution: Soluzione proposta dall'utente all'enigma.
 *
 * Restituisce:
 *   - OK se la soluzione è stata inviata correttamente e l'oggetto è stato sbloccato.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - GAME_ERR_CMD_NOT_ALLOWED se il comando non è consentito.
 *   - GAME_ERR_NOT_FOUND se l'oggetto specificato non è stato trovato nel gioco.
 *   - GAME_INF_PUZZLE_WRONG se la risposta all'enigma è sbagliata.
 *   - GAME_INF_OBJ_NO_LOCK se l'oggetto non è bloccato oppure non è bloccato da un enigma.
 *   - GAME_END_WIN se il giocatore ha vinto e il gioco è terminato con successo.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato a causa del timeout.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result sendPuzzleSolution(int sd, const char* obj_name, const char* solution) 
{
    op_result ret;
    desc_msg msg;

    // Invia al server le informazioni
    init_msg(&msg, MSG_GAME_PUZZLE_SOL, obj_name, solution);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve la risposta
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;

    switch (msg.type)
    {
        case MSG_SUCCESS: {
            // Oggetto sbloccato con successo
            return OK;
        }
        case MSG_GAME_PUZZLE_WRONG: {
            // Risposta sbagliata
            return GAME_INF_PUZZLE_WRONG;
        }
        case MSG_GAME_INF_OBJ_NO_LOCK: {
            // L'oggetto non è bloccato oppure non è bloccato da un enigma
            return GAME_INF_OBJ_NO_LOCK;
        }
        case MSG_GAME_ERR_NOT_FOUND: {
            // Oggetto non trovato
            return GAME_ERR_NOT_FOUND;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }
}
