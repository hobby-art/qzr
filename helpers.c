#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "helpers.h"


int init_db(sqlite3 **db) {

    if (sqlite3_open("qzr.db", db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        return 1;
    }

    const char *schema =
            "PRAGMA foreign_keys = ON;"

            "CREATE TABLE IF NOT EXISTS categories ("
            "  id   INTEGER PRIMARY KEY,"
            "  name TEXT NOT NULL UNIQUE"
            ");"

            "CREATE TABLE IF NOT EXISTS questions ("
            "  id          INTEGER PRIMARY KEY,"
            "  category_id INTEGER NOT NULL REFERENCES categories(id),"
            "  question    TEXT NOT NULL,"
            "  answer      TEXT NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS attempts ("
            "  id               INTEGER PRIMARY KEY,"
            "  question_id      INTEGER NOT NULL REFERENCES questions(id),"
            "  attempts         INTEGER NOT NULL,"
            "  correct_attempts INTEGER NOT NULL"
            ");";

    char *err = NULL;
    if (sqlite3_exec(*db, schema, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "Schema error: %s\n", err);
        sqlite3_free(err);
        return 1;
    }

    return 0;
}


void show_main_menu(void) {
    puts("1. Quick start\n"
         "2. Add new question\n"
         "3. Remove question");
    printf("> ");
}


int read_input(char *buf, size_t size) {

    if (fgets(buf, size, stdin) == NULL) {
        return -1;
    }

    char *newline = strchr(buf, '\n');
    if (newline != NULL) {
        *newline = '\0';
        return 0;
    }

    int c;
    int too_long = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        too_long = 1;
    }

    return too_long;
}
