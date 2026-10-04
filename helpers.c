#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "helpers.h"
#include "ui.h"


void config_defaults(Config *cfg) {
    cfg->strict_mode = 0;
    cfg->randomize_questions = 1;
    cfg->questions_per_quiz = 0;
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
        if (strcmp(key, "randomize_questions") == 0) {
            cfg->randomize_questions = atoi(value);
        }
        if (strcmp(key, "questions_per_quiz") == 0) {
            cfg->questions_per_quiz = atoi(value);
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

    puts("\n1) Quick start\n"
         "2) Add new question\n"
         "3) Remove question\n"
         "4) Show available questions\n"
         "5) Reset stats\n"
         "6) Import questions from CSV\n"
         "7) Export questions to CSV\n");
    printf("> ");
}


int read_input(char *buf, size_t size) {

    buf[0] = '\0';

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


static int ask_non_empty_text(const char *prompt, char *buf, size_t size) {

    printf("%s", prompt);
    if (read_input(buf, size) != 0 || buf[0] == '\0') {
        return -1;
    }

    return 0;
}


void show_added_question_info(Question *q) {

    printf(ERASE_AND_HOME);

    puts(GREEN "\nDone." RESET " You added:");
    printf(BOLD "Category" RESET ": %s\n", q->category);
    printf(BOLD "Question" RESET ": %s\n", q->question);
    printf(BOLD "Answer" RESET ": %s\n", q->answer);
}


static int add_question_steps(sqlite3 *db, const Question *q) {

    sqlite3_stmt *stmt = NULL;
    int result_code;
    sqlite3_int64 category_id;
    sqlite3_int64 question_id;

    result_code = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO categories (category) VALUES (?);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, q->category, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Insert category failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }

    result_code = sqlite3_prepare_v2(db, "SELECT id FROM categories WHERE category = ?;", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, q->category, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    if (result_code != SQLITE_ROW) {
        fprintf(stderr, RED "Category lookup failed: %s\n" RESET, sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    category_id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    result_code = sqlite3_prepare_v2(db, "INSERT INTO questions (category_id, question, answer) VALUES (?, ?, ?);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, category_id);
    sqlite3_bind_text(stmt, 2, q->question, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, q->answer, -1, SQLITE_TRANSIENT);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Insert question failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }

    question_id = sqlite3_last_insert_rowid(db);
    result_code = sqlite3_prepare_v2(db, "INSERT INTO stats (question_id, attempts, correct_attempts) VALUES (?, 0, 0);", -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, question_id);
    result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Insert stats failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}


int add_question_to_db(sqlite3 *db, const Question *q) {

    if (sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Can't start transaction: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    int result_code = add_question_steps(db, q);

    sqlite3_exec(db, result_code == 0 ? "COMMIT;" : "ROLLBACK;", NULL, NULL, NULL);

    return result_code;
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
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }

    int number = 0;
    while ((result_code = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("  %d) %s (%d)\n",
            ++number,
            (const char *)sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2));
    }

    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Reading categories failed: %s\n" RESET, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return (result_code == SQLITE_DONE) ? 0 : -1;

}


static int show_questions(sqlite3 *db, sqlite3_int64 category_id) {

    const char *sql =
            "SELECT c.category, q.question, q.answer, "
            "COALESCE(s.attempts, 0), COALESCE(s.correct_attempts, 0) "
            "FROM questions q "
            "JOIN categories c ON c.id = q.category_id "
            "LEFT JOIN stats s ON s.question_id = q.id "
            "WHERE ?1 = 0 OR c.id = ?1 "
            "ORDER BY c.category COLLATE NOCASE, q.id;";

    sqlite3_stmt *stmt = NULL;
    int result_code = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (result_code != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, category_id);

    int count = 0;
    printf("\n");
    while ((result_code = sqlite3_step(stmt)) == SQLITE_ROW) {

        const char *line_color = (count % 2 == 0) ? CYAN_BG : "";
        printf("%s[%d] (%s) %s -> %s [attempts: %d, correct: %d]\n" RESET,
            line_color,
            count + 1,
            (const char *)sqlite3_column_text(stmt, 0),
            (const char *)sqlite3_column_text(stmt, 1),
            (const char *)sqlite3_column_text(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4));
        count++;
    }

    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Reading questions failed: %s\n" RESET, sqlite3_errmsg(db));
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


static int ask_number(const char *prompt, long *out) {

    char input[MAX_INPUT_SIZE];

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


static sqlite3_int64 lookup_id(sqlite3 *db, const char *sql, sqlite3_int64 p1, sqlite3_int64 p2) {

    sqlite3_stmt *stmt = NULL;
    sqlite3_int64 id = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, p1);
    if (sqlite3_bind_parameter_count(stmt) >= 2) {
        sqlite3_bind_int64(stmt, 2, p2);
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return id;
}


static int count_questions(sqlite3 *db) {

    sqlite3_stmt *stmt = NULL;
    int count = -1;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM questions;", -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}


int choose_category(sqlite3 *db, const char *title, sqlite3_int64 *category_id) {

    long choice;

    int total = count_questions(db);
    if (total < 0) {
        return -1;
    }
    if (total == 0) {
        printf("\nNo available questions. Add some with option 2.\n");
        return -1;
    }

    printf("\n%s\n", title);
    printf("  0) ALL categories (%d)\n", total);
    if (print_categories(db) != 0) {
        return -1;
    }

    if (ask_number(BOLD "\nYour choice: " RESET, &choice) != 0 || choice < 0) {
        printf("Please enter a valid number.\n");
        return -1;
    }

    *category_id = 0;
    if (choice > 0) {
        sqlite3_int64 id = lookup_id(db,
            "SELECT id FROM categories ORDER BY category COLLATE NOCASE, id "
            "LIMIT 1 OFFSET ?1;",
            choice - 1, 0);
        if (id < 0) {
            printf("No such category.\n");
            return -1;
        }
        *category_id = id;
    }

    return 0;
}


static int get_category_name(sqlite3 *db, long position, char *buf, size_t size) {

    sqlite3_stmt *stmt = NULL;
    int found = -1;

    if (sqlite3_prepare_v2(db,
        "SELECT category FROM categories "
        "ORDER BY category COLLATE NOCASE, id "
        "LIMIT 1 OFFSET ?1;",
        -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
            return -1;
        }

    sqlite3_bind_int64(stmt, 1, position - 1);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        snprintf(buf, size, "%s", (const char *)sqlite3_column_text(stmt, 0));
        found = 0;
    }

    sqlite3_finalize(stmt);
    return found;
}


int add_new_question(sqlite3 *db, Question *q) {

    long choice;

    memset(q, 0, sizeof *q);

    printf(ERASE_AND_HOME);

    printf(BOLD "\nChoose category:\n" RESET);
    printf("  0) New category\n");
    if (print_categories(db) != 0) {
        return -1;
    }

    if (ask_number(BOLD "\nYour choice: " RESET, &choice) != 0 || choice < 0) {
        return -1;
    }

    if (choice == 0) {
        if (ask_non_empty_text(BOLD "\nNew category name: " RESET, q->category, sizeof q->category) != 0) {
            return -1;
        }
    } else {
        if (get_category_name(db, choice, q->category, sizeof q->category) != 0) {
            puts("No such category.");
            return -1;
        }
        printf(BOLD "\nCategory: %s\n" RESET, q->category);
    }

//    if(ask_non_empty_text(BOLD "\nCategory" RESET ": ", q->category, sizeof q->category) != 0) return -1;
    if(ask_non_empty_text(BOLD "\nQuestion: " RESET, q->question, sizeof q->question) != 0) return -1;
    if(ask_non_empty_text(BOLD "\nAnswer: " RESET, q->answer, sizeof q->answer) != 0) return -1;

    return 0;
}


void show_questions_menu(sqlite3 *db) {

    sqlite3_int64 category_id;
    if (choose_category(db, ERASE_AND_HOME "Show questions from:", &category_id) != 0) {
        return;
    }

    show_questions(db, category_id);
}


static int run_delete(sqlite3 *db, const char *sql, sqlite3_int64 p1, sqlite3_int64 p2) {

    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, p1);
    if (sqlite3_bind_parameter_count(stmt) >= 2) {
        sqlite3_bind_int64(stmt, 2, p2);
    }

    int result_code = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Delete failed: %s\n" RESET, sqlite3_errmsg(db));
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

    long category_pos;
    long choice;

    printf(ERASE_AND_HOME);

    printf("\nRemove from category:\n");
    if (print_categories(db) != 0) {
        return;
    }

    if (ask_number(BOLD "\nCategory" RESET ": ", &category_pos) != 0 || category_pos <= 0) {
        printf("Cancelled.\n");
        return;
    }

    sqlite3_int64 category_id = lookup_id(db,
        "SELECT id FROM categories ORDER BY category COLLATE NOCASE, id "
        "LIMIT 1 OFFSET ?1;",
        category_pos - 1, 0);
    if (category_id < 0) {
        printf("No such category.\n");
        return;
    }

    printf("\n");
    show_questions(db, category_id);

    if (ask_number(BOLD "\nQuestion number to remove (0 = remove the WHOLE category)" RESET ": ",
        &choice) != 0 || choice < 0) {
            printf("Cancelled.\n");
            return;
    }

    if (choice == 0) {
        char answer[16];
        printf(RED_BG "This deletes the category, ALL its questions and their stats.\n" RESET);
        printf("Type 'yes' to confirm: ");
        read_input(answer, sizeof answer);

        if (strcmp(answer, "yes") != 0) {
            printf("Cancelled.\n");
            return;
        }

        int changed = remove_category(db, category_id);
        if (changed < 0)        printf(RED "Removing failed.\n" RESET);
        else if (changed == 0)  printf("No such category.\n");
        else                    printf(GREEN "Category removed.\n" RESET);
    } else {
        sqlite3_int64 question_id = lookup_id(db,
            "SELECT id FROM questions WHERE category_id = ?1 "
            "ORDER BY id LIMIT 1 OFFSET ?2;",
            category_id, choice - 1);
        if (question_id < 0) {
            printf("No question with that number.\n");
            return;
        }

        int changed = remove_question(db, question_id, category_id);
        if (changed < 0)        printf(RED "Removing failed.\n" RESET);
        else if (changed == 0)  printf("Nothing was removed.\n");
        else                    printf(GREEN "Question removed.\n" RESET);
    }
}


void reset_stats_menu(sqlite3 *db) {

    char answer[16];

    printf(RED "\nThis resets all stats for all questions. Type 'yes' to confirm.\n" RESET "> ");
    read_input(answer, sizeof answer);

    if (strcmp(answer, "yes") != 0) {
        printf("Cancelled.\n");
        return;
    }

    char *err = NULL;
    if (sqlite3_exec(db,
        "UPDATE stats SET attempts = 0, correct_attempts = 0;",
        NULL, NULL, &err) != SQLITE_OK) {
            fprintf(stderr, RED "Resetting stats failed: %s\n" RESET, err);
            sqlite3_free(err);
            return;
        }

    printf(GREEN "Stats reset for %d question(s).\n" RESET, sqlite3_changes(db));
}


typedef enum {
    ANSWER_CORRECT,
    ANSWER_NOT_CORRECT,
    ANSWER_QUIT
} AnswerResult;


static void update_stat(sqlite3 *db, const char *sql, sqlite3_int64 question_id) {

    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
        return;
    }

    sqlite3_bind_int64(stmt, 1, question_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, RED "Updating stats failed: %s\n" RESET, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}


static AnswerResult ask_one_question(sqlite3 *db, const Config *cfg, int number,
                                    sqlite3_int64 question_id,
                                    const char *question, const char *answer) {

    char input[MAX_INPUT_SIZE];

    printf(CYAN "\n\n\nQuestion %d:" RESET " %s\n", number, question);

    update_stat(db,
        "UPDATE stats SET attempts = attempts + 1 WHERE question_id = ?1;",
        question_id);

    printf("> ");

    if (read_input(input, sizeof input) == -1 || strcmp(input, "/q") == 0) {
        return ANSWER_QUIT;
    }

    if (!cfg->strict_mode) {
        printf(CYAN "Answer:" RESET " %s\n", answer);
        return ANSWER_NOT_CORRECT;
    }

    if (strcmp(input, answer) == 0) {
        puts(GREEN "Correct!" RESET);
        update_stat(db,
            "UPDATE stats SET correct_attempts = correct_attempts + 1 WHERE question_id = ?1;",
            question_id);
        return ANSWER_CORRECT;
    }

    printf(RED "Incorrect." RESET "Correct answer: %s\n", answer);
    return ANSWER_NOT_CORRECT;
}


static void print_quiz_summary(int asked, int correct, const Config *cfg, int quit_early) {

    if (quit_early) {
        puts(ERASE_AND_HOME "\n=== Quiz stopped ===");
    } else {
        puts(ERASE_AND_HOME GREEN_BG BOLD "\n=== Congratulations, you finished the quiz! ===" RESET);
    }

    printf("Questons answered: %d\n", asked);

    if (cfg->strict_mode) {
        printf("Correct answers: %d\n", correct);
    }
}


void run_quiz(sqlite3 *db, const Config *cfg) {

    sqlite3_int64 category_id;
    if (choose_category(db, ERASE_AND_HOME "Quiz from:", &category_id) != 0) {
        return;
    }

    int per_quiz = cfg->questions_per_quiz;
    if (per_quiz < -1) {
        puts("Invalid questions_per_quiz in settings. Using the whole category.");
        per_quiz = 0;
    }
    int infinite = (per_quiz == -1);
    int limit = (per_quiz > 0) ? per_quiz : -1;

    const char *order = cfg->randomize_questions ? "RANDOM()" : "q.category_id, q.id";
    char sql[256];
    snprintf(sql, sizeof sql,
        "SELECT q.id, q.question, q.answer "
        "FROM questions q "
        "WHERE ?1 = 0 OR q.category_id = ?1 "
        "ORDER BY %s "
        "LIMIT ?2;", order);

    puts(ERASE_AND_HOME "\nType /q to quit the quiz.");

    int asked = 0;
    int correct = 0;
    int quit = 0;

    do {

        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
            return;
        }
        sqlite3_bind_int64(stmt, 1, category_id);
        sqlite3_bind_int(stmt, 2, limit);

        int shown_in_pass = 0;

        while (!quit && sqlite3_step(stmt) == SQLITE_ROW) {

            AnswerResult result = ask_one_question(db, cfg, asked + 1,
                sqlite3_column_int64(stmt, 0),
                (const char *)sqlite3_column_text(stmt, 1),
                (const char *)sqlite3_column_text(stmt, 2));

            shown_in_pass++;

            if (result == ANSWER_QUIT) {
                quit = 1;
            } else {
                asked++;
                if (result == ANSWER_CORRECT) {
                    correct++;
                }
            }

        }

        sqlite3_finalize(stmt);

        if (shown_in_pass == 0) {
            break;
        }

    } while (infinite && !quit);

    print_quiz_summary(asked, correct, cfg, quit);
}

// CSV export

static void write_csv_field(FILE *f, const char *text) {

    fputc('"', f);

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '"') {
            fputc('"', f);
        }
        fputc(*p, f);
    }

    fputc('"', f);
}


static int export_questions(sqlite3 *db, const char *path) {

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, RED "Can't open %s for writing.\n" RESET, path);
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT c.category, q.question, q.answer "
        "FROM questions q "
        "JOIN categories c ON c.id = q.category_id "
        "ORDER BY c.category COLLATE NOCASE, q.id;",
        -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, RED "Prepare failed: %s\n", sqlite3_errmsg(db));
            fclose(f);
            return -1;
        }

    fputs("category, question, answer\n", f);

    int count = 0;
    int result_code;
    while ((result_code = sqlite3_step(stmt)) == SQLITE_ROW) {
        write_csv_field(f, (const char *)sqlite3_column_text(stmt, 0));
        fputc(',', f);
        write_csv_field(f, (const char *)sqlite3_column_text(stmt, 1));
        fputc(',', f);
        write_csv_field(f, (const char *)sqlite3_column_text(stmt, 2));
        fputc('\n', f);
        count++;
    }
    sqlite3_finalize(stmt);

    if (result_code != SQLITE_DONE) {
        fprintf(stderr, RED "Reading questions failed.\n" RESET);
        fclose(f);
        return -1;
    }

    if (fclose(f) != 0) {
        fprintf(stderr, RED "Writing the file failed.\n" RESET);
        return -1;
    }

    return count;
}


