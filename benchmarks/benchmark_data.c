#include "benchmark_data.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void initialize_categories(size_t category_count, CategoryList *categories)
{
    categories->items = calloc(category_count, sizeof(*categories->items));
    if (categories->items == NULL) {
        categories->size = 0;
        categories->capacity = 0;
        return;
    }

    categories->size = category_count;
    categories->capacity = category_count;
    for (size_t index = 0; index < category_count; index++) {
        Category *category = &categories->items[index];
        category->id = (int)(index + 1);
        category->is_active = 1;
        (void)snprintf(
            category->name,
            sizeof(category->name),
            "Category %zu",
            index + 1
        );
    }
}

int benchmark_categories_create(size_t category_count, CategoryList *categories)
{
    if (categories == NULL || category_count == 0
        || category_count > (size_t)INT_MAX) {
        return 0;
    }

    category_list_init(categories);
    initialize_categories(category_count, categories);
    return categories->items != NULL;
}

int benchmark_data_create(
    size_t expense_count,
    size_t category_count,
    CategoryList *categories,
    ExpenseList *expenses
)
{
    if (categories == NULL || expenses == NULL || expense_count == 0
        || expense_count > (size_t)INT_MAX
        || category_count == 0 || category_count > (size_t)INT_MAX) {
        return 0;
    }

    category_list_init(categories);
    expense_list_init(expenses);
    initialize_categories(category_count, categories);
    if (categories->items == NULL) {
        return 0;
    }

    expenses->items = calloc(expense_count, sizeof(*expenses->items));
    if (expenses->items == NULL) {
        category_list_destroy(categories);
        return 0;
    }
    expenses->size = expense_count;
    expenses->capacity = expense_count;
    expenses->next_id = (int)expense_count + 1;

    for (size_t index = 0; index < expense_count; index++) {
        Expense *expense = &expenses->items[index];
        size_t month_index = index % 12;
        size_t day_index = (index / 12) % 28;

        expense->id = (int)index + 1;
        expense->timestamp.year = 2024;
        expense->timestamp.month = (int)month_index + 1;
        expense->timestamp.day = (int)day_index + 1;
        expense->timestamp.hour = (int)(index % 24);
        expense->timestamp.minute = (int)(index % 60);
        expense->timestamp.second = (int)(index % 60);
        expense->amount_paise = (int64_t)((index * 7919) % 1000000) + 1;
        expense->category_id = (int)(index % category_count) + 1;
        if (index % 2 == 0) {
            (void)strcpy(expense->note, "benchmark transaction");
        } else {
            (void)strcpy(expense->note, "regular expense");
        }
    }

    return 1;
}
