#include "shared.h"

/*
 * Inizializza un descrittore di messaggio con il tipo specificato e i campi payload.
 *
 * Parametri:
 *   - msg: Puntatore al descrittore del messaggio da inizializzare.
 *   - type: Tipo del messaggio da inizializzare.
 *   - ...: Argomenti variabili a seconda del tipo del messaggio.
 *
 * La funzione inizializza un descrittore di messaggio con il tipo specificato e i campi payload
 * in base al tipo di messaggio fornito. Accetta argomenti variabili che dipendono dal tipo del messaggio.
 */
void init_msg(desc_msg* msg, msg_type type, ...) 
{
    va_list argList;
    va_start(argList, type);

    switch (type)
    {
        case MSG_REQ_START_GAME:
        {
            const char* username = va_arg(argList, const char*);
            int room = va_arg(argList, int);

            sprintf(msg->payload, "%s %d", username, room);
            break;
        }
        case MSG_GAME_CMD_HELP:
        case MSG_LIST_START: 
        {
            sprintf(msg->payload, "%d", va_arg(argList, int));
            break;
        }
        case MSG_SU_REQ_USER_SESSION_ALTER_TIME:
        {
            const char* username = va_arg(argList, const char*);
            int seconds = va_arg(argList, int);
            const char opt = va_arg(argList, int);

            sprintf(msg->payload, "%s %d %c", username, seconds, opt);
            break;
        }
        case MSG_SU_REQ_USER_SESSION_DATA:
        case MSG_SU_REQ_USER_SESSION_OBJS:
        case MSG_SU_REQ_USER_SESSION_BAG:
        case MSG_LIST_ITEM:
        case MSG_GAME_END_WIN:
        case MSG_GAME_END_TIMEOUT:
        case MSG_GAME_CMD_DROP:
        case MSG_GAME_CMD_TAKE:
        case MSG_GAME_CMD_LOOK:
        case MSG_GAME_INF_LOCK_PUZZLE:
        case MSG_GAME_DESCR:
        {
            sprintf(msg->payload, "%s", va_arg(argList, const char*));
            break;
        }
        case MSG_SU_REQ_USER_SESSION_SET_HELP:
        case MSG_GAME_PUZZLE_SOL:
        case MSG_REQ_LOGIN:
        case MSG_REQ_SIGNUP:
        {
            const char* s1 = va_arg(argList, const char*);
            const char* s2 = va_arg(argList, const char*);

            sprintf(msg->payload, "%s %s", s1, s2);
            break;
        }
        case MSG_GAME_CMD_USE: 
        {
            const char* obj1 = va_arg(argList, const char*);
            const char* obj2 = va_arg(argList, const char*);

            if (obj2 == NULL || strlen(obj2) == 0)
                sprintf(msg->payload, "%s", obj1);
            else
                sprintf(msg->payload, "%s %s", obj1, obj2);
            break;
        }
        case MSG_GAME_INIT:
        {
            time_t room_seconds = va_arg(argList, time_t);
            int bag_size = va_arg(argList, int), room_token = va_arg(argList, int);

            sprintf(msg->payload, "%lld %d %d", (long long)room_seconds, bag_size, room_token);
            break;
        }
        case MSG_GAME_STATE:
        {
            time_t remaining_time = va_arg(argList, time_t);
            int user_token = va_arg(argList, int);
            int objs_in_bag = va_arg(argList, int);
            int help_msg_id = va_arg(argList, int);

            sprintf(msg->payload, "%lld %d %d %d", (long long)remaining_time, user_token, objs_in_bag, help_msg_id);
            break;
        }
        case MSG_SU_USER_SESSION_DATA: 
        {
            time_t remaining_time = va_arg(argList, time_t);
            int room_token = va_arg(argList, int);
            int user_token = va_arg(argList, int);
            int dim_bag = va_arg(argList, int);
            int objs_in_bag = va_arg(argList, int);

            sprintf(msg->payload, "%lld %d %d %d %d", (long long)remaining_time, room_token, user_token, dim_bag, objs_in_bag);
            break;
        }
        default:
        {
            memset(msg->payload, '\0', MAX_PAYLOAD_DIM);
            break;
        }
    }
    msg->payload[MAX_PAYLOAD_DIM - 1] = '\0';
    msg->type = type;
    va_end(argList);
}

