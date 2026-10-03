#include "test_support.h"

#include "sorting.h"

#include <stdlib.h>

static void assert_sorted_ids(
    const ExpenseList *expenses,
    const CategoryList *categories,
    SortingOption option,
    const int *expected_ids,
    size_t expected_size
)
{
    ExpenseView view = {0};

    assert(sorting_create_view(expenses, categories, option, &view)
        == SORTING_SUCCESS);
    assert(view.size == expected_size);
    for (size_t index = 0; index < expected_size; index++) {
        assert(view.items[index]->id == expected_ids[index]);
    }
    sorting_view_destroy(&view);
}

static void test_large_sort(
    const CategoryList *categories
)
{
    const size_t expense_count = 100000;
    ExpenseList expenses;
    Expense *snapshot;
    ExpenseView view = {0};

    expense_list_init(&expenses);
    expenses.items = malloc(expense_count * sizeof(*expenses.items));
    assert(expenses.items != NULL);
    expenses.size = expense_count;
    expenses.capacity = expense_count;
    expenses.next_id = (int)expense_count + 1;
    for (size_t index = 0; index < expense_count; index++) {
        expenses.items[index] = test_make_expense(
            (int)index + 1,
            2,
            2026,
            9,
            20,
            (int64_t)((expense_count - index) % 997) + 1,
            "large sort"
        );
    }

    snapshot = malloc(expense_count * sizeof(*snapshot));
    assert(snapshot != NULL);
    memcpy(snapshot, expenses.items, expense_count * sizeof(*snapshot));

    assert(sorting_create_view(
        &expenses,
        categories,
        SORT_BY_AMOUNT_LOWEST,
        &view
    ) == SORTING_SUCCESS);
    assert(view.size == expense_count);
    for (size_t index = 1; index < view.size; index++) {
        const Expense *previous = view.items[index - 1];
        const Expense *current = view.items[index];

        assert(previous->amount_paise <= current->amount_paise);
        if (previous->amount_paise == current->amount_paise) {
            assert(previous->id < current->id);
        }
    }
    assert(memcmp(
        expenses.items,
        snapshot,
        expense_count * sizeof(*snapshot)
    ) == 0);

    sorting_view_destroy(&view);
    free(snapshot);
    expense_list_destroy(&expenses);
}

