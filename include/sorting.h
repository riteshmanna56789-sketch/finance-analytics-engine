#ifndef SORTING_H
#define SORTING_H

#include <stddef.h>

#include "category.h"
#include "expense.h"

typedef enum {
    SORT_BY_DATE_NEWEST = 1,
    SORT_BY_DATE_OLDEST,
    SORT_BY_AMOUNT_LOWEST,
    SORT_BY_AMOUNT_HIGHEST,
    SORT_BY_CATEGORY_ASCENDING
} SortingOption;

typedef enum {
    SORTING_SUCCESS = 0,
    SORTING_INVALID_INPUT,
    SORTING_MEMORY_ERROR
} SortingResult;

/*
 * A non-owning view of expenses. The array is temporary, while each Expense
 * remains owned by the original ExpenseList.
 */
typedef struct {
    const Expense **items;
    size_t size;
} ExpenseView;

/*
 * Creates a sorted pointer view without changing expenses.
 *
 * Stable insertion sort is used: O(n^2) time in the worst case and O(n)
 * additional space. Stability preserves the original list order when keys
 * compare equal.
 */
SortingResult sorting_create_view(
    const ExpenseList *expenses,
    const CategoryList *categories,
    SortingOption option,
    ExpenseView *view
);

void sorting_view_destroy(ExpenseView *view);

#endif
