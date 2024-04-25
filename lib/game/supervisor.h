#ifndef GAME_SUPERVISOR
#define GAME_SUPERVISOR

#include "shared.h"

typedef struct                              // Struttura per memorizzare le informazioni della sessione di gioco osservata
{
    char username[MAX_USR_DIM];             // Username del giocatore
    char room_name[MAX_ROOM_NAME_DIM];      // Nome della stanza
    time_t remaining_time;                  // Tempo di gioco rimanente
    int help_msg_id;                        // Numero dell'ultimo messaggio di aiuto inviato al giocatore
    int dim_bag;                            // Dimensione dello zaino del giocatore
    int n_bag_objs;                         // Numero di oggetti nello zaino del giocatore
    int user_token;                         // Numero di token che il giocatore possiede
    int room_token;                         // Numero di token necessari al giocatore per vincere
}
observed_session;

const observed_session* getObservedSession();
bool selectUser(const char* username);
void freeList(char** list, int n);

op_result reqActiveUsers(int sd, char*** users_list, int* n);
op_result reqUserSessionData(int sd);
op_result reqUserSessionObjs(int sd, char*** objs_list, int* n);
op_result reqUserSessionBag(int sd, char*** objs_names, int* n);

op_result reqUserSessionAddTime(int sd, int seconds);
op_result reqUserSessionSubTime(int sd, int seconds);
op_result reqUserSessionSetTime(int sd, int seconds);

op_result reqUserSessionSetHelp(int sd, const char* help);

#endif