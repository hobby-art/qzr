#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "helpers.h"
#include "ui.h"


// gcc -Wall -Wextra -g -fsanitize=address,undefined main.c helpers.c -lsqlite3 -o qzr
// remove flags on final build


int main(void) {

    Config cfg;

    sqlite3 *db;
    if (init_db(&db) != 0) {
        return 1;
    }

    char input[MAX_INPUT_SIZE]; // input size for quiz answers and menu options

    puts(GREEN_BG "\n           === Hello! Choose a command or type 'q' to exit. ===" RESET);

    while (1) {

        show_main_menu();

        int status = read_input(input, sizeof(input));
        if (status == -1) {
            puts("No input. Try again.");
            break;
        } else if (status == 1) {
            puts("Reply is too long. Try again.");
            continue;
        }

        if (strcmp(input, "q") == 0) {
            break;
        }

        int menu_choice = atoi(input);

        switch (menu_choice) {

           case 1:

               config_defaults(&cfg);
               config_load(&cfg, "settings.conf");
               run_quiz(db, &cfg);
               continue;

           case 2: {

               Question q;
               if (add_new_question(&q) != 0) {
                   puts("Cancelled: fields can't be empty or too long.");
                   continue;
               }
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

           case 5:

               reset_stats_menu(db);
               continue;

           default:

               puts("Invalid option. Try again.");
               continue;
        }

    }

   sqlite3_close(db);

}
