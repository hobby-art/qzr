#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "helpers.h"


void config_defaults(Config *cfg) {
    cfg->strict_mode = 0;
}


void config_load(Config *cfg, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return;

    char line[512];

    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *value = eq + 1;

        if (strcmp(key, "strict_mode") == 0) {
            cfg->strict_mode = atoi(value);
        }
    }
    fclose(file);
}


int init_db(sqlite3 **db) {

    if (sqlite3_open("qzr.db", db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        return 1;
    }

    const char *schema =
            "PRAGMA foreign_keys = ON;"

            "CREATE TABLE IF NOT EXISTS categories ("
            "  id       INTEGER PRIMARY KEY,"
            "  category TEXT NOT NULL UNIQUE"
            ");"

            "CREATE TABLE IF NOT EXISTS questions ("
            "  id          INTEGER PRIMARY KEY,"
            "  category_id INTEGER NOT NULL REFERENCES categories(id),"
            "  question    TEXT NOT NULL,"
            "  answer      TEXT NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS stats ("
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
    puts("\n1. Quick start\n"
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


Question add_new_question(void) {
    Question q = {0};

    puts("\nCategory:");
    read_input(q.category, sizeof q.category);

    puts("Question:");
    read_input(q.question, sizeof q.question);

    puts("Answer:");
    read_input(q.answer, sizeof q.answer);

    return q;
}


void show_added_question_info(Question *q) {
    puts("\nDone. You added:");
    printf("Category: %s\n", q->category);
    printf("Question: %s\n", q->question);
    printf("Answer: %s\n", q->answer);
}


int add_question_to_db(sqlite3 *db, const Question *q) {
    sqlite3_stmt *stmt = NULL;
    int result_code;
    sqlite3_int64 category_id;
    sqlite3_int64 question_id;

    result_code = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO categories (category) VALUES (?);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, q->category, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Insert category failed: %s\n", sqlite3_errmsg(db));
    }

    result_code = sqlite3_prepare_v2(db, "SELECT id FROM categories WHERE category = ?;", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_bind_text(stmt, 1, q->category, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    if (result_code != SQLITE_ROW) {
        fprintf(stderr, "Category lookup failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    category_id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    result_code = sqlite3_prepare_v2(db, "INSERT INTO questions (category_id, question, answer) VALUES (?, ?, ?);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, category_id);
    sqlite3_bind_text(stmt, 2, q->question, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, q->answer, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Insert question failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    question_id = sqlite3_last_insert_rowid(db);
    result_code = sqlite3_prepare_v2(db, "INSERT INTO stats (question_id, attempts, correct_attempts) VALUES (?, 0, 0);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, question_id);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Insert stats failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}
