#ifndef EXPENSE_H
#define EXPENSE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} Timestamp;

typedef struct {
    int id;
    Timestamp timestamp;
    int64_t amount_paise;
    int category_id;
    char note[200];
} Expense;

typedef struct {
    Expense *items;
    size_t size;
    size_t capacity;
} ExpenseList;

typedef enum {
    EXPENSE_SUCCESS = 0,
    EXPENSE_MEMORY_ERROR,
    EXPENSE_INVALID_INPUT
} ExpenseResult;

void expense_list_init(ExpenseList *list);
ExpenseResult expense_list_add(ExpenseList *list, Expense expense);
ExpenseResult expense_next_id(const ExpenseList *list, int *next_id);
ExpenseResult expense_create(
    ExpenseList *list,
    int category_id,
    int64_t amount_paise,
    const char *note
);
ExpenseResult expense_parse_amount_paise(
    const char *input,
    int64_t *amount_paise
);
void expense_list_destroy(ExpenseList *list);

#endif