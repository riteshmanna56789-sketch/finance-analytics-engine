#ifndef QUERY_H
#define QUERY_H

#include <stdint.h>

#include "category.h"
#include "expense.h"

typedef enum {
    QUERY_SUCCESS = 0,
    QUERY_INVALID_INPUT,
    QUERY_TIME_ERROR
} QueryResult;

typedef enum {
    QUERY_TODAY = 1,
    QUERY_THIS_WEEK,
    QUERY_THIS_MONTH,
    QUERY_THIS_YEAR
} QueryTimeFilter;

typedef struct {
    int year;
    int month;
    int day;
} QueryDate;

typedef struct {
    int category_enabled;
    int category_id;
    int amount_enabled;
    int64_t minimum_amount_paise;
    int64_t maximum_amount_paise;
    int date_enabled;
    QueryDate start_date;
    QueryDate end_date;
    int note_enabled;
    const char *note;
} QueryFilter;

typedef void (*QueryMatchCallback)(
    const Expense *expense,
    void *context
);

QueryResult query_by_category(
    const ExpenseList *expenses,
    int category_id,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_by_amount(
    const ExpenseList *expenses,
    int64_t amount_paise,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_by_time(
    const ExpenseList *expenses,
    QueryTimeFilter filter,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_by_note(
    const ExpenseList *expenses,
    const char *note,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_by_amount_range(
    const ExpenseList *expenses,
    int64_t minimum_amount_paise,
    int64_t maximum_amount_paise,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_by_date_range(
    const ExpenseList *expenses,
    QueryDate start_date,
    QueryDate end_date,
    QueryMatchCallback callback,
    void *context
);

QueryResult query_expenses(
    const ExpenseList *expenses,
    const QueryFilter *filter,
    QueryMatchCallback callback,
    void *context
);

#endif