/*
 * Invia un messaggio attraverso il socket specificato.
 *
 * Parametri:
 *   - sd: Descrittore del socket attraverso il quale inviare il messaggio.
 *   - msg: Puntatore al descrittore del messaggio da inviare.
 * 
 * Restituisce:
 *   - OK se l'invio è riuscito.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso.
 *   - NET_ERR_SEND in caso di errore nell'invio.
 */
op_result send_to_socket(int sd, desc_msg* msg) 
{
    int ret;
    uint16_t len;
    uint8_t type_of_msg = msg->type;

    // Invia il tipo di messaggio
    ret = send(sd, (void*)&type_of_msg, sizeof(uint8_t), MSG_NOSIGNAL /* flag che impedisce la generazione del segnale SIGPIPE in caso di chiusura del socket remoto */);
    if (ret < 0)
        goto err;

    // Prepara il payload del messaggio per l'invio
    msg->payload[MAX_PAYLOAD_DIM - 1] = '\0';

    // Converte la lunghezza del payload in formato di rete (big-endian)
    len = htons(strlen(msg->payload));

    // Invia la lunghezza al destinatario
    ret = send(sd, (void*)&len, sizeof(uint16_t), MSG_NOSIGNAL);
    if (ret < 0)
        goto err;

    if (len == 0)
        // Il payload del messaggio è vuoto
        return OK;

    // Invia il payload del messaggio al destinatario
    ret = send(sd, (void*)msg->payload, strlen(msg->payload), MSG_NOSIGNAL);
    if (ret < 0)
        goto err;

    return OK;

err:
    // Gestisce chiusura del socket remoto o errori nell'invio
    if (errno == EPIPE || errno == ECONNRESET)
        return NET_ERR_REMOTE_SOCKET_CLOSED;
    return NET_ERR_SEND;
}

/*
 * Riceve un messaggio dal socket specificato e lo memorizza nel descrittore del messaggio fornito.
 *
 * Parametri:
 *   - sd: Descrittore del socket dal quale ricevere il messaggio.
 *   - msg: Puntatore al descrittore del messaggio in cui memorizzare i dati ricevuti.
 *
 * Restituisce:
 *   - OK se la ricezione è riuscita.
 *   - NET_ERR_REMOTE_SOCKET_CLOSED se il socket remoto è chiuso.
 *   - NET_ERR_RECV in caso di errore nella ricezione.
 */
op_result receive_from_socket(int sd, desc_msg* msg) 
{
    int ret;
    uint16_t len;
    uint8_t type_of_msg;

    // Riceve il tipo di messaggio
    ret = recv(sd, (void*)&type_of_msg, sizeof(uint8_t), MSG_WAITALL);
    if (ret <= 0)
        goto err;
    msg->type = type_of_msg;

    // Riceve la lunghezza del payload in formato di rete (big-endian)
    ret = recv(sd, (void*)&len, sizeof(uint16_t), MSG_WAITALL);
    if (ret <= 0)
        goto err;

    // Converte la lunghezza in formato host (little-endian)
    len = ntohs(len);
    if (len == 0) {
        // Il payload del messaggio è vuoto
        memset(msg->payload, '\0', MAX_PAYLOAD_DIM);
        return OK;
    }

    // Riceve il payload del messaggio
    ret = recv(sd, (void*)msg->payload, len, MSG_WAITALL);
    if (ret <= 0)
        goto err;

    // Termina la stringa ricevuta
    msg->payload[len] = '\0';

    return OK;

err:
    if (ret == 0) {
        // Gestisce la chiusura del socket remoto
        return NET_ERR_REMOTE_SOCKET_CLOSED;
    }
    // Gestisce gli errori di ricezione
    return NET_ERR_RECV;
}