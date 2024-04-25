#ifndef UTILS
#define UTILS

#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

bool is_number(const char* str);
long string_to_long(const char* str);
void substring(const char *source, int start, int length, char *destination);
bool get_port(const char* str, int* port);

void read_line(char* buffer, int max_size);
void trim(char* str);
void trimEnd(char* str);
void trimStart(char* str);

char* getStringTimestamp();
time_t getTimestamp();

void pressEnterToContinue();

#endif