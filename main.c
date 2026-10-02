#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "helpers.h"


// gcc -Wall -Wextra main.c helpers.c -lsqlite3 -o qzr


int main(void) {

    Config cfg;
    config_defaults(&cfg);
    config_load(&cfg, "settings.conf");

    sqlite3 *db;
    if (init_db(&db) != 0) {
        return 1;
    }

    char input[2048]; // input size for quiz answers and menu options

    puts(GREEN "=== Hello! Choose a command or type 'q' to exit. ===" RESET);

    while (1) {

        show_main_menu();

        int status = read_input(input, sizeof(input));
        if (status == -1) {
            puts("No input. Try again.");
            continue;
        } else if (status == 1) {
            puts("Reply is too long. Try again.");
            continue;
        }

        if (input[0] == 'q') {
            return 0;
        }

        int menu_choice = atoi(input);

        switch (menu_choice) {
           case 1:
               puts("Option 1. Work in progress.");
               continue;
           case 2: {
               Question q = add_new_question();
               if (add_question_to_db(db, &q) == 0) {
                   show_added_question_info(&q);
               }
               continue;
           }
           case 3:
               remove_questions_menu(db);
               continue;
           case 4:
               show_questions_menu(db);
               continue;
           default:
               puts("Invalid option. Try again.");
               continue;
        }

    }

   sqlite3_close(db);

}
