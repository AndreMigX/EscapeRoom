#include "supervisor.h"

static op_result receive_list(int sd, char*** list, int* n);

static op_result receiveState(int sd);
static op_result reqUserSessionAlterTime(int sd, int seconds, const char opt);

// Informazioni sessione dell'utente selezionato
static observed_session session = {
    .username = "\0",
    .room_name = "\0"
};

/*
 * Restituisce:
 *   - Puntatore costante alla struttura che contiene le informazioni sulla sessione di gioco dell'utente selezionato.
 */
const observed_session* getObservedSession() 
{
    return &session;
}

/*
 * Seleziona l'utente da osservare.
 * 
 * Parametri:
 *   - username: Il nome dell'utente.
 * 
 * Restituisce:
 *   - true se l'utente è stato selezionato con successo, false altrimenti
 */
bool selectUser(const char* username) 
{
    if (username == NULL || strcmp(username, "") == 0)
        return false;

    strncpy(session.username, username, MAX_USR_DIM);
    session.username[MAX_USR_DIM - 1] = '\0';
    return true;
}

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
 * Riceve lo stato attuale della sessione di gioco dell'utente selezionato.
 *
 * Parametri:
 *   - sd: Descrittore del socket utilizzato per la comunicazione con il server.
 *
 * Restituisce:
 *   - OK se lo stato è stato ricevuto con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_RECV in caso di errori nella ricezione del messaggio dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - SU_ERR_ALTER_TIME_OPT se l'opzione specificata per la modifica del tempo rimanente non è supportata dal server
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
        case MSG_SU_ERR_USER_NOT_FOUND: {
            return SU_ERR_USER_NOT_FOUND;
        }
        case MSG_SU_ERR_ALTER_TIME_OPT: {
            return SU_ERR_ALTER_TIME_OPT;
        }
        default: {
            return ERR_UNEXPECTED_MSG_TYPE;
        }
    }

    // Aggiorna lo stato
    sscanf(msg.payload, "%lld %d %d %d", (long long*) &session.remaining_time, &session.user_token, &session.n_bag_objs, &session.help_msg_id);
    return ret;
}

/*
 * Richiede al server la lista dei giocatori attivi.
 * Inizializza un messaggio di richiesta al server e lo invia tramite il socket specificato.
 * Successivamente, riceve la lista degli utenti in gioco dal server.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 *   - users_list: Puntatore a un array di stringhe che conterrà l'elenco dei nomi utente dei giocatori attivi ricevuti dal server.
 *   - n: Puntatore a una variabile intera che conterrà il numero totale di utenti in gioco.
 * 
 * Restituisce:
 *   - OK se la richiesta degli utenti è stata inviata con successo al server e la lista è stata ricevuta correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione della lista dal server.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 *   - ERR_OTHER se si verificano altri errori durante la ricezione o l'allocazione di memoria.
 */
op_result reqActiveUsers(int sd, char*** users_list, int* n) 
{
    op_result ret;
    desc_msg msg;

    // Chiede al server la lista degli utenti in gioco
    init_msg(&msg, MSG_SU_REQ_ACTIVE_USERS_LIST);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve la lista dei nomi utente
    return receive_list(sd, users_list, n);
}

/*
 * Richiede al server le informazioni di base sulla sessione di gioco dell'utente selezionato.
 * Queste informazioni includono il nome della stanza, il tempo rimanente, il numero di token della stanza
 * e quelli ottenuti dal giocatore, la dimensione dello zaino e il numero di oggetti in esso contenuti.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il server.
 * 
 * Restituisce:
 *   - OK se le informazioni sono state ottenute con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori nella ricezione dei messaggi dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - ERR_UNEXPECTED_MSG_TYPE se viene ricevuto un messaggio inaspettato dal server.
 */
