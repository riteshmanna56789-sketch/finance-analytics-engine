#include "storage.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *STORAGE_HEADER = "FAE_STORAGE 1";
static const size_t MAX_LINE_LENGTH = 4096;

static int write_line(FILE *file, const char *text)
{
    return fputs(text, file) >= 0 && fputc('\n', file) != EOF;
}

static int read_line(FILE *file, char *buffer, size_t buffer_size)
{
    size_t length;
    int character;

    if (fgets(buffer, (int)buffer_size, file) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
        if (length > 1 && buffer[length - 2] == '\r') {
            buffer[length - 2] = '\0';
        }
        return 1;
    }

    if (length == buffer_size - 1) {
        while ((character = fgetc(file)) != '\n' && character != EOF) {
        }
        return -1;
    }

    return 1;
}

static int parse_unsigned(
    const char *text,
    size_t *value
)
{
    char *end;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (text == end || errno == ERANGE || *end != '\0'
        || parsed > SIZE_MAX) {
        return 0;
    }

    *value = (size_t)parsed;
    return 1;
}

static int parse_int(const char *text, int *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (text == end || errno == ERANGE
        || parsed < INT_MIN || parsed > INT_MAX || *end != '\0') {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int parse_int64(const char *text, int64_t *value)
{
    char *end;
    intmax_t parsed;

    errno = 0;
    parsed = strtoimax(text, &end, 10);
    if (text == end || errno == ERANGE || *end != '\0'
        || parsed < INT64_MIN || parsed > INT64_MAX) {
        return 0;
    }

    *value = (int64_t)parsed;
    return 1;
}

static int parse_record_header(
    const char *line,
    const char *prefix,
    size_t *field_count,
    char **fields
)
{
    char buffer[MAX_LINE_LENGTH];
    char *cursor;
    size_t index = 0;

    if (strncmp(line, prefix, strlen(prefix)) != 0
        || line[strlen(prefix)] != ' ') {
        return 0;
    }

    if (strlen(line) >= sizeof(buffer)) {
        return 0;
    }

    strcpy(buffer, line + strlen(prefix) + 1);
    cursor = strtok(buffer, " ");
    while (cursor != NULL && index < *field_count) {
        fields[index++] = cursor;
        cursor = strtok(NULL, " ");
    }

    return index == *field_count && cursor == NULL;
}

static StorageResult load_category(
    FILE *file,
    CategoryList *categories
)
{
    char line[MAX_LINE_LENGTH];
    char *fields[3];
    size_t field_count = 3;
    size_t name_length;
    int id;
    int is_active;
    Category category;
    int result;

    result = read_line(file, line, sizeof(line));
    if (result != 1
        || !parse_record_header(line, "CATEGORY", &field_count, fields)
        || !parse_int(fields[0], &id)
        || !parse_int(fields[1], &is_active)
        || (is_active != 0 && is_active != 1)
        || !parse_unsigned(fields[2], &name_length)
        || name_length >= sizeof(category.name)
        || id < 1) {
        return STORAGE_INVALID_DATA;
    }

    result = read_line(file, category.name, sizeof(category.name));
    if (result != 1 || strlen(category.name) != name_length) {
        return STORAGE_INVALID_DATA;
    }

    for (size_t index = 0; index < categories->size; index++) {
        if (categories->items[index].id == id
            || strcmp(categories->items[index].name, category.name) == 0) {
            return STORAGE_INVALID_DATA;
        }
    }

    if (name_length == 0) {
        return STORAGE_INVALID_DATA;
    }
    for (size_t index = 0; index < name_length; index++) {
        if (!isspace((unsigned char)category.name[index])) {
            break;
        }
        if (index + 1 == name_length) {
            return STORAGE_INVALID_DATA;
        }
    }

    category.id = id;
    category.is_active = is_active;
    if (category_list_add(categories, category) != CATEGORY_SUCCESS) {
        return STORAGE_MEMORY_ERROR;
    }

    return STORAGE_SUCCESS;
}

static int timestamp_is_valid(const Timestamp *timestamp)
{
    return timestamp->year >= 1
        && timestamp->month >= 1 && timestamp->month <= 12
        && timestamp->day >= 1 && timestamp->day <= 31
        && timestamp->hour >= 0 && timestamp->hour <= 23
        && timestamp->minute >= 0 && timestamp->minute <= 59
        && timestamp->second >= 0 && timestamp->second <= 60;
}

static StorageResult load_expense(
    FILE *file,
    const CategoryList *categories,
    ExpenseList *expenses
)
{
    char line[MAX_LINE_LENGTH];
    char *fields[10];
    size_t field_count = 10;
    size_t note_length;
    Expense expense;
    int result;

    result = read_line(file, line, sizeof(line));
    if (result != 1
        || !parse_record_header(line, "EXPENSE", &field_count, fields)
        || !parse_int(fields[0], &expense.id)
        || !parse_int(fields[1], &expense.timestamp.year)
        || !parse_int(fields[2], &expense.timestamp.month)
        || !parse_int(fields[3], &expense.timestamp.day)
        || !parse_int(fields[4], &expense.timestamp.hour)
        || !parse_int(fields[5], &expense.timestamp.minute)
        || !parse_int(fields[6], &expense.timestamp.second)
        || !parse_int64(fields[7], &expense.amount_paise)
        || !parse_int(fields[8], &expense.category_id)
        || !parse_unsigned(fields[9], &note_length)
        || note_length >= sizeof(expense.note)
        || !timestamp_is_valid(&expense.timestamp)
        || expense.id < 1
        || expense.amount_paise <= 0
        || category_find_by_id(categories, expense.category_id) == NULL) {
        return STORAGE_INVALID_DATA;
    }

    result = read_line(file, expense.note, sizeof(expense.note));
    if (result != 1 || strlen(expense.note) != note_length) {
        return STORAGE_INVALID_DATA;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        if (expenses->items[index].id == expense.id) {
            return STORAGE_INVALID_DATA;
        }
    }

    if (expense_list_add(expenses, expense) != EXPENSE_SUCCESS) {
        return STORAGE_MEMORY_ERROR;
    }

    return STORAGE_SUCCESS;
}

StorageResult storage_save(
    const char *filename,
    const CategoryList *categories,
    const ExpenseList *expenses
)
{
    char temporary_filename[4096];
    FILE *file;
    StorageResult result = STORAGE_SUCCESS;

    if (filename == NULL || categories == NULL || expenses == NULL) {
        return STORAGE_INVALID_DATA;
    }

    if (snprintf(
            temporary_filename,
            sizeof(temporary_filename),
            "%s.tmp",
            filename
        ) < 0
        || strlen(filename) + 4 >= sizeof(temporary_filename)) {
        return STORAGE_FILE_ERROR;
    }

    file = fopen(temporary_filename, "w");
    if (file == NULL) {
        return STORAGE_FILE_ERROR;
    }

    if (!write_line(file, STORAGE_HEADER)
        || fprintf(file, "CATEGORIES %zu\n", categories->size) < 0) {
        result = STORAGE_FILE_ERROR;
    }

    for (size_t index = 0; result == STORAGE_SUCCESS
        && index < categories->size; index++) {
        const Category *category = &categories->items[index];

        if (fprintf(file, "CATEGORY %d %d %zu\n",
                category->id,
                category->is_active,
                strlen(category->name)) < 0
            || write_line(file, category->name) == 0) {
            result = STORAGE_FILE_ERROR;
        }
    }

    if (result == STORAGE_SUCCESS
        && fprintf(file, "EXPENSES %zu\n", expenses->size) < 0) {
        result = STORAGE_FILE_ERROR;
    }

    for (size_t index = 0; result == STORAGE_SUCCESS
        && index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];

        if (fprintf(file, "EXPENSE %d %d %d %d %d %d %d %" PRId64
                " %d %zu\n",
                expense->id,
                expense->timestamp.year,
                expense->timestamp.month,
                expense->timestamp.day,
                expense->timestamp.hour,
                expense->timestamp.minute,
                expense->timestamp.second,
                expense->amount_paise,
                expense->category_id,
                strlen(expense->note)) < 0) {
            result = STORAGE_FILE_ERROR;
            break;
        }

        if (fputs(expense->note, file) == EOF || fputc('\n', file) == EOF) {
            result = STORAGE_FILE_ERROR;
        }
    }

    if (fclose(file) != 0 && result == STORAGE_SUCCESS) {
        result = STORAGE_FILE_ERROR;
    }

    if (result == STORAGE_SUCCESS) {
        if (rename(temporary_filename, filename) != 0) {
            if (remove(filename) != 0 && errno != ENOENT) {
                result = STORAGE_FILE_ERROR;
            } else if (rename(temporary_filename, filename) != 0) {
                result = STORAGE_FILE_ERROR;
            }
        }
    }

    if (result != STORAGE_SUCCESS) {
        remove(temporary_filename);
    }

    return result;
}

StorageResult storage_load(
    const char *filename,
    CategoryList *categories,
    ExpenseList *expenses
)
{
    FILE *file;
    char line[MAX_LINE_LENGTH];
    char *fields[1];
    size_t field_count = 1;
    size_t category_count;
    size_t expense_count;
    CategoryList loaded_categories;
    ExpenseList loaded_expenses;
    StorageResult result = STORAGE_SUCCESS;

    if (filename == NULL || categories == NULL || expenses == NULL) {
        return STORAGE_INVALID_DATA;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return errno == ENOENT ? STORAGE_NOT_FOUND : STORAGE_FILE_ERROR;
    }

    category_list_init(&loaded_categories);
    expense_list_init(&loaded_expenses);

    if (read_line(file, line, sizeof(line)) != 1
        || strcmp(line, STORAGE_HEADER) != 0) {
        result = STORAGE_INVALID_DATA;
    }

    if (result == STORAGE_SUCCESS
        && (read_line(file, line, sizeof(line)) != 1
            || !parse_record_header(line, "CATEGORIES", &field_count, fields)
            || !parse_unsigned(fields[0], &category_count))) {
        result = STORAGE_INVALID_DATA;
    }

    for (size_t index = 0; result == STORAGE_SUCCESS
        && index < category_count; index++) {
        result = load_category(file, &loaded_categories);
    }

    field_count = 1;
    if (result == STORAGE_SUCCESS
        && (read_line(file, line, sizeof(line)) != 1
            || !parse_record_header(line, "EXPENSES", &field_count, fields)
            || !parse_unsigned(fields[0], &expense_count))) {
        result = STORAGE_INVALID_DATA;
    }

    for (size_t index = 0; result == STORAGE_SUCCESS
        && index < expense_count; index++) {
        result = load_expense(file, &loaded_categories, &loaded_expenses);
    }

    if (result == STORAGE_SUCCESS
        && read_line(file, line, sizeof(line)) != 0) {
        result = STORAGE_INVALID_DATA;
    }

    if (fclose(file) != 0 && result == STORAGE_SUCCESS) {
        result = STORAGE_FILE_ERROR;
    }

    if (result == STORAGE_SUCCESS) {
        category_list_destroy(categories);
        expense_list_destroy(expenses);
        *categories = loaded_categories;
        *expenses = loaded_expenses;
    } else {
        category_list_destroy(&loaded_categories);
        expense_list_destroy(&loaded_expenses);
    }

    return result;
}