void test_sorting(void)
{
    CategoryList categories;
    ExpenseList expenses;
    ExpenseList empty_expenses;
    ExpenseView view;

    test_init_categories(&categories);
    expense_list_init(&expenses);
    expense_list_init(&empty_expenses);
    assert(expense_list_add(&expenses,
        test_make_expense(1, 3, 2024, 2, 29, 1, "leap")) == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(2, 4, 2026, 9, 20, INT64_MAX, "updated"))
        == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(3, 2, 2026, 9, 21, 10001, "bus"))
        == EXPENSE_SUCCESS);

    {
        const int newest_ids[] = {3, 2, 1};
        const int oldest_ids[] = {1, 2, 3};
        const int amount_low_ids[] = {1, 3, 2};
        const int amount_high_ids[] = {2, 3, 1};
        const int category_ids[] = {3, 1, 2};
        Expense original_items[3];

        memcpy(original_items, expenses.items, sizeof(original_items));
        assert_sorted_ids(
            &expenses,
            &categories,
            SORT_BY_DATE_NEWEST,
            newest_ids,
            3
        );
        assert_sorted_ids(
            &expenses,
            &categories,
            SORT_BY_DATE_OLDEST,
            oldest_ids,
            3
        );
        assert_sorted_ids(
            &expenses,
            &categories,
            SORT_BY_AMOUNT_LOWEST,
            amount_low_ids,
            3
        );
        assert_sorted_ids(
            &expenses,
            &categories,
            SORT_BY_AMOUNT_HIGHEST,
            amount_high_ids,
            3
        );
        assert_sorted_ids(
            &expenses,
            &categories,
            SORT_BY_CATEGORY_ASCENDING,
            category_ids,
            3
        );
        assert(memcmp(expenses.items, original_items, sizeof(original_items))
            == 0);
    }

    assert(sorting_create_view(
        &empty_expenses,
        &categories,
        SORT_BY_DATE_NEWEST,
        &view
    ) == SORTING_SUCCESS && view.size == 0);
    assert(view.items == NULL);
    sorting_view_destroy(&view);

    {
        ExpenseList single_expense;
        expense_list_init(&single_expense);
        assert(expense_list_add(
            &single_expense,
            test_make_expense(20, 2, 2026, 1, 1, 1, "single")
        ) == EXPENSE_SUCCESS);
        assert_sorted_ids(
            &single_expense,
            &categories,
            SORT_BY_DATE_NEWEST,
            (const int[]){20},
            1
        );
        expense_list_destroy(&single_expense);
    }

    {
        ExpenseList category_ties;
        expense_list_init(&category_ties);
        assert(expense_list_add(
            &category_ties,
            test_make_expense(30, 2, 2026, 1, 1, 100, "first food")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &category_ties,
            test_make_expense(31, 2, 2026, 1, 2, 200, "second food")
        ) == EXPENSE_SUCCESS);
        assert_sorted_ids(
            &category_ties,
            &categories,
            SORT_BY_CATEGORY_ASCENDING,
            (const int[]){30, 31},
            2
        );
        expense_list_destroy(&category_ties);
    }

    {
        ExpenseList identical_keys;
        const int original_ids[] = {40, 41, 42};
        const SortingOption options[] = {
            SORT_BY_DATE_NEWEST,
            SORT_BY_DATE_OLDEST,
            SORT_BY_AMOUNT_LOWEST,
            SORT_BY_AMOUNT_HIGHEST,
            SORT_BY_CATEGORY_ASCENDING
        };

        expense_list_init(&identical_keys);
        for (size_t index = 0;
             index < sizeof(original_ids) / sizeof(original_ids[0]);
             index++) {
            assert(expense_list_add(
                &identical_keys,
                test_make_expense(
                    original_ids[index],
                    2,
                    2026,
                    1,
                    1,
                    500,
                    "same keys"
                )
            ) == EXPENSE_SUCCESS);
        }
        for (size_t index = 0;
             index < sizeof(options) / sizeof(options[0]);
             index++) {
            assert_sorted_ids(
                &identical_keys,
                &categories,
                options[index],
                original_ids,
                sizeof(original_ids) / sizeof(original_ids[0])
            );
        }
        expense_list_destroy(&identical_keys);
    }

    {
        ExpenseList ties;
        expense_list_init(&ties);
        assert(expense_list_add(
            &ties,
            test_make_expense(10, 2, 2026, 9, 20, 100, "first")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &ties,
            test_make_expense(11, 4, 2026, 9, 20, 100, "second")
        ) == EXPENSE_SUCCESS);
        assert(sorting_create_view(
            &ties,
            &categories,
            SORT_BY_AMOUNT_LOWEST,
            &view
        ) == SORTING_SUCCESS);
        assert(view.items[0]->id == 10 && view.items[1]->id == 11);
        sorting_view_destroy(&view);
        assert(sorting_create_view(
            &ties,
            &categories,
            SORT_BY_DATE_NEWEST,
            &view
        ) == SORTING_SUCCESS);
        assert(view.items[0]->id == 10 && view.items[1]->id == 11);
        sorting_view_destroy(&view);
        expense_list_destroy(&ties);
    }
    assert(sorting_create_view(
        &expenses,
        &categories,
        SORT_BY_AMOUNT_HIGHEST,
        &view
    ) == SORTING_SUCCESS && view.items[0]->id == 2);
    assert(expenses.items[0].id == 1
        && expenses.items[1].id == 2
        && expenses.items[2].id == 3);
    sorting_view_destroy(&view);

    test_large_sort(&categories);

    expense_list_destroy(&empty_expenses);
    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}
