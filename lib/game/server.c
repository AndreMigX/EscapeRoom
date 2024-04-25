#include "server.h"

static game_session* create_session(int sd, const char* username, int room);
static bool start_session(int sd, const char* username, int room);
static void stop_session(int sd);
static time_t get_remaining_time(game_session* session);
static game_session* find_session_by_sd(int sd);
static game_session* find_session_by_username(const char* username);

static bool is_obj_consumed(game_session* session, game_obj* obj);
static bool is_obj_taken(game_session* session, game_obj* obj);
static bool is_obj_locked(game_session* session, game_obj* obj);
static bool is_obj_hidden(game_session* session, game_obj* obj);
static bool consume_obj(game_session* session, const char* obj_name);
static bool take_obj(game_session* session, game_obj* obj);
static bool drop_obj(game_session* session, game_obj* obj);
static void unlock_obj(game_session* session, const char* obj_name);
static void reveal_obj(game_session* session, const char* obj_name);
static void compute_action(game_session* session, game_action* action);

static game_obj* find_obj(game_session* session, const char* name, bool checkVisibility);

static op_result sendUserSessionState(int sd, game_session* session);
static op_result sendGameState(game_session* session);
static op_result cmdUseAlone(game_session* session, game_obj* obj);
static op_result cmdUseCombine(game_session* session, game_obj* obj1, game_obj* obj2);

static game_room rooms[MAX_ROOMS] = 
{
    {
        .name = "La stanza dei tre manufatti\0",
        .story = "In una stanza piena di segreti, devi trovare tre manufatti per uscire.\nNiente di epico, trova i manufatti, mettili nei posti giusti, e via.\0",
        .descr = "Ti trovi in una stanza dalle pareti bianche, illuminate da una luce soffusa.\nLa stanza è divisa in quattro piccole locazioni:\n++studio++, ++camera++ da letto, ++bagno++ e il ++santuario++\0",
        .timeout_text = "Il tempo è scaduto, resterai bloccato qui per sempre!\0",
        .quit_text = "Addio, avventuriero! Speriamo di rivederti presto\0",
        .win_text = "Complimenti, sei riuscito a scappare!\0",
        .dim_bag = 2,
        .token = 3,
        .seconds = 3600,
        .n_locations = 10,
        .locations = (game_location[])
        {
            {
                .name = "studio\0",
                .descr = "Un angolo con una ++scrivania++ disordinata\0",
                .n_objs = 0
            },
            {
                .name = "scrivania\0",
                .descr = "Sulla scrivania sul piano superiore riposa un **computer**.\nAl di sotto, si intravede un **interruttore** ed un **cassetto**\0",
                .n_objs = 5,
                .objs = (game_obj[])
                {
                    {
                        .name = "computer\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 0
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Un vecchio computer, peccato che sia senza corrente.\0",
                        .unlocked_descr = "Sullo schermo lampeggia un codice: 4242\0"
                    },
                    {
                        .name = "interruttore\0",
                        .isLocked = false,
                        .isHidden = false,
                        .takeable = false,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[])
                        {
                            {
                                .type = USE_ALONE,
                                .n_actions = 2,
                                .actions = (game_action[])
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "computer\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "interruttore\0"
                                    }
                                },
                                .use_descr = "Ho sentito un BIP provenire dal computer\0",
                            }
                        },
                        .unlocked_descr = "Grosso pulsante rosso\0"
                    },
                    {
                        .name = "cassetto\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_REVEAL,
                                    .obj_name = "maschera\0"
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Il cassetto è bloccato da un **lucchetto**\0",
                        .unlocked_descr = "Al suo interno un'antica **maschera**\0"
                    },
                    {
                        .name = "maschera\0",
                        .isLocked = false,
                        .isHidden = true,
                        .takeable = true,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[]) 
                        {
                            {
                                .type = USE_COMBINE,
                                .otherObj = "manichino\0",
                                .n_actions = 2,
                                .actions = (game_action[]) 
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "manichino\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "maschera\0"
                                    }
                                },
                                .use_descr = "Gli occhi della maschera si sono illuminati!\0"
                            }
                        },
                        .unlocked_descr = "Una maschera antica, dalle fattezze enigmatiche e dettagli intricati,\nevoca un senso di mistero\0"
                    },
                    {
                        .name = "lucchetto\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 0
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[])
                        {
                            {
                                .type = USE_ALONE,
                                .n_actions = 2,
                                .actions = (game_action[])
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "cassetto\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "lucchetto\0"
                                    }
                                },
                                .use_descr = "Il cassetto si è aperto\0",
                            }
                        },
                        .locked_descr = "Piccolo lucchetto con una serratura\0",
                        .unlocked_descr = "Piccolo lucchetto con una serratura, ormai aperto\0"
                    }
                }
            },
            {
                .name = "camera\0",
                .descr = "La camera da letto è semplice e accogliente.\nUn ++letto++ disfatto al centro della stanza, un ++armadio++ semiaperto\ne una ++finestra++ con tende leggermente svolazzanti.",
                .n_objs = 0
            },
            {
                .name = "finestra\0",
                .descr = "Sul davanzale, una piccola **chiave**\0",
                .n_objs = 1,
                .objs = (game_obj[])
                {
                    {
                        .name = "chiave\0",
                        .isLocked = false,
                        .isHidden = false,
                        .takeable = true,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[]) 
                        {
                            {
                                .type = USE_COMBINE,
                                .otherObj = "lucchetto\0",
                                .n_actions = 2,
                                .actions = (game_action[]) 
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "lucchetto\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "chiave\0"
                                    }
                                },
                                .use_descr = "Lucchetto sbloccato!\0"
                            }
                        },
                        .unlocked_descr = "Piccola chiave in ottone\0"
                    }
                }
            },
            {
                .name = "armadio\0",
                .descr = "Abiti appesi e oggetti sparsi sui ripiani. Una **cassa** si nasconde tra gli indumenti\0",
                .n_objs = 2,
                .objs = (game_obj[]) 
                {
                    {
                        .name = "cassa\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_PUZZLE,
                            .puzzle = 
                            {
                                .text = "Mmmh... è bloccata da un codice. Chissà quale sarà?\0",
                                .solution = "4242\0"
                            },
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_REVEAL,
                                    .obj_name = "vaso\0"
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Scatola di legno robusta e segnata dal tempo\0",
                        .unlocked_descr = "Scatola di legno robusta e segnata dal tempo, al suo interno un antico **vaso**\0"
                    },
                    {
                        .name = "vaso\0",
                        .isLocked = false,
                        .isHidden = true,
                        .takeable = true,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[])
                        {
                            {
                                .type = USE_COMBINE,
                                .otherObj = "teca\0",
                                .n_actions = 2,
                                .actions = (game_action[]) 
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "teca\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "vaso\0"
                                    }
                                },
                                .use_descr = "La teca si è illuminata!\0"
                            }
                        },
                        .unlocked_descr = "Un vaso antico a base circolare, con forme eleganti e superficie screpolata\0"
                    }
                }
            },
            {
                .name = "letto\0",
                .descr = "Tra le lenzuola si intravede una **tessera** magnetica\0",
                .n_objs = 1,
                .objs = (game_obj[]) 
                {
                    {
                        .name = "tessera\0",
                        .isLocked = false,
                        .isHidden = false,
                        .takeable = true,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[]) 
                        {
                            {
                                .type = USE_COMBINE,
                                .otherObj = "fessura\0",
                                .n_actions = 2,
                                .actions = (game_action[]) 
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "pareti\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "tessera\0"
                                    }
                                },
                                .use_descr = "Hmm... un rumore sembra provenire dalla doccia\0"
                            }
                        },
                        .unlocked_descr = "Ha una superficie molto riflettente\0"
                    }
                }
            },
            {
                .name = "bagno\0",
                .descr = "L'atmosfera è semplice e pulita, con una sensazione di tranquillità che pervade l'ambiente.\nUn grosso ++specchio++ domina uno dei muri, mentre una ++doccia++ si erge accanto al lavandino\0",
                .n_objs = 0
            },
            {
                .name = "specchio\0",
                .descr = "Un grosso specchio, incorniciato da un'elegante cornice dorata,\nsul lato c'è una piccola **fessura**\0",
                .n_objs = 1,
                .objs = (game_obj[]) 
                {
                    {
                        .name = "fessura\0",
                        .isLocked = false,
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .unlocked_descr = "Una sottile fessura, appena visibile sul lato della cornice dorata dello specchio.\nSembra appositamente progettata per accogliere qualcosa di piccolo e sottile\0"
                    }
                }
            },
            {
                .name = "doccia\0",
                .descr = "La doccia è una cabina con **pareti** bianche e un **rubinetto** metallico\0",
                .n_objs = 3,
                .objs = (game_obj[])
                {
                    {
                        .name = "pareti\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_REVEAL,
                                    .obj_name = "statuetta\0"
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Le pareti bianche, rivestite con mattonelle lucide,\nriflettono la luce e creano un ambiente luminoso e pulito\0",
                        .unlocked_descr = "Hmm... una mattonella si è staccata dal muro, lasciando un buco vuoto nel rivestimento.\nSi intravede una **statuetta**\0"
                    },
                    {
                        .name = "statuetta\0",
                        .isLocked = false,
                        .isHidden = true,
                        .takeable = true,
                        .consumable = true,
                        .n_uses = 1,
                        .uses = (game_use[]) 
                        {
                            {
                                .type = USE_COMBINE,
                                .otherObj = "piedistallo\0",
                                .n_actions = 2,
                                .actions = (game_action[]) 
                                {
                                    {
                                        .type = ACTION_UNLOCK,
                                        .obj_name = "piedistallo\0"
                                    },
                                    {
                                        .type = ACTION_CONSUME,
                                        .obj_name = "statuetta\0"
                                    }
                                },
                                .use_descr = "Il piedistallo si è appena illuminato!\0"
                            }
                        },
                        .unlocked_descr = "Un'antica statuetta a base quadrata, scolpita con precisione, emana un fascino misterioso.\nLe sue forme levigate e l'espressione del viso incutono un senso di meraviglia e curiosità\0"
                    },
                    {
                        .name = "rubinetto\0",
                        .isLocked = false,
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .unlocked_descr = "Di metallo lucido, con una semplice manopola per regolare l'acqua,\nanche se sembra non sia di grande utilità.\0"
                    }
                }
            },
            {
                .name = "santuario\0",
                .descr = "Nel santuario, c'è una **teca** di vetro vuota e un **piedistallo**.\nAccanto, un **manichino** senza testa",
                .n_objs = 3,
                .objs = (game_obj[]) 
                {
                    {
                        .name = "teca\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_TOKEN
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Scatola di vetro trasparente con un alloggio circolare\0",
                        .unlocked_descr = "Scatola di vetro trasparente con un alloggio circolare\0"
                    },
                    {
                        .name = "piedistallo\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_TOKEN
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Base solida e stabile, con una superficie piana e un alloggio a base quadrata al centro\0",
                        .unlocked_descr = "Base solida e stabile, con una superficie piana e un alloggio a base quadrata al centro\0"
                    },
                    {
                        .name = "manichino\0",
                        .isLocked = true,
                        .lock = 
                        {
                            .type = LOCK_OBJ,
                            .n_actions = 1,
                            .actions = (game_action[]) 
                            {
                                {
                                    .type = ACTION_TOKEN
                                }
                            }
                        },
                        .isHidden = false,
                        .takeable = false,
                        .consumable = false,
                        .n_uses = 0,
                        .locked_descr = "Figura umana senza testa, con le braccia e le gambe estese\0",
                        .unlocked_descr = "Il manichino, con la maschera in testa, assume un'aria ancora più enigmatica.\nGli occhi illuminati della maschera, emanano una luce intensa\0"
                    }
                }
            }
        }
    }
};

