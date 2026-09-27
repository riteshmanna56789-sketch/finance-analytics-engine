#include "storage.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Version 2 persists the expense ID high-water mark. Version 1 remains loadable. */
static const char *STORAGE_HEADER = "FAE_STORAGE 2";
static const size_t MAX_LINE_LENGTH = 4096;

static int write_line(FILE *file, const char *text)
{
    return fputs(text, file) >= 0 && fputc('\n', file) != EOF;
}

static int text_has_line_break(const char *text)
{
    while (*text != '\0') {
        if (*text == '\r' || *text == '\n') {
            return 1;
        }
        text++;
    }

    return 0;
}

static int read_line(FILE *file, char *buffer, size_t buffer_size)
{
    size_t length = 0;
    int character;
    int too_long = 0;
    int read_anything = 0;
    int pending_carriage_return = 0;

    if (buffer == NULL || buffer_size < 2) {
        return -1;
    }

    while ((character = fgetc(file)) != EOF && character != '\n') {
        read_anything = 1;
        if (pending_carriage_return) {
            if (length + 1 < buffer_size && !too_long) {
                buffer[length++] = '\r';
            } else {
                too_long = 1;
            }
            pending_carriage_return = 0;
        }

        if (character == '\r') {
            pending_carriage_return = 1;
        } else if (character == '\0' || length + 1 >= buffer_size) {
            too_long = 1;
        } else if (!too_long) {
            buffer[length++] = (char)character;
        }
    }

    if (ferror(file)) {
        return -2;
    }

    if (!read_anything && character == EOF) {
        return 0;
    }

    if (character != '\n' && pending_carriage_return) {
        if (length + 1 < buffer_size && !too_long) {
            buffer[length++] = '\r';
        } else {
            too_long = 1;
        }
    }

    buffer[length] = '\0';
    return too_long ? -1 : 1;
}

static int parse_unsigned(
    const char *text,
    size_t *value
)
{
    char *end;
    unsigned long long parsed;

    if (text == NULL || value == NULL || text[0] < '0' || text[0] > '9') {
        return 0;
    }

    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
    }

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
    {
        CategoryResult add_result = category_list_add(categories, category);
        if (add_result == CATEGORY_MEMORY_ERROR) {
            return STORAGE_MEMORY_ERROR;
        }
        if (add_result != CATEGORY_SUCCESS) {
            return STORAGE_INVALID_DATA;
        }
    }

    return STORAGE_SUCCESS;
}

static int timestamp_is_valid(const Timestamp *timestamp)
{
    return timestamp != NULL
        && expense_date_is_valid(
            timestamp->year,
            timestamp->month,
            timestamp->day
        )
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

    {
        ExpenseResult add_result = expense_list_add(expenses, expense);
        if (add_result == EXPENSE_MEMORY_ERROR) {
            return STORAGE_MEMORY_ERROR;
        }
        if (add_result != EXPENSE_SUCCESS) {
            return STORAGE_INVALID_DATA;
        }
    }

    return STORAGE_SUCCESS;
}

static int category_list_is_valid(const CategoryList *categories)
{
    if (categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)
        || categories->capacity > SIZE_MAX / sizeof(*categories->items)) {
        return 0;
    }

    for (size_t index = 0; index < categories->size; index++) {
        const Category *category = &categories->items[index];

        if (category->id < 1
            || (category->is_active != 0 && category->is_active != 1)
            || memchr(category->name, '\0', sizeof(category->name)) == NULL
            || category->name[0] == '\0'
            || text_has_line_break(category->name)) {
            return 0;
        }
        for (const char *cursor = category->name; *cursor != '\0'; cursor++) {
            if (!isspace((unsigned char)*cursor)) {
                break;
            }
            if (cursor[1] == '\0') {
                return 0;
            }
        }

        for (size_t other_index = 0; other_index < index; other_index++) {
            if (categories->items[other_index].id == category->id
                || strcmp(
                    categories->items[other_index].name,
                    category->name
                ) == 0) {
                return 0;
            }
        }
    }

    return 1;
}