op_result reqUserSessionData(int sd)
{
    op_result ret;
    desc_msg msg;

    // Controlla se è stato selezionato un utente
    if (strlen(session.username) == 0)
        return SU_ERR_USER_NOT_FOUND;

    // Chiede al server le informazioni di base 
    // sulla sessione di gioco dell'utente selezionato
    init_msg(&msg, MSG_SU_REQ_USER_SESSION_DATA, session.username);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve il nome della stanza
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;
    // Verifica del tipo di messaggio
    switch (msg.type)
    {
        case MSG_GAME_DESCR: {
            strncpy(session.room_name, msg.payload, MAX_ROOM_NAME_DIM);
            session.room_name[MAX_ROOM_NAME_DIM - 1] = '\0';
            break;
        }
        case MSG_SU_ERR_USER_NOT_FOUND: {
            return SU_ERR_USER_NOT_FOUND;
        }
        default:
            return ERR_UNEXPECTED_MSG_TYPE;
    }

    // Riceve il tempo rimanente, 
    // il numero di token della stanza e quelli ottenuti dal giocatore,
    // la dimensione dello zaino e il numero di oggetti in esso contenuti
    ret = receive_from_socket(sd, &msg);
    if (ret != OK)
        return ret;
    // Verifica del tipo di messaggio
    switch (msg.type)
    {
        case MSG_SU_USER_SESSION_DATA: {
            sscanf(msg.payload, "%lld %d %d %d %d", (long long*) &session.remaining_time, &session.room_token, &session.user_token, &session.dim_bag, &session.n_bag_objs);
            break;
        }
        default:
            return ERR_UNEXPECTED_MSG_TYPE;
    }

    return ret;
}

/*
 * Richiede al server lo stato di tutti gli oggetti bloccati, nascosti e consumabili
 * relativamente alla sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - objs_list: Puntatore a un array di stringhe che conterrà la lista degli oggetti.
 *   - n: Puntatore a una variabile intera che conterrà il numero totale di oggetti.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se l'utente selezionato non è stato trovato.
 *   - ERR_UNEXPECTED_MSG_TYPE se viene ricevuto un messaggio inaspettato dal server.
 *   - ERR_OTHER se si verificano altri errori durante la ricezione o l'allocazione di memoria.
 */
op_result reqUserSessionObjs(int sd, char*** objs_list, int* n) 
{
    op_result ret;
    desc_msg msg;

    // Controlla se è stato selezionato un utente
    if (strlen(session.username) == 0)
        return SU_ERR_USER_NOT_FOUND;

    // Invia al server la richiesta di ottenere lo stato degli oggetti
    init_msg(&msg, MSG_SU_REQ_USER_SESSION_OBJS, session.username);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve il tempo rimanente, il numero di token e il numero di oggetti nello zaino dell'utente selezionato
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve la lista degli oggetti dal server
    return receive_list(sd, objs_list, n);
}

/*
 * Richiede al server la lista degli oggetti nello zaino del giocatore selezionato
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - objs_names: Puntatore a un array di stringhe che conterrà la lista degli oggetti.
 *   - n: Puntatore a una variabile intera che conterrà il numero totale di oggetti.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se l'utente selezionato non è stato trovato.
 *   - ERR_UNEXPECTED_MSG_TYPE se viene ricevuto un messaggio inaspettato dal server.
 *   - ERR_OTHER se si verificano altri errori durante la ricezione o l'allocazione di memoria.
 */
op_result reqUserSessionBag(int sd, char*** objs_names, int* n) 
{
    op_result ret;
    desc_msg msg;

    // Controlla se è stato selezionato un utente
    if (strlen(session.username) == 0)
        return SU_ERR_USER_NOT_FOUND;

    // Invia al server la richiesta di ottenere gli oggetti nello zaino del giocatore
    init_msg(&msg, MSG_SU_REQ_USER_SESSION_BAG, session.username);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve il tempo rimanente, il numero di token e il numero di oggetti nello zaino dell'utente selezionato
    ret = receiveState(sd);
    if (ret != OK)
        return ret;

    // Riceve la lista degli oggetti dal server
    return receive_list(sd, objs_names, n);
}

