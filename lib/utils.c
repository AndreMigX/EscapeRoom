#include "utils.h"

/*
 * Funzione per verificare se una stringa contiene solo caratteri numerici
 */
bool is_number(const char* str) 
{
    int i, len = strlen(str);

    // Scorre ogni carattere nella stringa
    for (i = 0; i < len; i++) {
        // Se il carattere non è un numero, restituisce false
        if (str[i] < '0' || str[i] > '9')
            return false;
    }

    // Tutti i caratteri sono numerici, restituisce true
    return true;
}

/*
 * Funzione per convertire una stringa in un valore di tipo long intero
 */
long string_to_long(const char* str) 
{
    char* end;

    // Utilizza la funzione strtol della libreria standard C per la conversione
    return strtol(str, &end, 10);
}

/*
 * Estrae una sottostringa dalla stringa di origine e la copia nella stringa di destinazione
 */
void substring(const char *source, int start, int length, char *destination) 
{
    strncpy(destination, source + start, length);
    destination[length] = '\0'; // Aggiungi il terminatore di stringa
}

/*
 * Funzione per ottenere un numero di porta da una stringa
 */
bool get_port(const char* str, int* port)
{
    int len = strlen(str);
    long p;

    // Verifica se la lunghezza della stringa è accettabile e se contiene solo numeri
    if (len > 5 || !is_number(str))
        return false;
    
    // Converte la stringa in un valore di tipo long intero
    p = string_to_long(str);

    // Verifica se il numero di porta è nel range valido (0 - 65535)
    if (p > 65535)
        return false;

    // Assegna il valore del numero di porta attraverso il puntatore
    *port = (int)p;
    
    return true;
}

/* 
 * Legge una riga da stdin con una lunghezza massima di max_size caratteri, incluso il terminatore,
 * e memorizza la stringa risultante nel buffer specificato
 */
void read_line(char* buffer, int max_size)
{
    if (fgets(buffer, max_size, stdin) != NULL) 
    {
        // Rimuovi il carattere di nuova linea alla fine, se presente
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            // Buffer overflow: rimuovi i caratteri in eccesso dal buffer di input
            char c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    }
}

/*
 * Ottiene il timestamp corrente e lo formatta come una stringa leggibile.
 */
char* getStringTimestamp() {
    time_t current_time;
    time(&current_time);

    // Utilizza asctime per formattare il timestamp come stringa leggibile
    char *formatted_time = ctime(&current_time);

    // Rimuovi il carattere di nuova linea da formatted_time, se presente
    size_t length = strlen(formatted_time);
    if (length > 0 && formatted_time[length - 1] == '\n') {
        formatted_time[length - 1] = '\0';
    }

    return formatted_time;
}

time_t getTimestamp() {
    time_t current_time;
    time(&current_time);

    return current_time;
}

/* 
 * Funzione per rimuovere gli spazi bianchi dalla fine di una stringa 
 */
void trimEnd(char *str) {
    int length = strlen(str);

    // Riduci la lunghezza eliminando gli spazi bianchi dalla fine
    while (length > 0 && isspace((unsigned char)str[length - 1])) {
        str[--length] = '\0';
    }
}

/* 
 * Funzione per rimuovere gli spazi bianchi dall'inizio di una stringa
 */
void trimStart(char *str) {
    int start = 0;

    // Trova la posizione del primo carattere non spazio bianco
    while (isspace((unsigned char)str[start])) {
        start++;
    }

    // Sposta la stringa all'inizio
    memmove(str, str + start, strlen(str) - start + 1);
}

/* 
 * Funzione per rimuovere gli spazi bianchi da entrambi i lati di una stringa
 */
void trim(char *str) {
    trimStart(str);
    trimEnd(str);
}

/* 
 * Funzione che visualizza un messaggio e attende l'input dell'utente
 */
void pressEnterToContinue()
{
    printf("\nPremi invio per continuare...");
    fflush(stdout);
    getchar(); // Attendi l'input dell'utente
}