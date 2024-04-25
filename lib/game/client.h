#ifndef GAME_CLIENT
#define GAME_CLIENT

#include "shared.h"

typedef struct                                  // Struttura che definisce lo stato della sessione di gioco
{
    char username[MAX_USR_DIM];                 // Username del giocatore
    time_t remaining_time;                      // Tempo rimanente
    int current_help_msg_id;                    // Numero dell'ultimo messaggio di aiuto letto
    int last_help_msg_id;                       // Numero dell'ultimo messaggio di aiuto inviato dal supervisore
    int bag_size;                               // Dimensione dello zaino
    int objs_in_bag;                            // Numero di oggetti nello zaino
    int room_token;                             // Numero di token necessari per vincere
    int user_token;                             // Numero di token che il giocatore possiede
    int room;                                   // Room nella quale l'utente sta giocando 
    char current_description[MAX_DESCR_DIM];    // Ultima descrizione ricevuta dal server (storia della stanza, descrizione oggetto/location/stanza o messaggio di fine gioco)
    char current_help_msg[MAX_HELP_DIM];        // Ultimo messaggio di aiuto ricevuto
    enum  {
        FINISHED_WIN,                           // Sessione terminata perché il giocatore ha ottenuto tutti i token
        FINISHED_TIMEOUT,                       // Sessione terminata perché il giocatore ha esaurito il tempo
        FINISHED_QUIT,                          // Sessione terminata per volontà del giocatore
        FINISHED_NO                             // Sessione non ancora terminata
    }
    game_finished;                              // Indica se e come la sessione è terminata
}
game_state;

op_result reqLogin(int sd, const char* username, const char* password);
op_result reqSignup(int sd, const char* username, const char* password);

op_result reqRoomNames(int sd, char*** room_names, int* n);
op_result reqStartGame(int sd, unsigned short room);

op_result cmdLook(int sd, const char* what);
op_result cmdObjs(int sd, char*** objs_names, int* n);
op_result cmdUse(int sd, const char* obj1_name, const char* obj2_name, char** out);
op_result cmdTake(int sd, const char* obj_name, char** puzzle_text);
op_result cmdDrop(int sd, const char* obj_name);
op_result cmdHelp(int sd);
op_result cmdEnd(int sd);

op_result sendPuzzleSolution(int sd, const char* obj_name, const char* solution);

void freeList(char** list, int n);
const game_state* getGameState();

#endif