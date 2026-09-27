#include "analytics.h"
#include "category.h"
#include "expense.h"
#include "query.h"
#include "sorting.h"
#include "storage.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t matches;

static void count_match(const Expense *expense, void *context)
{
    (void)expense;
    (*(size_t *)context)++;
}

static Expense make_expense(
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

static void write_file(const char *filename, const char *contents)
{
    FILE *file = fopen(filename, "wb");
    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

int main(void)
{
    const char *test_file = "tests/hardening_storage.dat";
    CategoryList categories;
    CategoryList loaded_categories;
    CategoryList empty_categories;
    ExpenseList expenses;
    ExpenseList loaded_expenses;
    ExpenseList empty_expenses;
    CategoryList verify_categories;
    ExpenseList verify_expenses;
    ExpenseView view;
    AnalyticsSummary summary;
    Expense original;
    int64_t amount_paise;
    char long_category_name[50];
    char maximum_note[200];
    char long_note[201];

    category_list_init(NULL);
    expense_list_init(NULL);
    category_list_init(&categories);
    category_list_init(&loaded_categories);
    category_list_init(&empty_categories);
    category_list_init(&verify_categories);
    expense_list_init(&expenses);
    expense_list_init(&loaded_expenses);
    expense_list_init(&empty_expenses);
    expense_list_init(&verify_expenses);

    assert(category_create(&categories, NULL) == CATEGORY_INVALID_INPUT);
    assert(category_deactivate(&categories, 1) == CATEGORY_NOT_FOUND);
    memset(long_category_name, 'X', sizeof(long_category_name) - 1);
    long_category_name[sizeof(long_category_name) - 1] = '\0';
    assert(category_create(&categories, long_category_name) == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Food") == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Petrol") == CATEGORY_SUCCESS);
    assert(category_create(&categories, "Rent") == CATEGORY_SUCCESS);
    assert(category_active_count(&categories) == 4);
    assert(category_list_add(&categories, categories.items[0])
        == CATEGORY_DUPLICATE);
    assert(category_deactivate(&categories, 3) == CATEGORY_SUCCESS);
    assert(category_find_by_id(&categories, 3)->is_active == 0);
    assert(category_active_count(&categories) == 3);
    assert(category_deactivate(&categories, 3) == CATEGORY_ALREADY_INACTIVE);
    assert(category_deactivate(&categories, 999) == CATEGORY_NOT_FOUND);
    assert(category_deactivate(NULL, 1) == CATEGORY_INVALID_INPUT);

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
        make_expense(1, 3, 2024, 2, 29, 1, maximum_note)
    ) == EXPENSE_SUCCESS);
    assert(expense_list_add(
        &expenses,
        make_expense(2, 3, 2026, 9, 20, 50000, "Lunch at restaurant")
    ) == EXPENSE_SUCCESS);
    assert(expense_list_add(
        &expenses,
        make_expense(3, 2, 2026, 9, 21, 10001, "bus")
    ) == EXPENSE_SUCCESS);
    assert(expenses.next_id == 4);
    assert(expense_list_add(
        &expenses,
        make_expense(2, 2, 2026, 9, 21, 10000, "duplicate")
    ) == EXPENSE_INVALID_INPUT);
    assert(expense_list_add(
        &expenses,
        make_expense(4, 2, 2026, 2, 30, 10000, "impossible")
    ) == EXPENSE_INVALID_INPUT);
    assert(expense_create(&expenses, &categories, 3, 1, "inactive")
        == EXPENSE_CATEGORY_INACTIVE);
    assert(expense_create(&expenses, &categories, 999, 1, "missing")
        == EXPENSE_CATEGORY_NOT_FOUND);

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
    assert(expense_update(
        &expenses,
        &categories,
        2,
        1,
        3,
        "inactive category changed"
    ) == EXPENSE_CATEGORY_INACTIVE);
    assert(expense_update(&expenses, &categories, 999, 1, 2, "missing")
        == EXPENSE_NOT_FOUND);
    assert(expense_update(&expenses, &categories, 2, 0, 2, "invalid")
        == EXPENSE_INVALID_INPUT);
    memset(long_note, 'L', sizeof(long_note) - 1);
    long_note[sizeof(long_note) - 1] = '\0';
    assert(expense_update(&expenses, &categories, 2, 1, 2, long_note)
        == EXPENSE_INVALID_INPUT);

    matches = 0;
    assert(query_by_note(&expenses, "UPDATED", count_match, &matches)
        == QUERY_SUCCESS && matches == 1);
    matches = 0;
    assert(query_by_category(&expenses, 3, count_match, &matches)
        == QUERY_SUCCESS && matches == 1);
    matches = 0;
    assert(query_by_note(&expenses, "", count_match, &matches)
        == QUERY_SUCCESS && matches == 0);
    matches = 0;
    assert(query_by_amount_range(&expenses, 10000, INT64_MAX,
        count_match, &matches) == QUERY_SUCCESS && matches == 2);
    assert(query_by_amount_range(&expenses, 2, 1, count_match, &matches)
        == QUERY_INVALID_INPUT);
    matches = 0;
    assert(query_by_date_range(
        &expenses,
        (QueryDate){2026, 9, 20},
        (QueryDate){2026, 9, 20},
        count_match,
        &matches
    ) == QUERY_SUCCESS && matches == 1);
    assert(query_by_date_range(
        &expenses,
        (QueryDate){2026, 2, 30},
        (QueryDate){2026, 3, 1},
        count_match,
        &matches
    ) == QUERY_INVALID_INPUT);

    {
        QueryFilter filter = {0};
        filter.category_enabled = 1;
        filter.category_id = 4;
        filter.amount_enabled = 1;
        filter.minimum_amount_paise = INT64_MAX;
        filter.maximum_amount_paise = INT64_MAX;
        filter.date_enabled = 1;
        filter.start_date = (QueryDate){2026, 9, 20};
        filter.end_date = (QueryDate){2026, 9, 20};
        filter.note_enabled = 1;
        filter.note = "updated";
        matches = 0;
        assert(query_expenses(&expenses, &filter, count_match, &matches)
            == QUERY_SUCCESS && matches == 1);
    }

    assert(sorting_create_view(
        &empty_expenses,
        &categories,
        SORT_BY_DATE_NEWEST,
        &view
    ) == SORTING_SUCCESS && view.size == 0);
    sorting_view_destroy(&view);
    {
        ExpenseList ties;
        expense_list_init(&ties);
        assert(expense_list_add(
            &ties,
            make_expense(10, 2, 2026, 9, 20, 100, "first")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &ties,
            make_expense(11, 4, 2026, 9, 20, 100, "second")
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

    assert(analytics_calculate_summary(
        &empty_expenses,
        &empty_categories,
        &summary
    ) == ANALYTICS_SUCCESS
        && summary.total_paise == 0
        && summary.transaction_count == 0
        && summary.average_paise == 0
        && summary.minimum_paise == 0
        && summary.maximum_paise == 0);
    analytics_summary_destroy(&summary);
    assert(analytics_calculate_summary(
        &expenses,
        &categories,
        &summary
    ) == ANALYTICS_OVERFLOW);
    analytics_summary_destroy(&summary);
    assert(expense_update(&expenses, &categories, 2, 100, 4, "updated")
        == EXPENSE_SUCCESS);
    assert(analytics_calculate_summary(
        &expenses,
        &categories,
        &summary
    ) == ANALYTICS_SUCCESS);
    assert(summary.total_paise == 10102);
    assert(summary.transaction_count == 3);
    assert(summary.average_paise == 3367);
    assert(summary.average_remainder_paise == 1);
    assert(summary.minimum_paise == 1);
    assert(summary.maximum_paise == 10001);
    assert(summary.category_totals_paise[2] == 1);
    assert(summary.category_totals_paise[3] == 100);
    analytics_summary_destroy(&summary);
    {
        int original_category_id = expenses.items[0].category_id;

        expenses.items[0].category_id = 999;
        assert(analytics_calculate_summary(
            &expenses,
            &categories,
            &summary
        ) == ANALYTICS_INVALID_CATEGORY);
        assert(summary.category_totals_paise == NULL);
        expenses.items[0].category_id = original_category_id;
    }

    {
        ExpenseList one_expense;
        expense_list_init(&one_expense);
        assert(expense_list_add(
            &one_expense,
            make_expense(20, 2, 2026, 9, 20, INT64_MAX, "largest")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_summary(
            &one_expense,
            &categories,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 1);
        assert(summary.average_paise == INT64_MAX);
        assert(summary.average_remainder_paise == 0);
        assert(summary.minimum_paise == INT64_MAX);
        assert(summary.maximum_paise == INT64_MAX);
        analytics_summary_destroy(&summary);
        expense_list_destroy(&one_expense);
    }

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

    assert(storage_save(test_file, &categories, &expenses) == STORAGE_SUCCESS);
    assert(storage_load(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(loaded_categories.items[2].is_active == 0);
    assert(loaded_expenses.size == 1 && loaded_expenses.items[0].id == 4);
    assert(loaded_expenses.next_id == 5);

    strcpy(loaded_expenses.items[0].note, maximum_note);
    assert(storage_save(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(storage_load(
        test_file,
        &loaded_categories,
        &loaded_expenses
    ) == STORAGE_SUCCESS);
    assert(strlen(loaded_categories.items[0].name) == 49);
    assert(strlen(loaded_expenses.items[0].note) == 199);

    loaded_expenses.items[0].amount_paise = -1;
    assert(storage_save(test_file, &loaded_categories, &loaded_expenses)
        == STORAGE_INVALID_DATA);
    loaded_expenses.items[0].amount_paise = 1;
    assert(storage_load(
        test_file,
        &verify_categories,
        &verify_expenses
    ) == STORAGE_SUCCESS);
    assert(verify_expenses.items[0].amount_paise == 1);

    {
        const char *malformed_files[] = {
            "",
            "wrong header\n",
            "FAE_STORAGE 3\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID -1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES -1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY -1 1 4\nFood\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 2 30 0 0 0 1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 99 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 -1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 1\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 1 5\nabc\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFoo\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES "
                "18446744073709551615\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 2\n"
                "EXPENSE 1 2026 1 1 0 0 0 1 1 0\n\n"
                "EXPENSE 1 2026 1 2 0 0 0 1 1 0\n\n",
            "FAE_STORAGE 2\nNEXT_EXPENSE_ID 2\nCATEGORIES 1\n"
                "CATEGORY 1 1 4\nFood\nEXPENSES 0\nEXTRA\n"
        };

        for (size_t index = 0;
             index < sizeof(malformed_files) / sizeof(malformed_files[0]);
             index++) {
            write_file(test_file, malformed_files[index]);
            assert(storage_load(
                test_file,
                &loaded_categories,
                &loaded_expenses
            ) == STORAGE_INVALID_DATA);
            assert(loaded_categories.size == categories.size);
            assert(loaded_expenses.size == 1
                && loaded_expenses.items[0].id == 4);
        }
    }

    write_file(
        test_file,
        "FAE_STORAGE 2\nNEXT_EXPENSE_ID 1\nCATEGORIES 0\nEXPENSES 0"
    );
    assert(storage_load(test_file, &loaded_categories, &loaded_expenses)
        == STORAGE_SUCCESS);
    assert(loaded_categories.size == 0 && loaded_expenses.size == 0);

    assert(remove(test_file) == 0);
    {
        char temporary_file[128];
        assert(snprintf(
            temporary_file,
            sizeof(temporary_file),
            "%s.tmp",
            test_file
        ) > 0);
        remove(temporary_file);
    }

    category_list_destroy(&categories);
    category_list_destroy(&loaded_categories);
    category_list_destroy(&empty_categories);
    category_list_destroy(&verify_categories);
    expense_list_destroy(&expenses);
    expense_list_destroy(&loaded_expenses);
    expense_list_destroy(&empty_expenses);
    expense_list_destroy(&verify_expenses);
    return 0;
}
