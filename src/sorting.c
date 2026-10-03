#include "sorting.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int compare_timestamp(
    const Timestamp *left,
    const Timestamp *right
)
{
    if (left->year != right->year) {
        return left->year < right->year ? -1 : 1;
    }
    if (left->month != right->month) {
        return left->month < right->month ? -1 : 1;
    }
    if (left->day != right->day) {
        return left->day < right->day ? -1 : 1;
    }
    if (left->hour != right->hour) {
        return left->hour < right->hour ? -1 : 1;
    }
    if (left->minute != right->minute) {
        return left->minute < right->minute ? -1 : 1;
    }
    if (left->second != right->second) {
        return left->second < right->second ? -1 : 1;
    }

    return 0;
}

static int compare_expenses(
    const Expense *left,
    const Expense *right,
    const CategoryList *categories,
    SortingOption option
)
{
    int comparison;

    if (option == SORT_BY_DATE_NEWEST || option == SORT_BY_DATE_OLDEST) {
        comparison = compare_timestamp(&left->timestamp, &right->timestamp);
        return option == SORT_BY_DATE_NEWEST ? -comparison : comparison;
    }

    if (option == SORT_BY_AMOUNT_LOWEST || option == SORT_BY_AMOUNT_HIGHEST) {
        if (left->amount_paise < right->amount_paise) {
            comparison = -1;
        } else if (left->amount_paise > right->amount_paise) {
            comparison = 1;
        } else {
            comparison = 0;
        }

        return option == SORT_BY_AMOUNT_HIGHEST ? -comparison : comparison;
    }

    {
        const Category *left_category = category_find_by_id(
            categories,
            left->category_id
        );
        const Category *right_category = category_find_by_id(
            categories,
            right->category_id
        );
        const char *left_name = left_category == NULL
            ? ""
            : left_category->name;
        const char *right_name = right_category == NULL
            ? ""
            : right_category->name;

        return strcmp(left_name, right_name);
    }
}

static void merge_expense_views(
    const Expense **source,
    const Expense **destination,
    size_t begin,
    size_t middle,
    size_t end,
    const CategoryList *categories,
    SortingOption option
)
{
    size_t left = begin;
    size_t right = middle;
    size_t output = begin;

    while (left < middle && right < end) {
        if (compare_expenses(
                source[left],
                source[right],
                categories,
                option
            ) <= 0) {
            destination[output++] = source[left++];
        } else {
            destination[output++] = source[right++];
        }
    }

    while (left < middle) {
        destination[output++] = source[left++];
    }
    while (right < end) {
        destination[output++] = source[right++];
    }
}

SortingResult sorting_create_view(
    const ExpenseList *expenses,
    const CategoryList *categories,
    SortingOption option,
    ExpenseView *view
)
{
    const Expense **source;
    const Expense **destination;
    const Expense **scratch;

    if (expenses == NULL || categories == NULL || view == NULL
        || option < SORT_BY_DATE_NEWEST
        || option > SORT_BY_CATEGORY_ASCENDING
        || expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)
        || categories->size > categories->capacity
        || (categories->size > 0 && categories->items == NULL)) {
        return SORTING_INVALID_INPUT;
    }

    view->items = NULL;
    view->size = 0;

    if (expenses->size == 0) {
        return SORTING_SUCCESS;
    }

    if (expenses->size > SIZE_MAX / sizeof(*view->items)) {
        return SORTING_MEMORY_ERROR;
    }

    view->items = malloc(expenses->size * sizeof(*view->items));
    if (view->items == NULL) {
        return SORTING_MEMORY_ERROR;
    }
    view->size = expenses->size;

    for (size_t index = 0; index < expenses->size; index++) {
        view->items[index] = &expenses->items[index];
    }

    if (expenses->size < 2) {
        return SORTING_SUCCESS;
    }

    if (expenses->size > SIZE_MAX / sizeof(*destination)) {
        sorting_view_destroy(view);
        return SORTING_MEMORY_ERROR;
    }
    scratch = malloc(expenses->size * sizeof(*scratch));
    if (scratch == NULL) {
        sorting_view_destroy(view);
        return SORTING_MEMORY_ERROR;
    }
    source = view->items;
    destination = scratch;

    for (size_t width = 1; width < expenses->size;) {
        size_t begin = 0;
        const Expense **temporary;

        while (begin < expenses->size) {
            size_t middle = begin + width;
            size_t end;

            if (middle > expenses->size) {
                middle = expenses->size;
            }
            end = middle + width;
            if (end > expenses->size) {
                end = expenses->size;
            }

            merge_expense_views(
                source,
                destination,
                begin,
                middle,
                end,
                categories,
                option
            );
            begin = end;
        }

        temporary = source;
        source = destination;
        destination = temporary;

        if (width > expenses->size / 2) {
            break;
        }
        width *= 2;
    }

    if (source != view->items) {
        memcpy(
            view->items,
            source,
            expenses->size * sizeof(*view->items)
        );
    }

    free(scratch);
    return SORTING_SUCCESS;
}

void sorting_view_destroy(ExpenseView *view)
{
    if (view == NULL) {
        return;
    }

    free(view->items);
    view->items = NULL;
    view->size = 0;
}
