#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#ifdef NDEBUG
#error "Tests require assertions to remain enabled"
#endif

#include "category.h"
#include "expense.h"

#include <assert.h>
#include <string.h>

static inline Expense test_make_expense(
    int id,
    int category_id,
    int year,
    int month,
    int day,
    int64_t amount_paise,
    const char *note
)
{
    Expense expense = {0};

    expense.id = id;
    expense.category_id = category_id;
    expense.timestamp.year = year;
    expense.timestamp.month = month;
    expense.timestamp.day = day;
    expense.amount_paise = amount_paise;
    assert(strlen(note) < sizeof(expense.note));
    strcpy(expense.note, note);
    return expense;
}

static inline void test_init_categories(CategoryList *categories)
{
    char longest_name[sizeof(((Category *)0)->name)];

    category_list_init(categories);
    memset(longest_name, 'X', sizeof(longest_name) - 1);
    longest_name[sizeof(longest_name) - 1] = '\0';
    assert(category_create(categories, longest_name) == CATEGORY_SUCCESS);
    assert(category_create(categories, "Food") == CATEGORY_SUCCESS);
    assert(category_create(categories, "Petrol") == CATEGORY_SUCCESS);
    assert(category_create(categories, "Rent") == CATEGORY_SUCCESS);
    assert(category_deactivate(categories, 3) == CATEGORY_SUCCESS);
}

#endif
