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
         "3. Remove question\n"
         "4. Show available questions");
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


static int print_categories(sqlite3 *db) {

    sqlite3_stmt *stmt = NULL;
    int result_code = sqlite3_prepare_v2(db,
                "SELECT c.id, c.category, COUNT(q.id) "
                "FROM categories c "
                "LEFT JOIN questions q ON q.category_id = c.id "
                "GROUP BY c.id "
                "ORDER BY c.category COLLATE NOCASE, c.id;",
                -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    while ((result_code = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("  %lld) %s (%d)\n",
            (long long)sqlite3_column_int64(stmt, 0),
            (const char *)sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2));
    }

    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Reading categories failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return (result_code == SQLITE_DONE) ? 0 : -1;

}


static int show_questions(sqlite3 *db, sqlite3_int64 category_id) {

    const char *sql =
            "SELECT q.id, c.category, q.question, q.answer "
            "FROM questions q "
            "JOIN categories c ON c.id = q.category_id "
            "WHERE ?1 = 0 OR c.id = ?1 "
            "ORDER BY c.category COLLATE NOCASE, q.id;";

    sqlite3_stmt *stmt = NULL;
    int result_code = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, category_id);

    int count = 0;
    while ((result_code = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("[%lld] (%s) %s -> %s\n",
            (long long)sqlite3_column_int64(stmt,0),
            (const char *)sqlite3_column_text(stmt, 1),
            (const char *)sqlite3_column_text(stmt, 2),
            (const char *)sqlite3_column_text(stmt, 3));
        count++;
    }

    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Reading questions failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        return -1;
    }

    if (count == 0) {
        printf("No questions found.\n");
    }

    return 0;

}


void show_questions_menu(sqlite3 *db) {

    char input[64];

    printf("\nShow questions from:\n");
    printf("  0) ALL categories\n");
    if (print_categories(db) != 0) {
        return;
    }

    printf("\nYour choice: ");
    read_input(input, sizeof input);

    char *end;
    long choice = strtol(input, &end, 10);
    if (end == input || *end != '\0') {
        printf("Please enter a number.\n");
        return;
    }

    show_questions(db, choice);

}


static int ask_number(const char *prompt, long *out) {

    char input[2048];

    printf("%s", prompt);
    read_input(input, sizeof input);

    char *end;
    long value = strtol(input, &end, 10);
    if (end == input || *end != '\0') {
        return -1;
    }

    *out = value;
    return 0;
}


static int run_delete(sqlite3 *db, const char *sql, sqlite3_int64 p1, sqlite3_int64 p2) {

    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, p1);
    if (sqlite3_bind_parameter_count(stmt) >= 2) {
        sqlite3_bind_int64(stmt, 2, p2);
    }

    int result_code = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, "Delete failed: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    return (result_code == SQLITE_DONE) ? changes : -1;

}


static int remove_question(sqlite3 *db, sqlite3_int64 question_id, sqlite3_int64 category_id) {

    int changed = -1;

    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

    if (run_delete(db,
        "DELETE FROM stats WHERE question_id IN "
        "(SELECT id FROM questions WHERE id = ?1 AND category_id = ?2);",
        question_id, category_id) >= 0) {
            changed = run_delete(db,
                "DELETE FROM questions WHERE id = ?1 AND category_id = ?2;",
                question_id, category_id);
        }

    if (changed >= 0) {
        if (run_delete(db,
            "DELETE FROM categories WHERE id = ?1 AND NOT EXISTS "
            "(SELECT 1 FROM questions WHERE category_id = ?1);",
            category_id, 0) < 0) {
                changed = -1;
            }
    }

    if (changed >= 0) {
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }

    return changed;

}


static int remove_category(sqlite3 *db, sqlite3_int64 category_id) {

    int changed = -1;

    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

    if (run_delete(db,
            "DELETE FROM stats WHERE question_id IN "
            "(SELECT id FROM questions WHERE category_id = ?1);",
            category_id, 0) >= 0
        && run_delete(db,
            "DELETE FROM questions WHERE category_id = ?1;",
            category_id, 0) >= 0) {

                changed = run_delete(db,
                    "DELETE FROM categories WHERE id = ?1;",
                    category_id, 0);

            }

    if (changed >= 0) {
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    } else {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }

    return changed;

}


void remove_questions_menu(sqlite3 *db) {

    long category_id;
    long choice;

    printf("\nRemove from category:\n");
    if (print_categories(db) != 0) {
        return;
    }

    if (ask_number("Category: ", &category_id) != 0 || category_id <= 0) {
        printf("Cancelled.\n");
        return;
    }

    printf("\n");
    show_questions(db, category_id);

    if (ask_number("Question number to remove (0 = remove the WHOLE category): ",
        &choice) != 0 || choice < 0) {
            printf("Cancelled.\n");
            return;
    }

    if (choice == 0) {
        char answer[16];
        printf("This deletes the category, ALL its questions and their stats.\n");
        printf("Type 'yes' to confirm: ");
        read_input(answer, sizeof answer);

        if (strcmp(answer, "yes") != 0) {
            printf("Cancelled.\n");
            return;
        }

        int changed = remove_category(db, category_id);
        if (changed < 0)        printf("Removing failed.\n");
        else if (changed == 0)  printf("No such category.\n");
        else                    printf("Category removed.\n");
    } else {
        int changed = remove_question(db, choice, category_id);
        if (changed < 0)        printf("Removing failed.\n");
        else if (changed == 0)  printf("No questions with that number in this category.\n");
        else                    printf("Question removed.\n");
    }

}