/*
 * Richiede al server di aggiungere tempo alla sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - seconds: Il numero di secondi da aggiungere al tempo rimanente della sessione.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - SU_ERR_ALTER_TIME_OPT se l'opzione specificata per la modifica del tempo rimanente non è supportata dal server
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result reqUserSessionAddTime(int sd, int seconds) {
    return reqUserSessionAlterTime(sd, seconds, '+');
}
/*
 * Richiede al server di sottrarre tempo dalla sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - seconds: Il numero di secondi da sottrarre al tempo rimanente della sessione.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - SU_ERR_ALTER_TIME_OPT se l'opzione specificata per la modifica del tempo rimanente non è supportata dal server
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result reqUserSessionSubTime(int sd, int seconds) {
    return reqUserSessionAlterTime(sd, seconds, '-');
}
/*
 * Richiede al server di impostare il tempo della sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - seconds: Il numero di secondi da impostare come tempo rimanente della sessione.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - SU_ERR_ALTER_TIME_OPT se l'opzione specificata per la modifica del tempo rimanente non è supportata dal server
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
op_result reqUserSessionSetTime(int sd, int seconds) {
    return reqUserSessionAlterTime(sd, seconds, '=');
}

/*
 * Funzione interna utilizzata per richiedere al server di modificare il tempo della sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - seconds: Il numero di secondi da aggiungere, sottrarre o impostare come tempo rimanente della sessione.
 *   - opt: Il carattere che indica l'operazione da eseguire ('+', '-' o '=').
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - SU_ERR_ALTER_TIME_OPT se l'opzione specificata per la modifica del tempo rimanente non è supportata dal server
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 */
static op_result reqUserSessionAlterTime(int sd, int seconds, const char opt) 
{
    op_result ret;
    desc_msg msg;

    // Controlla se è stato selezionato un utente
    if (strlen(session.username) == 0)
        return SU_ERR_USER_NOT_FOUND;

    // Invia al server la richiesta al server
    init_msg(&msg, MSG_SU_REQ_USER_SESSION_ALTER_TIME, session.username, seconds, opt);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco
    return receiveState(sd);
}

/*
 * Richiede al server di impostare un nuovo messaggio di aiuto per la sessione di gioco dell'utente selezionato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il server.
 *   - help: Il nuovo messaggio di aiuto da impostare per l'utente.
 * 
 * Restituisce:
 *   - OK se la richiesta è stata completata con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il server.
 *   - NET_ERR_SEND in caso di errori nell'invio della richiesta al server.
 *   - NET_ERR_RECV in caso di errori durante la ricezione dei dati dal server.
 *   - SU_ERR_USER_NOT_FOUND se non è stata trovata una sessione legata all'utente selezionato.
 *   - ERR_UNEXPECTED_MSG_TYPE se il messaggio ricevuto non è del tipo atteso.
 *   - ERR_OTHER se il messaggio di aiuto non è stato impostato.
 */
op_result reqUserSessionSetHelp(int sd, const char* help) 
{
    op_result ret;
    desc_msg msg;
    int current_help_msg_id = session.help_msg_id;

    // Controlla se è stato selezionato un utente
    if (strlen(session.username) == 0)
        return SU_ERR_USER_NOT_FOUND;

    // Invia al server la richiesta di impostare il nuovo messaggio di aiuto per l'utente
    init_msg(&msg, MSG_SU_REQ_USER_SESSION_SET_HELP, session.username, help);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Riceve lo stato aggiornato della sessione di gioco dall'utente
    ret = receiveState(sd);
    if (ret != OK)
        return ret;
    
    // Controlla se il messaggio di aiuto è stato impostato correttamente
    if (current_help_msg_id < session.help_msg_id)
        return OK;
    
    // Messaggio di aiuto non impostato
    return ERR_OTHER;
}