void export_csv_menu(sqlite3 *db) {

    char path[MAX_INPUT_SIZE];
    char answer[16];

    printf(ERASE_AND_HOME);

    if (ask_non_empty_text(BOLD "\nSave as (file name): " RESET, path, sizeof path) != 0) {
        puts("Cancelled.");
        return;
    }

    FILE *test = fopen(path, "r");
    if (test) {
        fclose(test);
        printf(RED "%s already exists and will be overwritten.\n" RESET
            "Type 'yes' to confirm: ", path);
        read_input(answer, sizeof answer);
        if (strcmp(answer, "yes") != 0) {
            puts("Cancelled.");
            return;
        }
    }

    int count = export_questions(db, path);
    if (count < 0) {
        puts(RED "Export failed." RESET);
    } else {
        printf(GREEN "Exported %d question(s) to %s\n" RESET, count, path);
    }
}


typedef enum {
    CSV_RECORD,
    CSV_END,
    CSV_BAD
} CsvResult;


static CsvResult read_csv_record(FILE *f, Question *q) {

    memset(q, 0, sizeof *q);

    char *fields[3] = { q->category, q->question, q->answer };

    int field = 0;
    size_t len = 0;
    int in_quotes = 0;
    int seen_anything = 0;
    int too_long = 0;
    int c;

    while ((c = fgetc(f)) != EOF) {

        int to_add = -1;

        if (in_quotes) {
            if (c == '"') {
                int next = fgetc(f);
                if (next == '"') {
                    to_add = '"';
                } else {
                    in_quotes = 0;
                    ungetc(next, f);
                }
            } else {
                to_add = c;
            }
        } else if (c == '"') {
            if (len != 0) {
                return CSV_BAD;
            }
            in_quotes = 1;
            seen_anything = 1;
        } else if (c == ',') {
            field++;
            if (field > 2) {
                return CSV_BAD;
            }
            len = 0;
            seen_anything = 1;
        } else if (c == '\r') {
            // Windows line ending is \r\n: ignore the \r
        } else if (c == '\n') {
            if (!seen_anything) {
                continue;
            }
            break;
        } else {
            to_add = c;
            seen_anything = 1;
        }

        if (to_add != -1) {
            if (len < MAX_INPUT_SIZE - 1) {
                fields[field][len++] = (char)to_add;
            } else {
                too_long = 1;
            }
        }

    }

    if (in_quotes) return CSV_BAD;
    if (!seen_anything) return CSV_END;
    if (field != 2 || too_long) return CSV_BAD;

    return CSV_RECORD;
}