//---Sessions Management---//

// Lista delle sessioni di gioco
static game_session* sessions_list = NULL;
static int n_sessions = 0;

/*
 * Crea una nuova sessione di gioco e restituisce il puntatore ad essa.
 * 
 * Parametri:
 *   - sd: Descrittore del socket associato all'utente.
 *   - username: nome dell'utente.
 *   - room: Numero della stanza in cui l'utente vuole giocare.
 * 
 * Restituisce:
 *   - Puntatore alla nuova sessione creata, o NULL in caso di errore nell'allocazione di memoria.
 */
static game_session* create_session(int sd, const char* username, int room) 
{
    int i, n_locked_objs = 0, n_hidden_objs = 0, n_consumable_objs = 0;
    game_session* session = (game_session*)malloc(sizeof(game_session));
    if (session == NULL) {
        return NULL;
    }
    
    // Alloca memoria per lo zaino del giocatore
    session->bag_objs = malloc(sizeof(game_room*) * rooms[room].dim_bag);
    if (session->bag_objs == NULL) {
        free(session);
        return NULL;
    }
    memset(session->bag_objs, 0, sizeof(game_room*) * rooms[room].dim_bag);

    // Conta il numero di oggetti bloccati, nascosti e consumabili all'interno della stanza
    session->n_special_objects = 0;
    for (i = 0; i < rooms[room].n_locations; i++) 
    {
        int j;
        for (j = 0; j < rooms[room].locations[i].n_objs; j++) 
        {
            if (rooms[room].locations[i].objs[j].isLocked ||
                rooms[room].locations[i].objs[j].isHidden ||
                rooms[room].locations[i].objs[j].consumable)
                    session->n_special_objects++;
            
            if (rooms[room].locations[i].objs[j].isLocked)
                n_locked_objs++;
            if (rooms[room].locations[i].objs[j].isHidden)
                n_hidden_objs++;
            if (rooms[room].locations[i].objs[j].consumable)
                n_consumable_objs++;
        }
    }
    session->n_locked_objs = n_locked_objs;
    session->n_hidden_objs = n_hidden_objs;
    session->n_consumable_objs = n_consumable_objs;

    // Alloca memoria per gli oggetti sbloccabili
    session->unlocked_objs = malloc(sizeof(game_obj*) * n_locked_objs);
    if (session->unlocked_objs == NULL) {
        free(session->bag_objs);
        free(session);
        return NULL;
    }
    memset(session->unlocked_objs, 0, sizeof(game_obj*) * n_locked_objs);

    // Alloca memoria per gli oggetti nascosti
    session->revealed_objs = malloc(sizeof(game_obj*) * n_hidden_objs);
    if (session->revealed_objs == NULL) {
        free(session->bag_objs);
        free(session->unlocked_objs);
        free(session);
        return NULL;
    }
    memset(session->revealed_objs, 0, sizeof(game_obj*) * n_hidden_objs);

    // Alloca memoria per gli oggetti consumabili
    session->consumed_objs = malloc(sizeof(game_obj*) * n_consumable_objs);
    if (session->consumed_objs == NULL) {
        free(session->bag_objs);
        free(session->unlocked_objs);
        free(session->revealed_objs);
        free(session);
        return NULL;
    }
    memset(session->consumed_objs, 0, sizeof(game_obj*) * n_consumable_objs);

    // Inizializza gli altri campi
    strncpy(session->username, username, MAX_USR_DIM);
    session->username[MAX_USR_DIM - 1] = '\0';
    
    session->sd = sd;
    session->room = room;
    session->help_msg_id = 0;
    memset(session->help_msg, '\0', 1);
    session->seconds = rooms[room].seconds;
    session->start_time = getTimestamp();
    session->token = 0;
    session->dim_bag = rooms[room].dim_bag;
    session->n_bag_objs = 0;
    session->next = NULL;

    return session;
}

/*
 * Avvia una nuova sessione di gioco per l'utente.
 * 
 * Parametri:
 *   - sd: Descrittore del socket associato all'utente.
 *   - username: nome dell'utente.
 *   - room: Numero della stanza in cui si trova l'utente.
 * 
 * Restituisce:
 *   - true se la sessione è stata avviata con successo, altrimenti false.
 */
static bool start_session(int sd, const char* username, int room) 
{
    game_session* session = create_session(sd, username, room);
    if (session == NULL)
        return false;

    session->next = sessions_list;
    sessions_list = session;
    n_sessions++;
    return true;
}

/*
 * Termina una sessione di gioco.
 * 
 * Parametri:
 *   - sd: Descrittore del socket associato alla sessione di gioco da terminare.
 */
static void stop_session(int sd) 
{
    game_session* current = sessions_list;
    game_session* previous = NULL;

    // Cerca la sessione
    while (current != NULL && current->sd != sd) 
    {
        previous = current;
        current = current->next;
    }

    // Se la sessione esiste, la elimina dalla lista e dealloca la memoria
    if (current != NULL) 
    {
        if (previous == NULL)
            sessions_list = current->next;
        else
            previous->next = current->next;
        
        free(current->bag_objs);
        free(current->unlocked_objs);
        free(current->revealed_objs);
        free(current->consumed_objs);
        free(current);

        n_sessions--;
    }
}

/*
 * Restituisce il riferimento alla sessione associata al descrittore del socket specificato.
 * 
 * Parametri:
 *   - sd: Descrittore del socket dell'utente.
 * 
 * Restituisce:
 *   - Puntatore alla sessione, o NULL se non trovata.
 */
static game_session* find_session_by_sd(int sd) 
{
    game_session* session = sessions_list;

    // Cerca la sessione nella lista
    while (session != NULL && session->sd != sd) {
        session = session->next;
    }

    // Restituisce il riferimento alla sessione se trovata, altrimenti NULL
    return session;
}

/*
 * Restituisce il riferimento alla sessione associata al nome utente specificato.
 * 
 * Parametri:
 *   - username: Nome utente del giocatore.
 * 
 * Restituisce:
 *   - Puntatore alla sessione, o NULL se non trovata.
 */
static game_session* find_session_by_username(const char* username) 
{
    game_session* session = sessions_list;

    // Cerca la sessione nella lista
    while (session != NULL && strcmp(session->username, username) != 0) {
        session = session->next;
    }

    // Restituisce il riferimento alla sessione se trovata, altrimenti NULL
    return session;
}

/*
 * Verifica se ci sono utenti attivi.
 * 
 * Restituisce:
 *   - true se ci sono utenti attivi, false altrimenti.
 */
bool activeUsers()
{
    return n_sessions != 0;
}

/*
 * Invia la lista degli utenti in gioco al client.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 * 
 * Restituisce:
 *   - OK se l'invio della lista è avvenuto con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio dei messaggi al client.
 */
