#ifndef UI
#define UI

#ifdef GAME_SERVER
/*------Server UI------*/
const char* SER_START_MENU =    "***************************** SERVER STARTED *****************************\n\n"
                                "Comandi:\n"
                                "> start <%d>\t--> avvia il server di gioco\n"
                                "> stop\t\t--> termina il server\n"
                                "\n**************************************************************************\n";
#endif

#ifdef GAME_CLIENT
/*------Client UI------*/
const char* CLI_AUTH_MENU = "******************************* ESCAPE ROOM ******************************\n\n"
                            "Comandi:\n"
                            "> login\t\t--> accedi ad un account esistente\n"
                            "> signup\t--> crea un nuovo account\n"
                            "> quit\t\t--> esci dal gioco\n"
                            "\n**************************************************************************\n";

const char* CLI_MAIN_MENU_HEADER =  "******************************* ESCAPE ROOM ******************************\n\n"
                                    "Comandi:\n"
                                    "> start <room>\t--> entra nella room e inizia a giocare\n"
                                    "> quit\t\t--> esci dal gioco\n";
const char* CLI_MAIN_MENU_TRAILER = "**************************************************************************\n";

const char* CLI_GAME_MENU_HELP_HEADER =     "******************************* ESCAPE ROOM ******************************\n"
                                            "Tempo rimanente: %d\tToken: %d/%d\tOggetti nello zaino: %d/%d\n\n%s\n\nAiuto:\n> %s\n\n";
const char* CLI_GAME_MENU_NEW_HELP_MSG =    "**Nuovo messaggio di aiuto**\n\n";

const char* CLI_GAME_MENU_HEADER =  "******************************* ESCAPE ROOM ******************************\n"
                                    "Tempo rimanente: %d\tToken: %d/%d\tOggetti nello zaino: %d/%d\n\n%s\n\n";
const char* CLI_GAME_MENU_CMDS =    "Comandi:\n"
                                    "> look [location | object]\t--> fornisce una breve descrizione della stanza,\n\t\t\t\t    delle sue locazioni e dei suoi oggetti\n"
                                    "> take object\t\t\t--> raccoglie l'oggetto indicato\n"
                                    "> use object1 [object2]\t\t--> utilizza object1, eventualmente\n\t\t\t\t    combinandolo con object2 se specificato\n"
                                    "> objs\t\t\t\t--> mostra l'elenco degli oggetti nello zaino\n"
                                    "> drop object\t\t\t--> rimuovere un oggetto dallo zaino\n"
                                    "> help\t\t\t\t--> aggiorna il messaggio di aiuto\n"
                                    "> end\t\t\t\t--> termina il gioco\n"
                                    "\n**************************************************************************\n";

const char* CLI_GAME_WIN =  "******************************* ESCAPE ROOM ******************************\n"
                            "Hai vinto!\n\n"
                            "%s\n"
                            "\n**************************************************************************\n";

const char* CLI_GAME_TIMEOUT =  "******************************* ESCAPE ROOM ******************************\n"
                                "Tempo scaduto!, hai raccolto %d token su %d\n\n"
                                "%s\n"
                                "\n**************************************************************************\n";

const char* CLI_GAME_END =  "******************************* ESCAPE ROOM ******************************\n"
                            "Gioco terminato, hai raccolto %d token su %d\n\n"
                            "Addio, avventuriero! Speriamo di rivederti presto nel mondo dei misteri\n"
                            "\n**************************************************************************\n";
#endif

#ifdef GAME_SUPERVISOR
/*----Supervisor UI----*/
const char* SU_MAIN_MENU_HEADER =   "******************************* SUPERVISOR *******************************\n\n"
                                    "Comandi:\n"
                                    "> look <username>\t--> osserva il giocatore specificato\n"
                                    "> update\t\t--> aggiorna la lista delle sessioni\n"
                                    "> quit\t\t\t--> esci\n";
const char* SU_MAIN_MENU_TRAILER =  "**************************************************************************\n";

const char* SU_LOOK_MENU =  "******************************* SUPERVISOR *******************************\n"
                            "%s sta giocando in \"%s\"\n"
                            "Tempo rimanente: %d\tToken: %d/%d\tOggetti nello zaino: %d/%d\n\n"
                            "Comandi:\n"
                            "> objs\t\t\t\t\t--> stato degli oggetti bloccati della stanza\n"
                            "> bag\t\t\t\t\t--> lista degli oggetti nello zaino del giocatore\n"
                            "> time ((add | sub | set)) <sec>\t--> aggiunge, sottrae o imposta il tempo rimanente\n"
                            "> help\t\t\t\t\t--> invia un messaggio di aiuto al giocatore\n"
                            "> back\t\t\t\t\t--> torna al menu principale\n"
                            "\n**************************************************************************\n";
#endif

#endif