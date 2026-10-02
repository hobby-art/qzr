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


typedef struct {
    int strict_mode;
} Config;

#define MAX_INPUT_SIZE 2048
typedef struct {
    char question[MAX_INPUT_SIZE];
    char answer[MAX_INPUT_SIZE];
    char category[MAX_INPUT_SIZE];
} Question;


// initialization
void config_load(Config *cfg, const char *path);
void config_defaults(Config *cfg);
int init_db(sqlite3 **db);
// main menu
void show_main_menu(void);
Question add_new_question(void);
void show_added_question_info(Question *q);
int add_question_to_db(sqlite3 *db, const Question *q);
void show_questions_menu(sqlite3 *db);
void remove_questions_menu(sqlite3 *db);
// misc
int read_input(char *buf, size_t size);

#endif
