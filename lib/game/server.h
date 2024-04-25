#ifndef GAME_SERVER
#define GAME_SERVER

#include "shared.h"

#define VERBOSE                                     // Attiva la modalità verbose
#define BACKLOG             10                      // Dimensione della coda di richieste di connessione
#define MAX_ROOMS           1                       // Numero di stanze disponibili

typedef enum                                        // Enumeratore che definisce i tipi di blocchi su un oggetto
{
    LOCK_OBJ,                                       // Indica se per sbloccare l'oggetto è necessario l'uso di un altro oggetto
    LOCK_PUZZLE                                     // Indica se per sbloccare l'oggetto è necessario risolvere un enigma
}
game_lock_type;

typedef enum                                        // Enumeratore che definisce i tipi di azioni
{
    ACTION_REVEAL,                                  // Azione che permette di rendere visibile un'oggetto nascosto
    ACTION_UNLOCK,                                  // Azione che permette di sbloccare un'oggetto bloccato
    ACTION_CONSUME,                                 // Azione che consuma un oggetto
    ACTION_TOKEN                                    // Azione che assegna un token al giocatore
}
game_action_type;

typedef enum                                        // Enumeratore che definisce i tipo di usi possibli
{
    USE_ALONE,                                      // L'oggetto può essere usato da solo
    USE_COMBINE                                     // L'oggetto può essere combinato con un altro oggetto
}
game_use_type;

typedef struct                                      // Struttura che definisce un'azione da eseguire su un oggetto
{
    game_action_type type;                          // Tipo di azione da eseguire
    char obj_name[MAX_NAME_DIM];                    // Nome dell'oggetto target (se richiesto)
}
game_action;

typedef struct                                      // Struttura che definisce un enigma
{
    char text[MAX_PUZZLE_DIM];                      // Contiene il testo dell'enigma
    char solution[MAX_PUZZLE_SOL_DIM];              // Contiene la risposta corretta all'enigma
}
game_puzzle;

typedef struct                                      // Struttura che definisce un lock su un oggetto
{
    game_lock_type type;                            // Specifica il tipo di blocco
    int n_actions;                                  // Numero di azioni
    game_action* actions;                           // Array di azioni da eseguire una volta sbloccato l'oggetto
    
    game_puzzle puzzle;                             // Specifica l'enigma da risolvere, significativo quando type = LOCK_PUZZLE
}
game_lock;

typedef struct                                      // Struttura che definisce un modo d'uso di un oggetto
{
    game_use_type type;                             // Specifia il tipo di uso
    char otherObj[MAX_NAME_DIM];                    // Specifica il nome dell'oggetto con il quale è possibile combinarlo, significativo solo se type = USE_COMBINE

    int n_actions;                                  // Numero di azioni
    game_action* actions;                           // Array di azioni

    char use_descr[MAX_DESCR_DIM];                  // Messaggio dopo aver usato l'oggetto
}
game_use;

typedef struct                                      // Struttura che definisce un oggetto di gioco
{
    char name[MAX_NAME_DIM];                        // Nome dell'oggetto

    bool isLocked;                                  // Specifica se l'oggetto è bloccato o meno
    game_lock lock;                                 // Indica come sbloccare l'oggetto, significativo solo se isLocked = true

    bool isHidden;                                  // Specifica se l'oggetto è visibile o meno 
    bool takeable;                                  // Indica se è possibile raccogliere l'oggetto
    bool consumable;                                // Indica che l'oggetto non può essere riottenuto dopo averlo usato, significativo solo se takeable = true

    int n_uses;                                     // Numero di usi possibili
    game_use* uses;                                 // Array di modi d'uso

    char locked_descr[MAX_DESCR_DIM];               // Descrizione dell'oggetto quando è bloccato
    char unlocked_descr[MAX_DESCR_DIM];             // Descrizione dell'oggetto quando è sbloccato
}
game_obj;