op_result sendActiveUsers(int sd) 
{
    desc_msg msg;
    op_result ret;
    game_session* current = sessions_list;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ricevere la lista degli utenti in gioco\n", sd);
    #endif

    // Invia il numero totale di utenti in gioco
    init_msg(&msg, MSG_LIST_START, n_sessions);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invia l'username di ogni utente
    while (current != NULL)
    {
        init_msg(&msg, MSG_LIST_ITEM, current->username);

        // Invia il messaggio
        ret = send_to_socket(sd, &msg);
        if (ret != OK)
            return ret;

        current = current->next;
    }
    return ret;
}

/*
 * Invia lo stato della sessione di gioco specificata al client, includendo il tempo rimanente, il numero di token,
 * il numero di oggetti nello zaino e il numero dell'ultimo messaggio di aiuto.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - session: Il puntatore alla sessione di gioco.
 * 
 * Restituisce:
 *   - OK se lo stato del giocatore è stato inviato con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
static op_result sendUserSessionState(int sd, game_session* session) 
{
    desc_msg msg;

    #ifdef VERBOSE
        printf("↳ Invio dello stato della sessione di %s al socket %d\n", session->username, sd);
    #endif

    // Inizializza il messaggio con le informazioni sullo stato del giocatore
    init_msg(&msg, MSG_GAME_STATE, get_remaining_time(session), session->token, session->n_bag_objs, session->help_msg_id);
    
    // Invia il messaggio al client
    return send_to_socket(sd, &msg);
}

/*
 * Invia al client le informazioni di base sulla sessione di gioco dell'utente specificato.
 * Include il nome della stanza associata all'utente e le informazioni sulla sessione corrente, come il tempo rimanente,
 * il numero di token della stanza e quelli ottenuti dall'utente, la dimensione dello zaino e il numero di oggetti in esso contenuti.
 * Se l'utente specificato non ha una sessione di gioco attiva, viene inviato un messaggio di errore.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - username: Il nome utente del giocatore di cui si vogliono ottenere le informazioni sulla sessione di gioco.
 * 
 * Restituisce:
 *   - OK se le informazioni sono state inviate con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio dei messaggi al client.
 */
