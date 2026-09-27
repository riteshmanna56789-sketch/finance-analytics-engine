#include "expense.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const size_t INITIAL_CAPACITY = 4;

static int timestamp_is_valid(const Timestamp *timestamp)
{
    int days_in_month;

    if (timestamp->year < 1
        || timestamp->month < 1 || timestamp->month > 12
        || timestamp->day < 1
        || timestamp->hour < 0 || timestamp->hour > 23
        || timestamp->minute < 0 || timestamp->minute > 59
        || timestamp->second < 0 || timestamp->second > 60) {
        return 0;
    }

    days_in_month = 31;
    if (timestamp->month == 4 || timestamp->month == 6
        || timestamp->month == 9 || timestamp->month == 11) {
        days_in_month = 30;
    } else if (timestamp->month == 2) {
        int leap_year = timestamp->year % 400 == 0
            || (timestamp->year % 4 == 0 && timestamp->year % 100 != 0);
        days_in_month = leap_year ? 29 : 28;
    }

    return timestamp->day <= days_in_month;
}

static int note_is_single_line(const char *note)
{
    while (*note != '\0') {
        if (*note == '\r' || *note == '\n') {
            return 0;
        }
        note++;
    }

    return 1;
}

static const char *skip_spaces(const char *input)
{
    while (isspace((unsigned char)*input)) {
        input++;
    }

    return input;
}

void expense_list_init(ExpenseList *list)
{
    if (list == NULL) {
        return;
    }

    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
    list->next_id = 1;
}

ExpenseResult expense_list_add(ExpenseList *list, Expense expense)
{
    if (list == NULL || expense.id < 1 || expense.category_id < 1
        || expense.amount_paise <= 0
        || !timestamp_is_valid(&expense.timestamp)
        || memchr(expense.note, '\0', sizeof(expense.note)) == NULL
        || !note_is_single_line(expense.note)
        || list->size > list->capacity
        || (list->size > 0 && list->items == NULL)
        || list->capacity > SIZE_MAX / sizeof(*list->items)) {
        return EXPENSE_INVALID_INPUT;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == expense.id) {
            return EXPENSE_INVALID_INPUT;
        }
    }

    if (list->size == list->capacity) {
        size_t new_capacity;

        if (list->capacity == 0) {
            new_capacity = INITIAL_CAPACITY;
        } else {
            if (list->capacity > SIZE_MAX / 2) {
                return EXPENSE_MEMORY_ERROR;
            }
            new_capacity = list->capacity * 2;
        }

        if (new_capacity > SIZE_MAX / sizeof(*list->items)) {
            return EXPENSE_MEMORY_ERROR;
        }

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

    if (expense.id >= list->next_id && expense.id < INT_MAX) {
        list->next_id = expense.id + 1;
    } else if (expense.id == INT_MAX) {
        list->next_id = INT_MAX;
    }

    return EXPENSE_SUCCESS;
}

ExpenseResult expense_create(
    ExpenseList *list,
    const CategoryList *categories,
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
    const Category *category;
    size_t maximum_note_length = sizeof(((Expense *)0)->note) - 1;

    if (list == NULL || categories == NULL || category_id < 1
        || amount_paise <= 0 || note == NULL
        || list->size > list->capacity
        || (list->size > 0 && list->items == NULL)
        || categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)) {
        return EXPENSE_INVALID_INPUT;
    }

    category = category_find_by_id(categories, category_id);
    if (category == NULL) {
        return EXPENSE_CATEGORY_NOT_FOUND;
    }
    if (!category->is_active) {
        return EXPENSE_CATEGORY_INACTIVE;
    }

    note_length = 0;
    while (note_length <= maximum_note_length && note[note_length] != '\0') {
        note_length++;
    }
    if (note_length > maximum_note_length || !note_is_single_line(note)) {
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
    if (list == NULL || next_id == NULL
        || list->size > list->capacity
        || (list->size > 0 && list->items == NULL)
        || list->next_id < 1) {
        return EXPENSE_INVALID_INPUT;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == list->next_id) {
            return EXPENSE_INVALID_INPUT;
        }
    }

    if (list->next_id < 1 || list->next_id > INT_MAX) {
        return EXPENSE_INVALID_INPUT;
    }

    *next_id = list->next_id;
    return EXPENSE_SUCCESS;
}

const Expense *expense_find_by_id(
    const ExpenseList *list,
    int expense_id
)
{
    if (list == NULL || expense_id < 1 || list->size > list->capacity
        || (list->size > 0 && list->items == NULL)) {
        return NULL;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == expense_id) {
            return &list->items[index];
        }
    }

    return NULL;
}

ExpenseResult expense_update(
    ExpenseList *list,
    const CategoryList *categories,
    int expense_id,
    int64_t amount_paise,
    int category_id,
    const char *note
)
{
    Expense *expense = NULL;
    const Category *category;
    size_t note_length;
    size_t maximum_note_length = sizeof(((Expense *)0)->note) - 1;

    if (list == NULL || categories == NULL || expense_id < 1
        || amount_paise <= 0 || category_id < 1 || note == NULL) {
        return EXPENSE_INVALID_INPUT;
    }
    if (list->size > list->capacity
        || (list->size > 0 && list->items == NULL)
        || categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)) {
        return EXPENSE_INVALID_INPUT;
    }

    for (size_t index = 0; index < list->size; index++) {
        if (list->items[index].id == expense_id) {
            expense = &list->items[index];
            break;
        }
    }

    if (expense == NULL) {
        return EXPENSE_NOT_FOUND;
    }

    note_length = 0;
    while (note_length <= maximum_note_length && note[note_length] != '\0') {
        note_length++;
    }
    if (note_length > maximum_note_length || !note_is_single_line(note)) {
        return EXPENSE_INVALID_INPUT;
    }

    category = category_find_by_id(categories, category_id);
    if (category == NULL) {
        return EXPENSE_CATEGORY_NOT_FOUND;
    }
    if (category_id != expense->category_id && !category->is_active) {
        return EXPENSE_CATEGORY_INACTIVE;
    }

    expense->amount_paise = amount_paise;
    expense->category_id = category_id;
    memcpy(expense->note, note, note_length + 1);

    return EXPENSE_SUCCESS;
}

ExpenseResult expense_delete(ExpenseList *list, int expense_id)
{
    size_t index;

    if (list == NULL || expense_id < 1
        || list->size > list->capacity
        || (list->size > 0 && list->items == NULL)) {
        return EXPENSE_INVALID_INPUT;
    }

    for (index = 0; index < list->size; index++) {
        if (list->items[index].id == expense_id) {
            break;
        }
    }

    if (index == list->size) {
        return EXPENSE_NOT_FOUND;
    }

    if (index + 1 < list->size) {
        memmove(
            &list->items[index],
            &list->items[index + 1],
            (list->size - index - 1) * sizeof(*list->items)
        );
    }
    list->size--;

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
    const uint64_t maximum_whole = INT64_MAX / 100;

    if (input == NULL || amount_paise == NULL) {
        return EXPENSE_INVALID_INPUT;
    }

    cursor = skip_spaces(input);
    if (*cursor == '+' || *cursor == '-') {
        return EXPENSE_INVALID_INPUT;
    }

    while (isdigit((unsigned char)*cursor)) {
        unsigned int digit = (unsigned int)(*cursor - '0');

        if (whole > (maximum_whole - digit) / 10) {
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
    list->next_id = 1;
}