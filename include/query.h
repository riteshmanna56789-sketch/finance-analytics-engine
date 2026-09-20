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

#endif