op_result sendUserSessionData(int sd, const char* username) 
{
    desc_msg msg;
    op_result ret;
    game_session* session;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ricevere informazioni sulla sessione di %s\n", sd, username);
    #endif

    // Cerca la sessione in base al nome utente specificato
    session = find_session_by_username(username);
    if (session == NULL)
    {
        // Sessione non trovata
        #ifdef VERBOSE
            printf("↳ Non c'è nessuna sessione relativa ad un giocatore con username specificato\n");
        #endif

        // Invia messaggio di errore
        init_msg(&msg, MSG_SU_ERR_USER_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    // Sessione trovata
    // Invio del nome della stanza
    init_msg(&msg, MSG_GAME_DESCR, rooms[session->room].name);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;
    
    // Invio delle informazioni di base
    init_msg(&msg, MSG_SU_USER_SESSION_DATA, get_remaining_time(session), rooms[session->room].token, session->token, session->dim_bag, session->n_bag_objs);
    return send_to_socket(sd, &msg);
}

/*
 * Invia al client lo stato aggiornato della sessione di gioco dell'utente specificato
 * e quello degli oggetti bloccati, nascosti o consumabili.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - username: Il nome utente del giocatore di cui si vuole ottenere lo stato degli oggetti.
 * 
 * Restituisce:
 *   - OK se lo stato degli oggetti è stato inviato con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result sendUserSessionObjs(int sd, const char* username) 
{
    desc_msg msg;
    op_result ret;
    game_session* session;
    int i;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ricevere lo stato degli oggetti nella sessione di %s\n", sd, username);
    #endif

    // Cerca la sessione in base al nome utente specificato
    session = find_session_by_username(username);
    if (session == NULL)
    {
        // Sessione non trovata
        #ifdef VERBOSE
            printf("↳ Non c'è nessuna sessione relativa ad un giocatore con username specificato\n");
        #endif

        // Invia messaggio di errore
        init_msg(&msg, MSG_SU_ERR_USER_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    // Sessione trovata
    // Invio dello stato aggiornato
    ret = sendUserSessionState(sd, session);
    if (ret != OK)
        return ret;
    
    // Invia il numero totale di elementi nella lista
    init_msg(&msg, MSG_LIST_START, session->n_special_objects);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invio dello stato degli oggetti bloccati, nascosti o consumabili
    init_msg(&msg, MSG_LIST_ITEM, "");
    for (i = 0; i < rooms[session->room].n_locations; i++) 
    {
        int j;
        for (j = 0; j < rooms[session->room].locations[i].n_objs; j++) 
        {
            // Verifica se l'oggetto è bloccato, nascosto o consumabile
            if (rooms[session->room].locations[i].objs[j].isLocked ||
                rooms[session->room].locations[i].objs[j].isHidden ||
                rooms[session->room].locations[i].objs[j].consumable) 
            {
                // Costruisce il messaggio con lo stato dell'oggetto
                sprintf(msg.payload, "%s %d %d %d", 
                    rooms[session->room].locations[i].objs[j].name,
                    (rooms[session->room].locations[i].objs[j].isLocked && is_obj_locked(session, &rooms[session->room].locations[i].objs[j])) ? 1 : 0,
                    (rooms[session->room].locations[i].objs[j].isHidden && is_obj_hidden(session, &rooms[session->room].locations[i].objs[j])) ? 1 : 0,
                    (rooms[session->room].locations[i].objs[j].consumable && is_obj_consumed(session, &rooms[session->room].locations[i].objs[j])) ? 1 : 0
                );

                // Invia il messaggio
                ret = send_to_socket(sd, &msg);
                if (ret != OK)
                    return ret;
            }
        }
    }
    return ret;
}

/*
 * Invia al client lo stato aggiornato della sessione di gioco dell'utente specificato
 * e la lista degli oggetti nello zaino del giocatore.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il client.
 *   - username: Il nome utente del giocatore di cui si vogliono conoscere gli oggetti nello zaino.
 * 
 * Restituisce:
 *   - OK se la lista degli oggetti è stata inviata con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result sendUserSessionBag(int sd, const char* username) 
{
    desc_msg msg;
    op_result ret;
    game_session* session;
    int i;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ricevere gli oggetti nello zaino di %s\n", sd, username);
    #endif

    // Cerca la sessione in base al nome utente specificato
    session = find_session_by_username(username);
    if (session == NULL)
    {
        // Sessione non trovata
        #ifdef VERBOSE
            printf("↳ Non c'è nessuna sessione relativa ad un giocatore con username specificato\n");
        #endif

        // Invia messaggio di errore
        init_msg(&msg, MSG_SU_ERR_USER_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    // Sessione trovata
    // Invio dello stato aggiornato
    ret = sendUserSessionState(sd, session);
    if (ret != OK)
        return ret;
    
    // Invio del numero totale di oggetti nello zaino
    init_msg(&msg, MSG_LIST_START, session->n_bag_objs);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invia i nomi degli oggetti nello zaino
    for (i = 0; i < session->n_bag_objs; i++) 
    {
        if (session->bag_objs[i] == NULL)
            continue;
        
        // Invia il nome dell'oggetto
        init_msg(&msg, MSG_LIST_ITEM, session->bag_objs[i]->name);
        ret = send_to_socket(sd, &msg);
        if (ret != OK)
            return ret;
    }
    return ret;
}

/*
 * Gestisce la modifica del tempo rimanente della sessione di gioco dell'utente specificato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il client.
 *   - username: Il nome dell'utente di cui modificare il tempo della sessione.
 *   - opt: Il carattere che indica l'operazione da eseguire ('+', '-' o '=').
 *   - seconds: Il numero di secondi da aggiungere, sottrarre o impostare come tempo rimanente della sessione.
 *
 * Restituisce:
 *   - OK se la lista degli oggetti è stata inviata con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result alterSessionTime(int sd, const char* username, const char opt, int seconds) 
{
    desc_msg msg;
    game_session* session;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di alterare il tempo rimanente della sessione di %s\n", sd, username);
    #endif

    // Cerca la sessione in base al nome utente specificato
    session = find_session_by_username(username);
    if (session == NULL)
    {
        // Sessione non trovata
        #ifdef VERBOSE
            printf("↳ Non c'è nessuna sessione relativa ad un giocatore con username specificato\n");
        #endif

        // Invia messaggio di errore
        init_msg(&msg, MSG_SU_ERR_USER_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    // Sessione trovata
    // Modifica del tempo rimanente
    switch (opt)
    {
        case '+': 
        {
            session->seconds += seconds;
            #ifdef VERBOSE
                printf("↳ Aggiunti %d secondi. Nuovo tempo rimanente: %lld\n", seconds, (long long)get_remaining_time(session));
            #endif
            break;
        }
        case '-': 
        {
            session->seconds -= seconds;
            #ifdef VERBOSE
                printf("↳ Sottratti %d secondi. Nuovo tempo rimanente: %lld\n", seconds, (long long)get_remaining_time(session));
            #endif
            break;
        }
        case '=': 
        {
            session->seconds = seconds + (getTimestamp() - session->start_time);
            #ifdef VERBOSE
                printf("↳ Tempo rimanente impostato a: %lld\n", (long long)get_remaining_time(session));
            #endif
            break;
        }
        default: 
        {
            #ifdef VERBOSE
                printf("↳ Operazione non supportata\n");
            #endif
            // Invia messaggio di errore
            init_msg(&msg, MSG_SU_ERR_ALTER_TIME_OPT);
            return send_to_socket(sd, &msg);
        }
    }

    // Invio dello stato aggiornato
    return sendUserSessionState(sd, session);
}

/*
 * Imposta il messaggio di aiuto per la sessione di gioco dell'utente specificato.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il client.
 *   - username: Il nome dell'utente al quale inviare il messaggio di aiuto.
 *   - help_msg: Il nuovo messaggio di aiuto da impostare.
 * 
 * Restituisce:
 *   - OK se il messaggio di aiuto è stato impostato con successo e lo stato della sessione è stato aggiornato.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result setUserSessionHelp(int sd, const char* username, const char* help_msg) 
{
    desc_msg msg;
    game_session* session;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di impostare il messaggio di aiuto \"%s\" nella sessione di %s\n", sd, help_msg, username);
    #endif

    // Cerca la sessione in base al nome utente specificato
    session = find_session_by_username(username);
    if (session == NULL)
    {
        // Sessione non trovata
        #ifdef VERBOSE
            printf("↳ Non c'è nessuna sessione relativa ad un giocatore con username specificato\n");
        #endif

        // Invia messaggio di errore
        init_msg(&msg, MSG_SU_ERR_USER_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    // Sessione trovata
    // Imposta il messaggio di aiuto
    strncpy(session->help_msg, help_msg, MAX_HELP_DIM);
    session->help_msg[MAX_HELP_DIM - 1] = '\0';
    session->help_msg_id++;

    // Invio dello stato aggiornato
    return sendUserSessionState(sd, session);
}

//-------------------------//

/*
 * Calcola il tempo rimanente per la sessione di gioco specificata.
 * 
 * Parametri:
 *   - session: Puntatore alla sessione di gioco per la quale si vuole conoscere il tempo rimanente.
 * 
 * Restituisce:
 *   - Il tempo rimanente della sessione di gioco, espresso in secondi.
 */
static time_t get_remaining_time(game_session* session) 
{
    time_t remaining_time;
    if (session == NULL)
        return 0;
    
    remaining_time = session->seconds - (getTimestamp() - session->start_time);
    return remaining_time < 0 ? 0 : remaining_time;
}

/*
 * Verifica se l'oggetto è stato consumato dal giocatore
 * 
 * Restituisce:
 *   - true se l'oggetto è consumato, false altrimenti
 */
static bool is_obj_consumed(game_session* session, game_obj* obj) 
{
    if (obj->consumable == false)
        return false;

    int i = 0;
    for (; i < session->n_consumable_objs; i++) 
    {
        if (session->consumed_objs[i] == obj)
            return true;
    }
    return false;
}

/*
 * Verifica se l'oggetto è nello zaino del giocatore
 * 
 * Restituisce:
 *   - true se l'oggetto è nello zaino, false altrimenti
 */
static bool is_obj_taken(game_session* session, game_obj* obj) 
{
    if (obj->takeable == false)
        return false;

    int i = 0;
    for (; i < session->dim_bag; i++) 
    {
        if (session->bag_objs[i] == obj)
            return true;
    }
    return false;
}

/*
 * Verifica se il giocatore ha sbloccato o meno un oggetto bloccato
 * 
 * Restituisce:
 *   - true se l'oggetto è bloccato, false altrimenti
 */
static bool is_obj_locked(game_session* session, game_obj* obj) 
{
    if (obj->isLocked == false)
        return false;

    int i = 0;
    for (; i < session->n_locked_objs; i++) 
    {
        if (session->unlocked_objs[i] == obj)
            return false;
    }
    return true;
}

/*
 * Verifica se il giocatore può vedere o meno un oggetto nascosto
 * 
 * Restituisce:
 *   - true se l'oggetto è visibile, false altrimenti
 */
static bool is_obj_hidden(game_session* session, game_obj* obj) 
{
    if (obj->isHidden == false)
        return false;

    int i = 0;
    for (; i < session->n_hidden_objs; i++) 
    {
        if (session->revealed_objs[i] == obj)
            return false;
    }
    return true;
}

/*
 * Trova un oggetto nella stanza corrente del giocatore.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - name: Il nome dell'oggetto da cercare.
 *   - checkVisibility: Se true, controlla se l'oggetto è visibile al giocatore.
 *
 * Restituisce:
 *   - Il puntatore all'oggetto se esiste e, se richiesto, visibile al giocatore.
 *   - NULL se l'oggetto non esiste o, se richiesto, non è visibile al giocatore.
 *
 * La funzione ricerca l'oggetto nella stanza corrente del giocatore. Se l'oggetto esiste e, se richiesto,
 * è visibile al giocatore, restituisce un puntatore all'oggetto. Se l'oggetto è invisibile al giocatore
 * e il controllo sulla visibilità è attivato, la funzione restituisce NULL.
 */
static game_obj* find_obj(game_session* session, const char* name, bool checkVisibility) 
{
    int i, j;
    if (session == NULL || name == NULL || strlen(name) == 0)
        return NULL;

    for (i = 0; i < rooms[session->room].n_locations; i++) 
    {
        for (j = 0; j < rooms[session->room].locations[i].n_objs; j++) 
        {
            if (strcmp(rooms[session->room].locations[i].objs[j].name, name) == 0) 
            {
                // Oggetto trovato
                if (checkVisibility && is_obj_hidden(session, &rooms[session->room].locations[i].objs[j]))
                    // Se l'oggetto è invisibile al giocatore, è come se non esistesse
                    return NULL;

                return &rooms[session->room].locations[i].objs[j];
            }
        }
    }
    return NULL;
}

/*
 * Consuma un oggetto.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj_name: Il nome dell'oggetto da consumare.
 *
 * Restituisce:
 *   - true se l'oggetto è stato consumato con successo.
 *   - false se l'oggetto non è stato trovato, non è consumabile o è già stato consumato.
 *
 * La funzione cerca l'oggetto nel contesto del giocatore e verifica se può essere consumato.
 * Se l'oggetto è consumabile e non è già stato consumato, lo segna come consumato.
 */
static bool consume_obj(game_session* session, const char* obj_name) 
{
    int i;
    game_obj* obj = find_obj(session, obj_name, true);
    if (obj == NULL)
        return false;

    // Controlla se l'oggetto è consumabile
    if (obj->consumable == false)
        return false;

    // Controlla se l'oggetto è consumato
    if (is_obj_consumed(session, obj))
        // L'oggetto è già stato consumato
        return false;

    // Elimina l'oggetto dallo zaino del giocatore, se al suo interno
    drop_obj(session, obj);

    // Inserimento dell'oggetto nel vettore degli oggetti consumati dal giocatore
    for (i = 0; i < session->n_consumable_objs; i++) 
    {
        // Trovo la prima posizione disponibile
        if (session->consumed_objs[i] == NULL)
        {
            session->consumed_objs[i] = obj;
            return true;
        }
    }

    return false;
}

/*
 * Aggiunge un oggetto allo zaino del giocatore.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj: Il puntatore all'oggetto da aggiungere allo zaino.
 *
 * Restituisce:
 *   - true se l'oggetto è stato aggiunto con successo allo zaino.
 *   - false se l'oggetto è bloccato, cosumato oppure lo zaino è pieno.
 *
 * La funzione controlla se l'oggetto è bloccato o consumato; se lo è, restituisce false.
 * Successivamente, controlla se c'è spazio nello zaino del giocatore; se non c'è spazio, restituisce false.
 * Se l'oggetto può essere aggiunto allo zaino, la funzione lo inserisce nel primo slot libero e restituisce true.
 */
static bool take_obj(game_session* session, game_obj* obj)
{
    int i;
    // Controlla se l'oggetto è bloccato
    if (is_obj_locked(session, obj))
        return false;

    // Controlla se l'oggetto è consumato
    if (is_obj_consumed(session, obj))
        return false;

    // Controlla se c'è spazio nello zaino
    if (session->n_bag_objs == session->dim_bag)
        return false;
    
    // Inserimento dell'oggetto nel vettore degli oggetti nello zaino del giocatore
    for (i = 0; i < session->dim_bag; i++) 
    {
        // Trovo la prima posizione disponibile
        if (session->bag_objs[i] == NULL)
        {
            session->bag_objs[i] = obj;
            session->n_bag_objs++;
            return true;
        }
    }
    return false;
}

/*
 * Rimuove un oggetto dallo zaino del giocatore.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj: Il puntatore all'oggetto da rimuovere.
 *
 * Restituisce:
 *   - true se l'oggetto è stato rimosso con successo dallo zaino.
 *   - false se l'oggetto non è presente nello zaino.
 *
 * La funzione scorre tutti gli oggetti nello zaino del giocatore e cerca l'oggetto da rimuovere.
 * Se l'oggetto è trovato, viene rimosso dallo zaino e la funzione restituisce true.
 * Se l'oggetto non è presente nello zaino, la funzione restituisce false.
 */
static bool drop_obj(game_session* session, game_obj* obj) 
{
    int i;
    // Scorre tutti gli oggetti nello zaino
    for (i = 0; i < session->dim_bag; i++) 
    {
        if (session->bag_objs[i] != obj)
            continue;
        // Oggetto trovato
        session->bag_objs[i] = NULL;
        session->n_bag_objs--;
        return true;
    }
    // Oggetti non trovato
    return false;
}

/*
 * Sblocca un oggetto per l'utente specificato.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj_name: Il nome dell'oggetto da sbloccare.
 *
 * La funzione cerca l'oggetto nella stanza corrente dell'utente e, se l'oggetto è bloccato, lo sblocca
 * e applica le azioni di sblocco associate all'oggetto.
 */
static void unlock_obj(game_session* session, const char* obj_name) 
{
    int i;
    game_obj* obj = find_obj(session, obj_name, true);
    if (obj == NULL)
        return;

    // Controlla se l'oggetto è bloccato
    if (is_obj_locked(session, obj) == false)
        // L'oggetto non è bloccato, non c'è nulla da fare
        return;

    // Inseriemento dell'oggetto nel vettore degli oggetti sbloccati dal giocatore
    for (i = 0; i < session->n_locked_objs; i++) 
    {
        // Trovo la prima posizione disponibile
        if (session->unlocked_objs[i] == NULL)
        {
            session->unlocked_objs[i] = obj;
            break;
        }
    }

    // Scorre ed esegue tutte le azioni
    for (i = 0; i < obj->lock.n_actions; i++)
        compute_action(session, &obj->lock.actions[i]);
}

/*
 * Rende visibile un oggetto all'utente specificato.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj_name: Il nome dell'oggetto da rendere visibile.
 *
 * La funzione cerca l'oggetto nella stanza corrente dell'utente e, se l'oggetto è nascosto, lo rende visibile.
 */
static void reveal_obj(game_session* session, const char* obj_name)
{
    int i;
    game_obj* obj = find_obj(session, obj_name, false);
    if (obj == NULL)
        return;

    // Controlla se l'oggetto è nascosto
    if (is_obj_hidden(session, obj) == false)
        // L'oggetto non è nascosto, non c'è nulla da fare
        return;
    
    // Inseriemento dell'oggetto nel vettore degli oggetti visibili al giocatore
    for (i = 0; i < session->n_hidden_objs; i++) 
    {
        // Trovo la prima posizione disponibile
        if (session->revealed_objs[i] == NULL)
        {
            session->revealed_objs[i] = obj;
            break;
        }
    }
}

/*
 * Esegue l'azione specificata per l'utente nella sessione di gioco corrente.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - action: Il puntatore all'azione da eseguire.
 */
static void compute_action(game_session* session, game_action* action)
{
    switch (action->type)
    {
        case ACTION_TOKEN:
        {
            #ifdef VERBOSE
                printf("↳ Assegno un token alla sessione del socket %d\n", session->sd);
            #endif
            session->token++;
            break;
        }
        case ACTION_UNLOCK:
        {
            #ifdef VERBOSE
                printf("↳ Sblocco l'oggetto \"%s\" per la sessione del socket %d\n", action->obj_name, session->sd);
            #endif
            unlock_obj(session, action->obj_name);
            break;
        }
        case ACTION_REVEAL:
        {
            #ifdef VERBOSE
                printf("↳ Rendo visibile l'oggetto \"%s\" per la sessione del socket %d\n", action->obj_name, session->sd);
            #endif
            reveal_obj(session, action->obj_name);
            break;
        }
        case ACTION_CONSUME: 
        {
            #ifdef VERBOSE
                printf("↳ Consumo l'oggetto \"%s\" per la sessione del socket %d\n", action->obj_name, session->sd);
            #endif
            consume_obj(session, action->obj_name);
            break;
        }
    }
}

/*
 * Gestisce il caso in cui l'autenticazione dell'utente è avvenuta con successo.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 * 
 * Restituisce:
 *   - OK se lo stato del giocatore è stato inviato con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result authUserSuccess(int sd) 
{
    desc_msg msg;

    #ifdef VERBOSE
        printf("↳ Il socket %d si è autenticato con successo, invio della notifica al client\n", sd);
    #endif

    // Invia un messaggio di successo al client
    init_msg(&msg, MSG_SUCCESS);
    return send_to_socket(sd, &msg);
}

/*
 * Gestisce il caso in cui l'autenticazione dell'utente è fallita a causa di credenziali non valide.
 * Invia un messaggio di errore al client indicando che le credenziali fornite non sono valide.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 * 
 * Restituisce:
 *   - OK se lo stato del giocatore è stato inviato con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result authUserFailed(int sd)
{
    desc_msg msg;

    #ifdef VERBOSE
        printf("↳ Il socket %d ha fallito l'autenticazione, invio della notifica al client\n", sd);
    #endif

    // Invia il messaggio di errore al client
    init_msg(&msg, MSG_AUTH_ERR_INVALID_CREDENTIALS);
    return send_to_socket(sd, &msg);
}

/*
 * Gestisce il caso in cui la registrazione dell'utente è fallita a causa di un nome utente già esistente.
 * Invia un messaggio di errore al client indicando che il nome utente fornito è già in uso.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 * 
 * Restituisce:
 *   - OK se lo stato del giocatore è stato inviato con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result authUsernameExists(int sd)
{
    desc_msg msg;

    #ifdef VERBOSE
        printf("↳ Il socket %d ha fallito la registrazione a causa di username già esistente, invio della notifica al client\n", sd);
    #endif

    // Invia il messaggio di errore al client
    init_msg(&msg, MSG_AUTH_ERR_USERNAME_EXISTS);
    return send_to_socket(sd, &msg);
}

/*
 * Gestisce la disconnessione di un utente terminando una sua eventuale sessione di gioco.
 * 
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 */
void authUserDisconnected(int sd)
{
    #ifdef VERBOSE
        printf("↳ Il socket %d si è disconnesso, l'eventuale sessione viene terminata\n", sd);
    #endif
    stop_session(sd);
}

/*
 * Invia lo stato di gioco al client, includendo il tempo rimanente, il numero di token, il numero di oggetti nello zaino e il numero dell'ultimo messaggio di aiuto.
 * Se il tempo rimanente è scaduto, invia un messaggio di fine gioco per timeout e rimuove il giocatore dalla lista degli utenti attivi.
 * Se il giocatore ha ottenuto tutti i token della stanza, invia un messaggio di fine gioco indicando la vittoria e rimuove il giocatore dalla lista degli utenti attivi.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 * 
 * Restituisce:
 *   - OK se lo stato del giocatore è stato inviato con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
static op_result sendGameState(game_session* session) 
{
    desc_msg msg;

    // Calcola il tempo rimanente per il giocatore
    time_t remaining_time = get_remaining_time(session);

    if (remaining_time <= 0) 
    {
        #ifdef VERBOSE
            printf("↳ Tempo massimo superato, la sessione viene terminata\n");
        #endif
        // Il tempo è scaduto, il gioco finisce
        op_result ret;
        init_msg(&msg, MSG_GAME_END_TIMEOUT, rooms[session->room].timeout_text);

        // Invia il messaggio al client
        ret = send_to_socket(session->sd, &msg);
        if (ret != OK)
            return ret;

        // Termina la sessione di gioco,
        // deallocando tutta la memoria ad assa associata
        stop_session(session->sd);

        return GAME_END_TIMEOUT;
    }
    else if (session->token == rooms[session->room].token) 
    {
        #ifdef VERBOSE
            printf("↳ Il giocatore ha ottenuto tutti i token, la sessione viene terminata\n");
        #endif
        // Il giocatore ha ottenuto tutti i token della stanza, ha vinto
        op_result ret;
        init_msg(&msg, MSG_GAME_END_WIN, rooms[session->room].win_text);

        // Invia il messaggio al client
        ret = send_to_socket(session->sd, &msg);
        if (ret != OK)
            return ret;

        // Termina la sessione di gioco,
        // deallocando tutta la memoria ad assa associata
        stop_session(session->sd);

        return GAME_END_WIN;
    }

    return sendUserSessionState(session->sd, session);
}

/*
 * Invia i nomi delle stanze disponibili e il numero totale di stanze.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *
 * Restituisce:
 *   - OK se l'invio è riuscito.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso.
 *   - NET_ERR_SEND in caso di errori durante l'invio.
 * 
 * La funzione invia inizialmente il numero totale di stanze disponibili con il tipo di messaggio MSG_LIST_START.
 * Successivamente, invia i nomi delle stanze usando MSG_LIST_ITEM.
 */
op_result sendRoomNames(int sd) 
{
    op_result ret;
    desc_msg msg;
    int i;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ottenere le stanze disponibili\n", sd);
    #endif

    // Invia il numero totale di stanze disponibili
    init_msg(&msg, MSG_LIST_START, MAX_ROOMS);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invia i nomi delle stanze disponibili
    for (i = 0; i < MAX_ROOMS; i++) 
    {
        // Crea il messaggio con il nome della stanza
        init_msg(&msg, MSG_LIST_ITEM, rooms[i].name);
        
        // Invia il messaggio
        ret = send_to_socket(sd, &msg);
        if (ret != OK)
            return ret;
    }

    return ret;
}

/*
 * Avvia la sessione di gioco per il client specificato.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - username: nome dell'utente.
 *   - room: Indice della stanza in cui far iniziare il gioco.
 *
 * Restituisce:
 *   - OK se l'avvio del gioco è avvenuto con successo e tutte le informazioni sono state inviate correttamente al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio dei messaggi al client.
 *
 * La funzione verifica se l'indice della stanza è valido. Se l'indice è fuori dai limiti,
 * invia un messaggio di errore al client.
 * Successivamente, controlla se esiste già una sessione per l'username fornito e, in caso affermativo, invia un messaggio di errore.
 * Infine, avvia la sessione e invia le informazioni necessarie al client.
 */
op_result startGame(int sd, const char* username, int room) 
{
    desc_msg msg;
    op_result ret;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d con nome utente \"%s\" di avviare una sessione di gioco nella stanza \"%s\"\n", sd, username, rooms[room].name);
    #endif

    // Verifica se l'indice della stanza è valido
    if (room < 0 || room >= MAX_ROOMS) {
        #ifdef VERBOSE
            printf("↳ Indice della stanza non valido\n");
        #endif
        // Invia un messaggio di errore al client se l'indice della stanza è fuori dai limiti
        init_msg(&msg, MSG_GAME_ERR_INVALID_ROOM);
        return send_to_socket(sd, &msg);
    }

    // Controlla se esiste già una sessione con lo stesso username
    if (find_session_by_username(username) != NULL) {
        #ifdef VERBOSE
            printf("↳ Esiste già una sessione con questo username\n");
        #endif
        // Avvio fallito
        init_msg(&msg, MSG_GAME_ERR_INIT);
        return send_to_socket(sd, &msg);
    }

    // Avvia la sessione di gioco
    if (!start_session(sd, username, room)) {
        #ifdef VERBOSE
            printf("↳ Avvio della sessione non riuscito\n");
        #endif
        // Avvio fallito
        init_msg(&msg, MSG_GAME_ERR_INIT);
        return send_to_socket(sd, &msg);
    }

    #ifdef VERBOSE
        printf("↳ Sessione avviata, invio dei dati iniziali: descrizione della stanza, dimensione zaino e numero di token\n");
    #endif
    
    // Invia le informazioni necessarie per l'inizio del gioco
    init_msg(&msg, MSG_GAME_INIT, rooms[room].seconds, rooms[room].dim_bag, rooms[room].token);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invia la storia della room
    init_msg(&msg, MSG_GAME_DESCR, rooms[room].story);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    return ret;
}

/*
 * Gestisce il comando "look" inviato dal client, restituendo la descrizione richiesta.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - what: Nome dell'oggetto, della locazione o il valore "" per richiedere la descrizione della stanza.
 *
 * Restituisce:
 *   - OK se la descrizione è stata inviata con successo al client.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 *
 * La funzione verifica se l'utente è in gioco, restituendo un errore al client se non lo è.
 * Successivamente, invia lo stato aggiornato della sessione al client e gestisce la richiesta. 
 * Se l'oggetto o la locazione non è presente nella stanza, invia un messaggio di errore al client.
 */
op_result cmdLook(int sd, const char* what) 
{
    int i;
    const char* descr = NULL;
    desc_msg msg;
    op_result ret;
    game_session* session = find_session_by_sd(sd);

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ottenere una descrizione\n", sd);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Gestisce le richieste di descrizione per la stanza, le locazioni e gli oggetti
    if (strlen(what) == 0)
    {
        #ifdef VERBOSE
            printf("↳ Invio descrizione della stanza\n");
        #endif
        init_msg(&msg, MSG_GAME_DESCR, rooms[session->room].descr);
        return send_to_socket(sd, &msg);
    }

    // Ricerca tra le locazioni e gli oggetti
    for (i = 0; i < rooms[session->room].n_locations; i++) 
    {
        int j;
        if (strcmp(rooms[session->room].locations[i].name, what) == 0) 
        {
            // Locazione trovata
            descr = rooms[session->room].locations[i].descr;
            break;
        }

        for (j = 0; j < rooms[session->room].locations[i].n_objs; j++) 
        {
            if (strcmp(rooms[session->room].locations[i].objs[j].name, what) != 0)
                continue;
            
            // Oggetto trovato
            if (rooms[session->room].locations[i].objs[j].isHidden)
            {
                if (is_obj_hidden(session, &rooms[session->room].locations[i].objs[j])) 
                {
                    i = rooms[session->room].n_locations;
                    break;
                }
            }

            if (rooms[session->room].locations[i].objs[j].isLocked) 
            {
                if (is_obj_locked(session, &rooms[session->room].locations[i].objs[j]))
                    descr = rooms[session->room].locations[i].objs[j].locked_descr;
                else
                    descr = rooms[session->room].locations[i].objs[j].unlocked_descr;
            }
            else
                descr = rooms[session->room].locations[i].objs[j].unlocked_descr;

            break;
        }

        if (descr != NULL)
            break;
    }

    // Invia un messaggio di errore se l'oggetto o la locazione richiesta non è presente nella stanza
    if (descr == NULL)
    {
        #ifdef VERBOSE
            printf("↳ Descrizione di \"%s\" non trovata\n", what);
        #endif
        init_msg(&msg, MSG_GAME_ERR_NOT_FOUND);
        return send_to_socket(sd, &msg);
    }

    #ifdef VERBOSE
        printf("↳ Invio della descrizione di \"%s\"\n", what);
    #endif

    // Invia la descrizione richiesta al client
    init_msg(&msg, MSG_GAME_DESCR, descr);
    return send_to_socket(sd, &msg);
}

/*
 * Gestisce il comando "objs" inviato dal client per richiedere la lista degli oggetti nello zaino del giocatore.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *
 * Restituisce:
 *   - OK se il comando è stato gestito correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 * 
 * La funzione verifica se l'utente è in gioco. In caso contrario, invia un messaggio di errore al client.
 * Successivamente, invia lo stato corrente del giocatore e procede a inviare il numero totale di oggetti nello zaino,
 * seguito dalla lista degli oggetti. La lista viene inviata attraverso messaggi di tipo MSG_LIST_START e MSG_LIST_ITEM.
 */
op_result cmdObjs(int sd) 
{
    desc_msg msg;
    op_result ret;
    game_session* session = find_session_by_sd(sd);
    int i, sent;

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ricevere la lista di oggetti nello zaino\n", sd);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    #ifdef VERBOSE
        printf("↳ Invio degli oggetti\n");
    #endif

    // Invia il numero totale di oggetti nello zaino
    init_msg(&msg, MSG_LIST_START, session->n_bag_objs);
    ret = send_to_socket(sd, &msg);
    if (ret != OK)
        return ret;

    // Invia gli oggetti
    for (i = 0, sent = 0; i < session->dim_bag && sent < session->n_bag_objs; i++) 
    {
        if (session->bag_objs[i] == NULL)
            continue;

        // Crea il messaggio con il nome dell'oggetto
        init_msg(&msg, MSG_LIST_ITEM, session->bag_objs[i]->name);
        
        // Invia il messaggio
        ret = send_to_socket(sd, &msg);
        if (ret != OK)
            return ret;

        sent++;
    }

    return ret;
}

/*
 * Gestisce il comando "use" inviato dal client.
 * Determina il tipo di uso richiesto e inoltra la richiesta alla funzione corretta per l'elaborazione.
 * Se il client richiede di usare un oggetto da solo, chiama la funzione cmdUseAlone. Se il client richiede di combinare
 * due oggetti, chiama la funzione cmdUseCombine.
 *
 * Parametri:
 *   - sd: Descrittore del socket del client.
 *   - obj1_name: Nome del primo oggetto.
 *   - obj2_name: Nome del secondo oggetto, se presente.
 *
 * Restituisce:
 *   - OK se l'operazione è stata eseguita con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
op_result cmdUse(int sd, const char* obj1_name, const char* obj2_name) 
{
    desc_msg msg;
    op_result ret;
    game_session* session = find_session_by_sd(sd);

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di usare degli oggetti\n", sd);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Esegue il comando
    if (obj2_name == NULL || strlen(obj2_name) == 0)
        ret = cmdUseAlone(session, find_obj(session, obj1_name, true));
    else
        ret = cmdUseCombine(session, find_obj(session, obj1_name, true), find_obj(session, obj2_name, true));
    
    return ret;
}

/*
 * Gestisce l'uso di un oggetto da parte di un giocatore.
 * Controlla se l'oggetto esiste, se è bloccato, se può essere usato, se è stato consumato e se può essere utilizzato da solo.
 * Se l'uso è consentito, esegue le azioni associate all'uso e invia lo stato aggiornato del giocatore e un messaggio descrittivo al client.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj: Puntatore all'oggetto che l'utente sta cercando di usare.
 *
 * Restituisce:
 *   - OK se l'operazione è stata eseguita con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
static op_result cmdUseAlone(game_session* session, game_obj* obj) 
{
    desc_msg msg;
    op_result ret;

    int i;
    game_use* use;

    // Controlla se l'oggetto esiste
    if (obj == NULL)
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto non esiste o non è visibile al giocatore\n");
        #endif
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;
        
        // Invia il messaggio di errore
        init_msg(&msg, MSG_GAME_ERR_NOT_FOUND);
        return send_to_socket(session->sd, &msg);
    }

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di usare l'oggetto \"%s\"\n", session->sd, obj->name);
    #endif

    // Controlla se l'oggetto è bloccato
    if (is_obj_locked(session, obj)) 
    {
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;

        if (obj->lock.type == LOCK_OBJ)
        {
            // Per sbloccare l'oggetto è necessario l'uso di un altro oggetto
            #ifdef VERBOSE
                printf("↳ L'oggetto \"%s\" è bloccato da un altro oggetto\n", obj->name);
            #endif
            init_msg(&msg, MSG_GAME_INF_LOCK_ACTION);
            return send_to_socket(session->sd, &msg);
        }
        
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è bloccato da un enigma, invio del testo al client\n", obj->name);
        #endif
        // L'oggetto è bloccato da un enigma
        init_msg(&msg, MSG_GAME_INF_LOCK_PUZZLE, obj->lock.puzzle.text);
        return send_to_socket(session->sd, &msg);
    }

    // Controlla se l'oggetto può essere usato
    if (obj->n_uses == 0) 
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non può essere usato in alcun modo\n", obj->name);
        #endif
        // L'oggetto non può essere usato in alcun modo
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;
        
        // Invia il messaggio di errore
        init_msg(&msg, MSG_GAME_INF_OBJ_NO_USE);
        return send_to_socket(session->sd, &msg);
    }

    // Controlla se l'oggetto è stato consumato
    if (is_obj_consumed(session, obj)) 
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è consumato, pertanto non può essere riutilizzato\n", obj->name);
        #endif
        // Non può essere usato
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;
            
        // Invia il messaggio di errore
        init_msg(&msg, MSG_GAME_INF_OBJ_CONSUMED);
        return send_to_socket(session->sd, &msg);
    }
    
    // Scorre tutti i modi d'uso
    use = NULL;
    for (i = 0; i < obj->n_uses; i++) 
    {
        if (obj->uses[i].type != USE_ALONE)
            continue;

        // Modo d'uso trovato
        use = &obj->uses[i];
        break;
    }

    if (use == NULL)
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non può essere usato da solo\n", obj->name);
        #endif
        // L'oggetto non può essere usato da solo
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;

        // Invia il messaggio di errore
        init_msg(&msg, MSG_GAME_INF_OBJ_NO_USE);
        return send_to_socket(session->sd, &msg);
    }

    #ifdef VERBOSE
        printf("↳ Esecuzione di tutte le azioni specificate per l'uso\n");
    #endif

    // Scorre ed esegue tutte le azioni
    for (i = 0; i < use->n_actions; i++)
        compute_action(session, &use->actions[i]);

    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Invia il messaggio
    init_msg(&msg, MSG_GAME_DESCR, use->use_descr);
    return send_to_socket(session->sd, &msg);
}

/*
 * Gestisce l'uso combinato di due oggetti da parte di un giocatore.
 * Controlla se entrambi gli oggetti esistono, se il primo oggetto è nello zaino, se il secondo oggetto è stato consumato,
 * se l'oggetto 1 può essere combinato con l'oggetto 2 e se l'oggetto 2 è bloccato.
 * Se l'uso combinato è consentito, esegue le azioni associate all'uso e invia lo stato aggiornato del giocatore e un messaggio descrittivo al client.
 *
 * Parametri:
 *   - session: Il puntatore alla sessione di gioco.
 *   - obj1: Puntatore al primo oggetto.
 *   - obj2: Puntatore al secondo oggetto.
 *
 * Restituisce:
 *   - OK se l'operazione è stata eseguita con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
static op_result cmdUseCombine(game_session* session, game_obj* obj1, game_obj* obj2) 
{
    desc_msg msg;
    msg_type msg_ret_type;
    op_result ret;

    int i;
    game_use* use;

    // Controlla se gli oggetti esistono
    if (obj1 == NULL || obj2 == NULL)
    {
        #ifdef VERBOSE
            printf("↳ Uno degli oggetti non esiste o non è visibile al giocatore\n");
        #endif
        msg_ret_type = MSG_GAME_ERR_NOT_FOUND;
        goto end;
    }

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di combinare \"%s\" con \"%s\"\n", session->sd, obj1->name, obj2->name);
    #endif

    // Controlla se l'oggetto 1 è nello zaino del giocatore
    if (is_obj_taken(session, obj1) == false) 
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non è nello zaino del giocatore, pertanto non può essere combinato\n", obj1->name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_NOT_TAKEN;
        goto end;
    }

    // Controlla se l'oggetto 2 è stato consumato
    if (is_obj_consumed(session, obj2)) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è consumato, pertanto non può essere usato\n", obj2->name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_CONSUMED;
        goto end;
    }

    // Controllo se l'oggetto 1 può essere combinato con l'oggetto 2
    use = NULL;
    for (i = 0; i < obj1->n_uses; i++) 
    {
        if (obj1->uses[i].type != USE_COMBINE)
            continue;
        if (strcmp(obj1->uses[i].otherObj, obj2->name) != 0)
            continue;
        use = &obj1->uses[i];
        break;
    }

    if (use == NULL) {
        // Non è possibile combinare l'oggetto 1 con l'oggetto 2
        #ifdef VERBOSE
            printf("↳ La combinazione di \"%s\" con \"%s\" non è definita\n", obj1->name, obj2->name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_NO_USE;
        goto end;
    }
    // Uso combinato trovato
    // Controlla se l'oggetto 2 è bloccato
    if (is_obj_locked(session, obj2)) 
    {
        // Controllo se l'oggetto 2 è bloccato da un enigma
        if (obj2->lock.type == LOCK_PUZZLE) 
        {
            #ifdef VERBOSE
                printf("↳ L'oggetto \"%s\" è bloccato da un enigma, pertanto non può essere combinato\n", obj2->name);
            #endif

            // Se l'oggetto 2 è bloccato da un enigma, non è ancora possibile usarlo con l'oggetto 1
            // Invia lo stato aggiornato della sessione al client
            ret = sendGameState(session);
            if (ret != OK)
                return ret;

            // Invia al client l'enigma da risolvere
            init_msg(&msg, MSG_GAME_INF_LOCK_PUZZLE, obj2->lock.puzzle.text);
            return send_to_socket(session->sd, &msg);
        }
        
        // L'oggetto 2 è bloccato da un altro oggetto
        // Controllo se tra le azioni da eseguire c'è lo sblocco dell'oggetto 2
        bool found = false;
        for (i = 0; i < use->n_actions; i++) 
        {
            if (use->actions[i].type != ACTION_UNLOCK)
                continue;
            if (strcmp(use->actions[i].obj_name, obj2->name) != 0)
                continue;
            // Azione trovata. L'oggetto 2 viene sbloccato dall'oggetto 1
            found = true;
            break;
        }

        if (found == false) {
            #ifdef VERBOSE
                printf("↳ L'oggetto \"%s\" è bloccato da un altro oggetto e l'uso con \"%s\" non lo sblocca\n", obj2->name, obj1->name);
            #endif
            // Azione non trovata. L'uso combinato dell'oggetto 1 con l'oggetto 2, non sblocca l'oggetto 2.
            // Pertanto, non è ancora possibile usarli insieme
            msg_ret_type = MSG_GAME_INF_LOCK_ACTION;
            goto end;
        }
    }

    #ifdef VERBOSE
        printf("↳ Esecuzione di tutte le azioni specificate per l'uso\n");
    #endif

    // Scorre ed esegue tutte le azioni
    for (i = 0; i < use->n_actions; i++)
        compute_action(session, &use->actions[i]);

    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Invia il messaggio
    init_msg(&msg, MSG_GAME_DESCR, use->use_descr);
    return send_to_socket(session->sd, &msg);

end:
    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;
    
    // Invia il risultato dell'operazione al client
    init_msg(&msg, msg_ret_type);
    return send_to_socket(session->sd, &msg);
}

/*
 * Gestisce il comando "take" inviato dal client per raccogliere un oggetto nel gioco.
 * Verifica se l'utente è in gioco, se l'oggetto esiste, se è possibile raccoglierlo e se ci sia spazio nello zaino.
 * Controlla anche se l'oggetto è già nello zaino, se è stato consumato o se è bloccato da un enigma.
 * Invia lo stato aggiornato del giocatore e il risultato dell'operazione al client.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - obj_name: Nome dell'oggetto da raccogliere.
 *
 * Restituisce:
 *   - OK se l'operazione è stata eseguita con successo.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
op_result cmdTake(int sd, const char* obj_name) 
{
    desc_msg msg;
    msg_type msg_ret_type;
    op_result ret;
    game_session* session = find_session_by_sd(sd);
    game_obj* obj = find_obj(session, obj_name, true);

    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di raccogliere l'oggetto \"%s\"\n", sd, obj_name);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Controlla se l'oggetto esiste
    if (obj == NULL) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non esiste o non è visibile al giocatore\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_ERR_NOT_FOUND;
        goto end;
    }

    // Controlla se l'oggetto può essere raccolto
    if (obj->takeable == false) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non può essere raccolto\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_NO_TAKE;
        goto end;
    }

    // Controlla se c'è spazio nello zaino
    if (session->n_bag_objs == session->dim_bag) {
        #ifdef VERBOSE
            printf("↳ Il giocatore ha esaurito lo spazio nello zaino\n");
        #endif
        msg_ret_type = MSG_GAME_INF_BAG_FULL;
        goto end;
    }

    // Controlla se l'oggetto è già nello zaino
    if (is_obj_taken(session, obj)) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è già nello zaino del giocatore\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_TAKEN;
        goto end;
    }

    // Controlla se l'oggetto è stato consumato
    if (is_obj_consumed(session, obj)) {
        // Non può essere raccolto
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è consumato, pertanto non può essere raccolto\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_CONSUMED;
        goto end;
    }

    // Controlla se l'oggetto è bloccato
    if (is_obj_locked(session, obj))
    {
        if (obj->lock.type == LOCK_OBJ)
        {
            // Per sbloccare l'oggetto è necessario l'uso di un altro oggetto
            #ifdef VERBOSE
                printf("↳ L'oggetto \"%s\" è bloccato da un altro oggetto, pertanto non può essere raccolto\n", obj_name);
            #endif
            msg_ret_type = MSG_GAME_INF_LOCK_ACTION;
            goto end;
        }

        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" è bloccato da un enigma, invio del testo al client\n", obj_name);
        #endif
        // L'oggetto è bloccato da un enigma
        // Invia lo stato aggiornato della sessione al client
        ret = sendGameState(session);
        if (ret != OK)
            return ret;
        
        // Invia al client l'enigma da risolvere
        init_msg(&msg, MSG_GAME_INF_LOCK_PUZZLE, obj->lock.puzzle.text);
        return send_to_socket(session->sd, &msg);
    }

    // Raccoglie l'oggetto
    take_obj(session, obj);

    #ifdef VERBOSE
        printf("↳ \"%s\" raccolto\n", obj_name);
    #endif

    // Invia il messaggio di successo
    msg_ret_type = MSG_SUCCESS;

end:
    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Invia il risultato dell'operazione
    init_msg(&msg, msg_ret_type);
    return send_to_socket(session->sd, &msg);
}

/*
 * Rimuove un oggetto dallo zaino del giocatore
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 *   - obj_name: Nome dell'oggetto da rimuovere dallo zaino.
 *
 * Restituisce:
 *   - OK se l'oggetto è stato rimosso con successo dallo zaino e il messaggio di conferma è stato inviato correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
op_result cmdDrop(int sd, const char* obj_name) 
{
    desc_msg msg;
    msg_type msg_ret_type;
    op_result ret;
    game_session* session = find_session_by_sd(sd);
    game_obj* obj = find_obj(session, obj_name, true);
    
    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di rilasciare l'oggetto \"%s\"\n", sd, obj_name);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Controlla se l'oggetto esiste
    if (obj == NULL) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non esiste o non è visibile al giocatore\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_ERR_NOT_FOUND;
        goto end;
    }

    // Controlla se l'oggetto è nello zaino
    if (is_obj_taken(session, obj) == false) {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non è nello zaino del giocatore\n", obj_name);
        #endif
        msg_ret_type = MSG_GAME_INF_OBJ_NOT_TAKEN;
        goto end;
    }
    
    // Rimuove l'oggetto dallo zaino
    drop_obj(session, obj);

    #ifdef VERBOSE
        printf("↳ \"%s\" rimosso dallo zaino\n", obj_name);
    #endif

    // Invia il messaggio di successo
    msg_ret_type = MSG_SUCCESS;

end:
    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Invia il risultato dell'operazione
    init_msg(&msg, msg_ret_type);
    return send_to_socket(session->sd, &msg);
}

/*
 * Gestisce il comando "help" inviato dal client per ottenere il messaggio di aiuto più recente.
 * 
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il client.
 *   - last_help_msg_id: Il numero dell'ultimo messaggio di aiuto ricevuto dal client.
 *
 * Restituisce:
 *   - OK se l'oggetto è stato rimosso con successo dallo zaino e il messaggio di conferma è stato inviato correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 */
op_result cmdHelp(int sd, int last_help_msg_id) 
{
    desc_msg msg;
    op_result ret;
    game_session* session = find_session_by_sd(sd);
    
    #ifdef VERBOSE
        printf("↳ Richiesta dal socket %d di ottenere il messaggio di aiuto più recente\n", sd);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;

    // Controlla se c'è un messaggio di aiuto disponibile
    if (session->help_msg_id == 0) {
        #ifdef VERBOSE
            printf("↳ Non c'è nessun messaggio di aiuto\n");
        #endif
        init_msg(&msg, MSG_GAME_INF_HELP_NO_MSG);
        return send_to_socket(sd, &msg);
    }

    // Controlla se il messaggio di aiuto più recente è già stato ricevuto dal client
    if (session->help_msg_id == last_help_msg_id) {
        // Il client ha già l'ultimo messaggio disponibile
        #ifdef VERBOSE
            printf("↳ Il client ha già l'ultimo messaggio di aiuto disponibile\n");
        #endif
        init_msg(&msg, MSG_GAME_INF_HELP_NO_NEW_MSG);
        return send_to_socket(sd, &msg);
    }

    // Invia il messaggio di aiuto più recente al client
    #ifdef VERBOSE
        printf("↳ Invio del messaggio di aiuto \"%s\"\n", session->help_msg);
    #endif
    init_msg(&msg, MSG_GAME_DESCR, session->help_msg);
    return send_to_socket(sd, &msg);
}

/*
 * Gestisce il comando inviato dal client per terminare volontariamente la partita.
 *
 * Parametri:
 *   - sd: Descrittore del socket per la comunicazione con il client.
 * 
 * Restituisce:
 *   - OK se il messaggio di fine partita è stato inviato correttamente.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto si è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errori nell'invio del messaggio al client.
 */
op_result cmdEnd(int sd) 
{
    desc_msg msg;
    game_session* session = find_session_by_sd(sd);

    #ifdef VERBOSE
        printf("↳ Richiesta di terminare la sessione legata al socket %d\n", sd);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Inizializza il messaggio di fine partita
    init_msg(&msg, MSG_GAME_END_QUIT, rooms[session->room].quit_text);

    // Termina la sessione di gioco,
    // deallocando tutta la memoria ad assa associata
    stop_session(session->sd);

    #ifdef VERBOSE
        printf("↳ Sessione terminata\n");
    #endif

    // Invia il messaggio di fine partita
    return send_to_socket(sd, &msg);
}

/*
 * Controlla se la soluzione proposta dall'utente per risolvere un enigma è corretta.
 * Se la soluzione è corretta, l'oggetto bloccato dall'enigma viene sbloccato.
 *
 * Parametri:
 *   - sd: Il descrittore del socket per la comunicazione con il client.
 *   - obj_name: Il nome dell'oggetto per cui è richiesta la soluzione dell'enigma.
 *   - solution: La soluzione proposta dall'utente.
 *
 * Restituisce:
 *   - OK se la soluzione è stata controllata correttamente e eventualmente l'oggetto è stato sbloccato.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso durante la comunicazione con il client.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 *   - GAME_END_TIMEOUT se il tempo è scaduto e il gioco è terminato.
 *   - GAME_END_WIN se il giocatore ha ottenuto tutti i token della stanza e il gioco è terminato.
 *
 * La funzione verifica se l'utente è in gioco e se l'oggetto esiste nella stanza corrente dell'utente.
 * Successivamente, controlla se l'oggetto è bloccato da un enigma. Se sì, verifica se la soluzione proposta
 * dall'utente è corretta. Se la soluzione è corretta, l'oggetto viene sbloccato e viene inviato un messaggio di successo al client.
 * Se la soluzione è errata, viene inviato un messaggio di insuccesso al client.
 */
op_result checkPuzzleSolution(int sd, const char* obj_name, const char* solution) 
{
    desc_msg msg;
    op_result ret;
    msg_type msg_res_type;
    game_session* session = find_session_by_sd(sd);
    game_obj* obj;

    #ifdef VERBOSE
        printf("↳ Controllo soluzione enigma per l'oggetto %s\n", obj_name);
    #endif

    // Verifica se l'utente è in gioco
    if (session == NULL) 
    {
        #ifdef VERBOSE
            printf("↳ Non esiste nessuna sessione di gioco associata al socket %d\n", sd);
        #endif
        init_msg(&msg, MSG_GAME_ERR_CMD_NOT_ALLOWED);
        return send_to_socket(sd, &msg);
    }

    // Controlla se l'oggetto esiste
    obj = find_obj(session, obj_name, true);
    if (obj == NULL)
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non esiste o non è visibile al giocatore\n", obj_name);
        #endif
        msg_res_type = MSG_GAME_ERR_NOT_FOUND;
        goto end;
    }

    // Controlla se l'oggetto è bloccato da un enigma
    if (is_obj_locked(session, obj) == false || obj->lock.type != LOCK_PUZZLE) 
    {
        #ifdef VERBOSE
            printf("↳ L'oggetto \"%s\" non è bloccato da un enigma\n", obj_name);
        #endif
        msg_res_type = MSG_GAME_INF_OBJ_NO_LOCK;
        goto end;
    }

    // Controlla se la risposta è giusta
    if (strcmp(obj->lock.puzzle.solution, solution) == 0) 
    {
        #ifdef VERBOSE
            printf("↳ Risposta corretta, sblocco di %s\n", obj_name);
        #endif
        // Risposta corretta, sblocca l'oggetto
        unlock_obj(session, obj_name);

        // Invia il messaggio di successo
        msg_res_type = MSG_SUCCESS;
        goto end;
    }
    
    #ifdef VERBOSE
        printf("↳ Risposta sbagliata\n");
    #endif
    // La risposta è sbagliata
    msg_res_type = MSG_GAME_PUZZLE_WRONG;

end:
    // Invia lo stato aggiornato della sessione al client
    ret = sendGameState(session);
    if (ret != OK)
        return ret;
    
    // Invia il risultato dell'operazione al client
    init_msg(&msg, msg_res_type);
    return send_to_socket(session->sd, &msg);
}