static int expense_list_is_valid(
    const CategoryList *categories,
    const ExpenseList *expenses
)
{
    int highest_id = 0;

    if (expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)
        || expenses->capacity > SIZE_MAX / sizeof(*expenses->items)
        || expenses->next_id < 1) {
        return 0;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        const Expense *expense = &expenses->items[index];

        if (expense->id < 1
            || expense->amount_paise <= 0
            || !timestamp_is_valid(&expense->timestamp)
            || category_find_by_id(categories, expense->category_id) == NULL
            || memchr(expense->note, '\0', sizeof(expense->note)) == NULL
            || text_has_line_break(expense->note)) {
            return 0;
        }

        if (expense->id > highest_id) {
            highest_id = expense->id;
        }

        for (size_t other_index = 0; other_index < index; other_index++) {
            if (expenses->items[other_index].id == expense->id) {
                return 0;
            }
        }
    }

    if (highest_id == INT_MAX) {
        return expenses->next_id == INT_MAX;
    }

    return expenses->next_id > highest_id;
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
    int temporary_filename_length;

    if (filename == NULL || categories == NULL || expenses == NULL
        || !category_list_is_valid(categories)
        || !expense_list_is_valid(categories, expenses)) {
        return STORAGE_INVALID_DATA;
    }

    temporary_filename_length = snprintf(
            temporary_filename,
            sizeof(temporary_filename),
            "%s.tmp",
            filename
        );
    if (temporary_filename_length < 0
        || (size_t)temporary_filename_length >= sizeof(temporary_filename)) {
        return STORAGE_FILE_ERROR;
    }

    file = fopen(temporary_filename, "w");
    if (file == NULL) {
        return STORAGE_FILE_ERROR;
    }

    if (!write_line(file, STORAGE_HEADER)
        || fprintf(file, "NEXT_EXPENSE_ID %d\n", expenses->next_id) < 0
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
#ifdef _WIN32
        if (!MoveFileExA(
                temporary_filename,
                filename,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
            )) {
            result = STORAGE_FILE_ERROR;
        }
#else
        if (rename(temporary_filename, filename) != 0) {
            result = STORAGE_FILE_ERROR;
        }
#endif
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
    size_t category_count = 0;
    size_t expense_count = 0;
    int persisted_next_id = 1;
    int is_legacy_format = 0;
    CategoryList loaded_categories;
    ExpenseList loaded_expenses;
    StorageResult result = STORAGE_SUCCESS;

    if (filename == NULL || categories == NULL || expenses == NULL
        || categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)
        || categories->capacity > SIZE_MAX / sizeof(*categories->items)
        || expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)
        || expenses->capacity > SIZE_MAX / sizeof(*expenses->items)) {
        return STORAGE_INVALID_DATA;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return errno == ENOENT ? STORAGE_NOT_FOUND : STORAGE_FILE_ERROR;
    }

    category_list_init(&loaded_categories);
    expense_list_init(&loaded_expenses);

    if (read_line(file, line, sizeof(line)) != 1) {
        result = STORAGE_INVALID_DATA;
    } else if (strcmp(line, "FAE_STORAGE 1") == 0) {
        is_legacy_format = 1;
    } else if (strcmp(line, STORAGE_HEADER) != 0) {
        result = STORAGE_INVALID_DATA;
    }

    if (result == STORAGE_SUCCESS && !is_legacy_format
        && (read_line(file, line, sizeof(line)) != 1
            || !parse_record_header(
                line,
                "NEXT_EXPENSE_ID",
                &field_count,
                fields
            )
            || !parse_int(fields[0], &persisted_next_id)
            || persisted_next_id < 1)) {
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

    if (result == STORAGE_SUCCESS && !is_legacy_format
        && persisted_next_id < loaded_expenses.next_id) {
        result = STORAGE_INVALID_DATA;
    }

    if (result == STORAGE_SUCCESS && !is_legacy_format) {
        loaded_expenses.next_id = persisted_next_id;
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
