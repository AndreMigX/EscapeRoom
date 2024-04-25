#ifndef GAME_SHARED
#define GAME_SHARED

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>

#include "../utils.h"

#define MAX_PAYLOAD_DIM     512
#define MAX_USR_DIM         50
#define MAX_PSW_DIM         50

#define MAX_DESCR_DIM       200
#define MAX_NAME_DIM        20
#define MAX_ROOM_NAME_DIM   50
#define MAX_PUZZLE_DIM      200
#define MAX_PUZZLE_SOL_DIM  50
#define MAX_HELP_DIM        200

// Enumeratore per i risultati delle operazioni e i tipi di errori.
typedef enum op_result
{
    NET_ERR_REMOTE_SOCKET_CLOSED,       // Errore, il socket remoto è chiuso durante la comunicazione.
    NET_ERR_SEND,                       // Errore nell'invio dei dati.
    NET_ERR_RECV,                       // Errore nella ricezione dei dati.

    AUTH_ERR_NO_AUTH,                   // Errore, l'utente non è autenticato.
    AUTH_ERR_INVALID_CREDENTIALS,       // Errore, credenziali non valide durante il processo di autenticazione.
    AUTH_ERR_USERNAME_EXISTS,           // Errore, l'username specificato è già stato registrato.

    GAME_ERR_INIT,                      // Errore durante l'inizializzazione della sessione di gioco.
    GAME_ERR_INVALID_ROOM,              // Errore, il numero della stanza non è valido.
    GAME_ERR_CMD_NOT_ALLOWED,           // Errore, esecuzione del comando non consentita.
    GAME_ERR_NOT_FOUND,                 // Errore, l'oggetto o la locazione richiesti non è stato trovato.

    GAME_INF_HELP_NO_MSG,               // Notifica l'assenza di messaggio di aiuto.
    GAME_INF_HELP_NO_NEW_MSG,           // Notifica l'assenza di nuovi messaggi di aiuto.

    GAME_INF_LOCK_ACTION,               // Notifica che un oggetto è bloccato da un'azione.
    GAME_INF_LOCK_PUZZLE,               // Notifica che un oggetto è bloccato da un enigma.
    GAME_INF_OBJ_NO_LOCK,               // Notifica che un oggetto non è bloccato.
    GAME_INF_PUZZLE_WRONG,              // Notifica di risposta errata a un enigma.

    GAME_INF_OBJ_NO_USE,                // Notifica l'impossibilità di utilizzare un oggetto.
    GAME_INF_OBJ_CONSUMED,              // Notifica che un oggetto è consumato.
    GAME_INF_OBJ_TAKEN,                 // Notifica che un oggetto è già stato raccolto.
    GAME_INF_OBJ_NOT_TAKEN,             // Notifica che un oggetto non è stato raccolto.
    GAME_INF_OBJ_NO_TAKE,               // Notifica che non è possibile raccogliere un oggetto.
    GAME_INF_BAG_FULL,                  // Notifica che lo zaino è pieno.

    GAME_END_TIMEOUT,                   // Notifica di fine gioco per timeout.
    GAME_END_QUIT,                      // Notifica di fine gioco per volontà del giocatore.
    GAME_END_WIN,                       // Notifica di completamento del gioco con successo.

    SU_ERR_USER_NOT_FOUND,              // Errore, non esiste una sessione di gioco per l'utente specificato.
    SU_ERR_ALTER_TIME_OPT,              // Errore, operazione di modifica del tempo non supportata o non valida.

    ERR_UNEXPECTED_MSG_TYPE,            // Errore, tipo di messaggio inatteso.
    ERR_OTHER,                          // Altro tipo di errore non specificato.

    OK                                  // Operazione completata con successo.
} op_result;

