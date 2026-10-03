#ifndef BENCHMARK_DATA_H
#define BENCHMARK_DATA_H

#include <stddef.h>

#include "category.h"
#include "expense.h"

int benchmark_data_create(
    size_t expense_count,
    size_t category_count,
    CategoryList *categories,
    ExpenseList *expenses
);

int benchmark_categories_create(size_t category_count, CategoryList *categories);

#endif
