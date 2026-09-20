#include "expense.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const size_t INITIAL_CAPACITY = 4;

static const char *skip_spaces(const char *input)
{
    while (isspace((unsigned char)*input)) {
        input++;
    }

    return input;
}

void expense_list_init(ExpenseList *list)
{
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}

ExpenseResult expense_list_add(ExpenseList *list, Expense expense)
{
    if (list == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    if (list->size == list->capacity) {
        size_t new_capacity = list->capacity == 0
            ? INITIAL_CAPACITY
            : list->capacity * 2;
        Expense *resized_items = realloc(
            list->items,
            new_capacity * sizeof(*resized_items)
        );

        if (resized_items == NULL) {
            return EXPENSE_MEMORY_ERROR;
        }

        list->items = resized_items;
        list->capacity = new_capacity;
    }

    list->items[list->size] = expense;
    list->size++;

    return EXPENSE_SUCCESS;
}

ExpenseResult expense_create(
    ExpenseList *list,
    int category_id,
    int64_t amount_paise,
    const char *note
)
{
    Expense expense;
    int next_id;
    time_t current_time;
    struct tm *local_time;
    size_t note_length;

    if (list == NULL || category_id < 1 || amount_paise <= 0 || note == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    note_length = strlen(note);
    if (note_length >= sizeof(expense.note)) {
        return EXPENSE_INVALID_INPUT;
    }

    if (expense_next_id(list, &next_id) != EXPENSE_SUCCESS) {
        return EXPENSE_INVALID_INPUT;
    }

    current_time = time(NULL);
    local_time = localtime(&current_time);
    if (local_time == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    expense.id = next_id;
    expense.timestamp.year = local_time->tm_year + 1900;
    expense.timestamp.month = local_time->tm_mon + 1;
    expense.timestamp.day = local_time->tm_mday;
    expense.timestamp.hour = local_time->tm_hour;
    expense.timestamp.minute = local_time->tm_min;
    expense.timestamp.second = local_time->tm_sec;
    expense.amount_paise = amount_paise;
    expense.category_id = category_id;
    memcpy(expense.note, note, note_length + 1);

    return expense_list_add(list, expense);
}

ExpenseResult expense_next_id(const ExpenseList *list, int *next_id)
{
    int highest_id = 0;

    if (list == NULL || next_id == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id > highest_id) {
            highest_id = list->items[index].id;
        }
    }

    if (highest_id == INT_MAX) {
        return EXPENSE_INVALID_INPUT;
    }

    *next_id = highest_id + 1;
    return EXPENSE_SUCCESS;
}

ExpenseResult expense_parse_amount_paise(
    const char *input,
    int64_t *amount_paise
)
{
    const char *cursor;
    uint64_t whole = 0;
    unsigned int fractional = 0;
    int fractional_digits = 0;
    int has_digit = 0;
    int has_decimal = 0;
    const uint64_t maximum_paise = INT64_MAX;

    if (input == NULL || amount_paise == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    cursor = skip_spaces(input);
    if (*cursor == '+' || *cursor == '-') {
        return EXPENSE_INVALID_INPUT;
    }

    while (isdigit((unsigned char)*cursor)) {
        unsigned int digit = (unsigned int)(*cursor - '0');

        if (whole > (maximum_paise - digit) / 100) {
            return EXPENSE_INVALID_INPUT;
        }
        whole = whole * 10 + digit;
        has_digit = 1;
        cursor++;
    }

    if (*cursor == '.') {
        has_decimal = 1;
        cursor++;
        while (isdigit((unsigned char)*cursor)) {
            if (fractional_digits == 2) {
                return EXPENSE_INVALID_INPUT;
            }
            fractional = fractional * 10
                + (unsigned int)(*cursor - '0');
            fractional_digits++;
            has_digit = 1;
            cursor++;
        }
    }

    cursor = skip_spaces(cursor);
    if (!has_digit || *cursor != '\0'
        || (has_decimal && fractional_digits == 0)) {
        return EXPENSE_INVALID_INPUT;
    }

    if (fractional_digits == 0) {
        fractional = 0;
    } else if (fractional_digits == 1) {
        fractional *= 10;
    }

    if (whole > (maximum_paise - fractional) / 100) {
        return EXPENSE_INVALID_INPUT;
    }

    whole = whole * 100 + fractional;
    if (whole == 0) {
        return EXPENSE_INVALID_INPUT;
    }

    *amount_paise = (int64_t)whole;
    return EXPENSE_SUCCESS;
}

void expense_list_destroy(ExpenseList *list)
{
    if (list == NULL) {
        return;
    }

    free(list->items);
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}