typedef struct                                      // Struttura che definisce una location della room
{
    char name[MAX_NAME_DIM];                        // Nome della location
    char descr[MAX_DESCR_DIM];                      // Descrizione della location

    int n_objs;                                     // Numero di oggetti nella location
    game_obj* objs;                                 // Oggetti nella location
}
game_location;

typedef struct                                      // Struttura che definisce una room
{
    char name[MAX_ROOM_NAME_DIM];                   // Nome della room
    char story[MAX_DESCR_DIM];                      // Storia del gioco
    char descr[MAX_DESCR_DIM];                      // Descrizione della room

    char timeout_text[MAX_DESCR_DIM];               // Messaggio da mostrare al giocatore quando scade il tempo massimo
    char quit_text[MAX_DESCR_DIM];                  // Messaggio da mostrare al giocatore quando abbandona la partita
    char win_text[MAX_DESCR_DIM];                   // Messaggio da mostrare al giocatore quando ottiene tutti i token

    int dim_bag;                                    // Dimensione dello zaino del giocatore
    int token;                                      // Numero di token necessari per vincere
    time_t seconds;                                 // Tempo in secondi che il giocatore ha a disposizione
    
    int n_locations;                                // Numero di locazioni della room
    game_location* locations;                       // Locations della room
}
game_room;

typedef struct game_session                         // Struttura che definisce una sessione di gioco
{
    int sd;                                         // Socket di comunicazione dell'utente
    char username[MAX_USR_DIM];                     // Username dell'utente
    int room;                                       // Room nella quale l'utente sta giocando

    int help_msg_id;                                // Numero dell'ultimo messaggio di aiuto inviato
    char help_msg[MAX_HELP_DIM];                    // Messaggio di aiuto inviato dal supervisore

    int n_special_objects;                          // Numero di oggetti bloccati, nascosti e consumabili all'interno della stanza
    time_t start_time;                              // Timestamp di quando l'utente ha iniziato a giocare, usato per calcolare il tempo rimanente
    time_t seconds;                                 // Tempo in secondi che il giocatore ha a disposizione
    int token;                                      // Numero di token che il giocatore possiede

    int dim_bag;                                    // Dimensione dello zaino
    int n_bag_objs;                                 // Numero di oggetti nello zaino
    game_obj** bag_objs;                            // Oggetti che il giocatore ha nello zaino

    int n_locked_objs;                              // Numero di oggetti bloccati all'interno della stanza
    game_obj** unlocked_objs;                       // Oggetti che il giocatore ha sbloccato

    int n_hidden_objs;                              // Numero di oggetti nascosti all'interno della stanza
    game_obj** revealed_objs;                       // Oggetti in origine nascosti ma che ora il giocatore può vedere

    int n_consumable_objs;                          // Numero di oggetti consumabili all'interno della stanza
    game_obj** consumed_objs;                       // Oggetti che il giocatore ha "consumato"

    struct game_session* next;
}
game_session;

bool activeUsers();

op_result authUserSuccess(int sd);
op_result authUserFailed(int sd);
op_result authUsernameExists(int sd);
void authUserDisconnected(int sd);

op_result sendRoomNames(int sd);
op_result startGame(int sd, const char* username, int room);

op_result cmdLook(int sd, const char* what);
op_result cmdObjs(int sd);
op_result cmdUse(int sd, const char* obj1_name, const char* obj2_name);
op_result cmdTake(int sd, const char* obj_name);
op_result cmdDrop(int sd, const char* obj_name);
op_result cmdHelp(int sd, int last_help_msg_id);
op_result cmdEnd(int sd);

op_result checkPuzzleSolution(int sd, const char* obj_name, const char* solution);

op_result sendActiveUsers(int sd);
op_result sendUserSessionData(int sd, const char* username);
op_result sendUserSessionObjs(int sd, const char* username);
op_result sendUserSessionBag(int sd, const char* username);
op_result alterSessionTime(int sd, const char* username, const char opt, int seconds);
op_result setUserSessionHelp(int sd, const char* username, const char* help_msg);

#endif