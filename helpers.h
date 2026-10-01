#ifndef HELPERS_H
#define HELPERS_H


#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>


// terminal controls
#define CLEAR "\033[2J\033[H"

// text colors
#define RESET "\033[0m"
#define GREEN "\033[32m"

// background colors


int init_db(sqlite3 **db);
void show_main_menu(void);
int read_input(char *buf, size_t size);

#endif
