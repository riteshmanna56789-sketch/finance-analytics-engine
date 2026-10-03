#include "test_support.h"

#include <stdint.h>
#include <string.h>

void test_expense(void)
{
    CategoryList categories;
    ExpenseList expenses;
    Expense original;
    int64_t amount_paise;
    char maximum_note[200];
    char long_note[201];

    test_init_categories(&categories);
    expense_list_init(&expenses);
    assert(expense_parse_amount_paise("0.01", &amount_paise)
        == EXPENSE_SUCCESS && amount_paise == 1);
    assert(expense_parse_amount_paise(
        "92233720368547758.07",
        &amount_paise
    ) == EXPENSE_SUCCESS && amount_paise == INT64_MAX);
    assert(expense_parse_amount_paise("92233720368547758.08", &amount_paise)
        == EXPENSE_INVALID_INPUT);
    assert(expense_parse_amount_paise("-1", &amount_paise)
        == EXPENSE_INVALID_INPUT);
    assert(expense_parse_amount_paise("0", &amount_paise)
        == EXPENSE_INVALID_INPUT);
    assert(expense_parse_amount_paise("   ", &amount_paise)
        == EXPENSE_INVALID_INPUT);
    assert(expense_parse_amount_paise("", &amount_paise)
        == EXPENSE_INVALID_INPUT);

    memset(maximum_note, 'N', sizeof(maximum_note) - 1);
    maximum_note[sizeof(maximum_note) - 1] = '\0';
    assert(expense_list_add(
        &expenses,
        test_make_expense(1, 3, 2024, 2, 29, 1, maximum_note)
    ) == EXPENSE_SUCCESS);
    assert(expense_list_add(
        &expenses,
        test_make_expense(2, 3, 2026, 9, 20, 50000, "Lunch at restaurant")
    ) == EXPENSE_SUCCESS);
    assert(expense_list_add(
        &expenses,
        test_make_expense(3, 2, 2026, 9, 21, 10001, "bus")
    ) == EXPENSE_SUCCESS);
    assert(expenses.next_id == 4);
    assert(expense_list_add(
        &expenses,
        test_make_expense(2, 2, 2026, 9, 21, 10000, "duplicate")
    ) == EXPENSE_INVALID_INPUT);
    assert(expense_list_add(
        &expenses,
        test_make_expense(4, 2, 2026, 2, 30, 10000, "impossible")
    ) == EXPENSE_INVALID_INPUT);
    original = expenses.items[1];
    assert(expense_update(
        &expenses,
        &categories,
        2,
        INT64_MAX,
        4,
        "updated"
    ) == EXPENSE_SUCCESS);
    assert(expenses.items[1].id == original.id);
    assert(memcmp(&expenses.items[1].timestamp, &original.timestamp,
        sizeof(original.timestamp)) == 0);
    assert(expenses.items[1].amount_paise == INT64_MAX);
    assert(expense_update(&expenses, &categories, 999, 1, 2, "missing")
        == EXPENSE_NOT_FOUND);
    assert(expense_update(&expenses, &categories, 2, 0, 2, "invalid")
        == EXPENSE_INVALID_INPUT);
    memset(long_note, 'L', sizeof(long_note) - 1);
    long_note[sizeof(long_note) - 1] = '\0';
    assert(expense_update(&expenses, &categories, 2, 1, 2, long_note)
        == EXPENSE_INVALID_INPUT);

    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}