// Enumeratore per i tipi di messaggi supportati nella comunicazione tra client e server.
typedef enum msg_type 
{
    // Richiesta di registrazione utente.
    // Payload: username (string), password (string).
    MSG_REQ_SIGNUP = 0,

    // Richiesta di accesso utente.
    // Payload: username (string), password (string).
    MSG_REQ_LOGIN,

    // Richiesta di elenco dei nomi delle stanze disponibili.
    // Nessun payload.
    MSG_REQ_ROOM_NAMES,

    // Richiesta di avvio di una partita.
    // Payload: username (string), numero della stanza (int).
    MSG_REQ_START_GAME,

    // Errore, credenziali non valide durante login o registrazione.
    // Nessun payload.
    MSG_AUTH_ERR_INVALID_CREDENTIALS,

    // Errore, l'username specificato è già stato registrato.
    // Nessun payload.
    MSG_AUTH_ERR_USERNAME_EXISTS,

    // Errore generico del server durante l'autenticazione.
    // Nessun payload.
    MSG_AUTH_ERR_SERVER,

    // Inizio di una lista di informazioni.
    // Payload: numero di elementi nella lista (int).
    MSG_LIST_START,

    // Elemento di una lista di informazioni.
    // Payload: elemento della lista (string).
    MSG_LIST_ITEM,

    // Trasporta le informazioni necessarie per l'inizializzazione della sessione di gioco.
    // Payload: tempo totale (time_t), dimensione zaino (int), token della room (int).
    MSG_GAME_INIT,

    // Trasporta una descrizione/testo.
    // Payload: descrizione/testo (string).
    MSG_GAME_DESCR,

    // Trasporta lo stato aggiornato della sessione di gioco.
    // Payload: tempo rimanente (time_t), token del giocatore (int), numero di oggetti nello zaino (int), numero dell'ultimo messaggio di aiuto (int).
    MSG_GAME_STATE,

    // Trasporta la soluzione di un enigma.
    // Payload: nome dell'oggetto da sbloccare (string), risposta (string).
    MSG_GAME_PUZZLE_SOL,

    // Notifica di risposta errata a un enigma.
    // Nessun payload.
    MSG_GAME_PUZZLE_WRONG,

    // Comando per ottenere la descrizione di una location, un oggetto o una stanza.
    // Payload: nome di una location o di un oggetto, oppure 'room' per la descrizione della stanza (string).
    MSG_GAME_CMD_LOOK,

    // Comando per ottenere la lista degli oggetti nello zaino.
    // Nessun payload.
    MSG_GAME_CMD_OBJS,

    // Comando per utilizzare uno o più oggetti.
    // Payload: nome di un oggetto (string), nome di un oggetto (string).
    MSG_GAME_CMD_USE,

    // Comando per raccogliere un oggetto.
    // Payload: nome dell'oggetto da raccogliere (string).
    MSG_GAME_CMD_TAKE,

    // Comando per togliere dallo zaino un oggetto.
    // Payload: nome dell'oggetto da togliere dallo zaino (string).
    MSG_GAME_CMD_DROP,

    // Comando per terminare la sessione.
    // Nessun payload.
    MSG_GAME_CMD_END,

    // Comando per ottenere l'ultimo messaggio di aiuto disponibile.
    // Payload: numero dell'ultimo messaggio di aiuto ricevuto (int).
    MSG_GAME_CMD_HELP,

    // Notifica l'assenza di messaggio di aiuto.
    // Nessun payload.
    MSG_GAME_INF_HELP_NO_MSG,

    // Notifica l'assenza di nuovi messaggi di aiuto.
    // Nessun payload.
    MSG_GAME_INF_HELP_NO_NEW_MSG,

    // Notifica che un oggetto è bloccato da un'azione.
    // Nessun payload.
    MSG_GAME_INF_LOCK_ACTION,

    // Notifica che un oggetto è bloccato da un enigma.
    // Payload: testo dell'enigma (string).
    MSG_GAME_INF_LOCK_PUZZLE,

    // Notifica che un oggetto non è bloccato.
    // Nessun payload.
    MSG_GAME_INF_OBJ_NO_LOCK,

    // Notifica l'impossibilità di utilizzare un oggetto.
    // Nessun payload.
    MSG_GAME_INF_OBJ_NO_USE,

    // Notifica che un oggetto è consumato.
    // Nessun payload.
    MSG_GAME_INF_OBJ_CONSUMED,

    // Notifica che un oggetto è già stato raccolto.
    // Nessun payload.
    MSG_GAME_INF_OBJ_TAKEN,

    // Notifica che un oggetto non è stato raccolto.
    // Nessun payload.
    MSG_GAME_INF_OBJ_NOT_TAKEN,

    // Notifica che non è possibile raccogliere un oggetto.
    // Nessun payload.
    MSG_GAME_INF_OBJ_NO_TAKE,

    // Notifica che lo zaino è pieno.
    // Nessun payload.
    MSG_GAME_INF_BAG_FULL,

    // Errore durante l'inizializzazione della sessione di gioco.
    // Nessun payload.
    MSG_GAME_ERR_INIT,

    // Errore, il numero della stanza non è valido.
    // Nessun payload.
    MSG_GAME_ERR_INVALID_ROOM,

    // Errore, esecuzione di un comando non consentita.
    // Nessun payload.
    MSG_GAME_ERR_CMD_NOT_ALLOWED,

    // Errore, l'oggetto o la locazione richiesti non è stato trovato
    // Nessun payload.
    MSG_GAME_ERR_NOT_FOUND,

    // Notifica di fine gioco per timeout.
    // Payload: messaggio di fine gioco per timeout (string).
    MSG_GAME_END_TIMEOUT,

    // Notifica di fine gioco per volontà del giocatore.
    // Payload: messaggio di fine gioco per volontà del giocatore (string).
    MSG_GAME_END_QUIT,
    
    // Notifica di completamento del gioco con successo.
    // Payload: messaggio di completamento del gioco (string).
    MSG_GAME_END_WIN,

    // Richiesta di ottenere la lista degli utenti attivi.
    // Nessun payload.
    MSG_SU_REQ_ACTIVE_USERS_LIST,

    // Richiesta di ottenere le informazioni sulla sessione di gioco di un utente.
    // Payload: username del giocatore (string).
    MSG_SU_REQ_USER_SESSION_DATA,

    // Richiesta di ottenere l'elenco degli oggetti presenti, inclusi i loro stati, nella sessione di gioco di un utente.
    // Payload: username del giocatore (string).
    MSG_SU_REQ_USER_SESSION_OBJS,

    // Richiesta di ottenere l'elenco degli oggetti nello zaino di un utente.
    // Payload: username del giocatore (string).
    MSG_SU_REQ_USER_SESSION_BAG,

    // Richiesta di modifica del tempo rimanente della sessione di gioco di un utente.
    // Payload: username del giocatore (string), secondi (int), '+' | '-' | '=' (add, sub, set) (char).
    MSG_SU_REQ_USER_SESSION_ALTER_TIME,

    // Richiesta di impostare un nuovo messaggio di aiuto per la sessione di gioco di un utente.
    // Payload: username del giocatore (string), messaggio di aiuto (string).
    MSG_SU_REQ_USER_SESSION_SET_HELP,

    // Trasporta le informazioni sulla sessione di gioco di un utente.
    // Payload: tempo rimanente (time_t), token della room (int), token del giocatore (int), dimensione dello zaino (int), oggetti nello zaino (int).
    MSG_SU_USER_SESSION_DATA,

    // Errore, non esiste una sessione di gioco per l'utente specificato.
    // Nessun payload.
    MSG_SU_ERR_USER_NOT_FOUND,

    // Errore, operazione di modifica del tempo non supportata o non valida.
    // Nessun payload.
    MSG_SU_ERR_ALTER_TIME_OPT,

    // Notifica il successo di un'operazione.
    // Nessun payload.
    MSG_SUCCESS
} msg_type;

typedef struct {                        // Struttura che definisce un messaggio
    msg_type type;                      // Tipo del messaggio
    char payload[MAX_PAYLOAD_DIM];      // Contenuto
} desc_msg;

void init_msg(desc_msg* msg, msg_type type, ...);
op_result send_to_socket(int sd, desc_msg* msg);
op_result receive_from_socket(int sd, desc_msg* msg);

#endif