static int question_exists(sqlite3 *db, const Question *q) {

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT 1 FROM questions q "
        "JOIN categories c ON c.id = q.category_id "
        "WHERE c.category = ?1 AND q.question = ?2 LIMIT 1;",
        -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, RED "Prepare failed: %s\n" RESET, sqlite3_errmsg(db));
            return -1;
        }

    sqlite3_bind_text(stmt, 1, q->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, q->question, -1, SQLITE_TRANSIENT);

    int result_code = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result_code == SQLITE_ROW) return 1;
    if (result_code == SQLITE_DONE) return 0;

    return -1;
}


static int import_questions(sqlite3 *db, const char *path, int *added, int *skipped) {

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, RED "Can't open %s\n" RESET, path);
        return -1;
    }

    if (!(fgetc(f) == 0xEF && fgetc(f) == 0xBB && fgetc(f) == 0xBF)) {
        rewind(f);
    }

    if (sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, RED "Can't start transaction: %s\n" RESET, sqlite3_errmsg(db));
        fclose(f);
        return -1;
    }

    Question q;
    CsvResult status;
    int record = 0;
    int result = 0;

    while ((status = read_csv_record(f, &q)) == CSV_RECORD) {

        record++;

        if (record == 1 &&
            strcmp(q.category, "category") == 0 &&
            strcmp(q.question, "question") == 0 &&
            strcmp(q.answer, "answer") == 0) {
                continue;
            }

        if (q.category[0] == '\0' || q.question[0] == '\0' || q.answer[0] == '\0') {
            fprintf(stderr, RED "Record %d has an empty value.\n" RESET, record);
            result = -1;
            break;
        }

        int exists = question_exists(db, &q);
        if (exists < 0) {
            result = -1;
            break;
        }
        if (exists == 1) {
            (*skipped)++;
            continue;
        }

        if (add_question_steps(db, &q) != 0) {
            result = -1;
            break;
        }
        (*added)++;

    }

    if (result == 0 && status == CSV_BAD) {
        fprintf(stderr, RED "Record %d is malformed "
            "(broken quotes, wrong number of columns or value too long).\n" RESET, record + 1);
        result = -1;
    }

    sqlite3_exec(db, result == 0 ? "COMMIT;" : "ROLLBACK;", NULL, NULL, NULL);
    fclose(f);

    return result;
}


void import_csv_menu(sqlite3 *db) {

    char path[MAX_INPUT_SIZE];

    printf(ERASE_AND_HOME);

    if (ask_non_empty_text(BOLD "\nCSV file to import: " RESET, path, sizeof path) != 0) {
        puts("Cancelled.");
        return;
    }

    int added = 0;
    int skipped = 0;

    if (import_questions(db, path, &added, &skipped) == 0) {
        printf(GREEN "\nImport finished. " RESET "Added: %d, skipped dubplicates: %d\n",
            added, skipped);
    } else {
        puts(RED "\nImport failed. Nothing was added" RESET);
    }
}
