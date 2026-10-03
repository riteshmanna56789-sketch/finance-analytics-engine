#include "test_support.h"

#include <stdint.h>
#include <string.h>

void test_workflows(void)
{
    CategoryList categories;
    ExpenseList expenses;

    test_init_categories(&categories);
    expense_list_init(&expenses);
    assert(expense_list_add(&expenses,
        test_make_expense(1, 3, 2024, 2, 29, 1, "leap")) == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(2, 4, 2026, 9, 20, 100, "updated"))
        == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(3, 2, 2026, 9, 21, 10001, "bus"))
        == EXPENSE_SUCCESS);

    assert(expense_create(&expenses, &categories, 3, 1, "inactive")
        == EXPENSE_CATEGORY_INACTIVE);
    assert(expense_create(&expenses, &categories, 999, 1, "missing")
        == EXPENSE_CATEGORY_NOT_FOUND);
    assert(expense_update(&expenses, &categories, 2, 1, 3,
        "inactive category changed") == EXPENSE_CATEGORY_INACTIVE);

    assert(expense_delete(&expenses, 1) == EXPENSE_SUCCESS);
    assert(expenses.items[0].id == 2);
    assert(expense_delete(&expenses, 3) == EXPENSE_SUCCESS);
    assert(expenses.size == 1);
    assert(expense_delete(&expenses, 2) == EXPENSE_SUCCESS);
    assert(expenses.size == 0);
    assert(expense_delete(&expenses, 2) == EXPENSE_NOT_FOUND);
    assert(expense_create(&expenses, &categories, 2, 1, "new")
        == EXPENSE_SUCCESS);
    assert(expenses.items[0].id == 4);

    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}
