#ifndef EXPENSE_MANAGEMENT_H
#define EXPENSE_MANAGEMENT_H

#include "category.h"
#include "expense.h"

void expense_management_edit(
    ExpenseList *expenses,
    const CategoryList *categories
);

void expense_management_delete(ExpenseList *expenses);

#endif
