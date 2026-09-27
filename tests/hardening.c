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
        ExpenseList same_day;
        Timestamp date = {2026, 9, 20, 0, 0, 0};
        expense_list_init(&same_day);
        assert(expense_list_add(
            &same_day,
            make_expense(1, 3, 2026, 9, 20, 100, "inactive category")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &same_day,
            make_expense(2, 999, 2026, 9, 20, 200, "unknown category")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_daily_summary(
            &same_day,
            &date,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 2);
        assert(summary.total_paise == 300);
        assert(summary.average_paise == 150);
        assert(summary.average_remainder_paise == 0);
        assert(summary.minimum_paise == 100);
        assert(summary.maximum_paise == 200);
        analytics_summary_destroy(&summary);
        expense_list_destroy(&same_day);
    }
    {
        Timestamp date = {2026, 9, 20, 0, 0, 0};
        Timestamp start = {2026, 9, 20, 0, 0, 0};
        Timestamp end = {2026, 9, 21, 0, 0, 0};

        assert(analytics_calculate_daily_summary(
            &expenses,
            &date,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 1);
        assert(summary.total_paise == 100);
        assert(summary.average_paise == 100);
        assert(summary.average_remainder_paise == 0);
        assert(summary.minimum_paise == 100 && summary.maximum_paise == 100);
        analytics_summary_destroy(&summary);

        date.day = 22;
        assert(analytics_calculate_daily_summary(
            &expenses,
            &date,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 0);
        assert(summary.total_paise == 0);
        assert(summary.minimum_paise == 0 && summary.maximum_paise == 0);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_yearly_summary(
            &expenses,
            2025,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 0);
        assert(summary.total_paise == 0);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_monthly_summary(
            &expenses,
            2026,
            9,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 2);
        assert(summary.total_paise == 10101);
        assert(summary.average_paise == 5050);
        assert(summary.average_remainder_paise == 1);
        assert(summary.minimum_paise == 100);
        assert(summary.maximum_paise == 10001);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_yearly_summary(
            &expenses,
            2026,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 2);
        assert(summary.total_paise == 10101);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_date_range_summary(
            &expenses,
            &start,
            &end,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 2);
        assert(summary.total_paise == 10101);
        assert(summary.minimum_paise == 100);
        assert(summary.maximum_paise == 10001);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_date_range_summary(
            &expenses,
            &start,
            &start,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 1 && summary.total_paise == 100);
        analytics_summary_destroy(&summary);
        assert(analytics_calculate_date_range_summary(
            &expenses,
            &end,
            &end,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 1
            && summary.total_paise == 10001);
        analytics_summary_destroy(&summary);

        assert(analytics_calculate_date_range_summary(
            &expenses,
            &end,
            &start,
            &summary
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_monthly_summary(
            &expenses,
            2026,
            13,
            &summary
        ) == ANALYTICS_INVALID_INPUT);

        date = (Timestamp){2026, 2, 30, 0, 0, 0};
        assert(analytics_calculate_daily_summary(
            &expenses,
            &date,
            &summary
        ) == ANALYTICS_INVALID_INPUT);
    }
    {
        Timestamp leap_day = {2024, 2, 29, 0, 0, 0};
        int original_category_id = expenses.items[0].category_id;

        assert(expense_date_is_valid(2028, 2, 29));
        assert(!expense_date_is_valid(2026, 2, 29));
        assert(expenses.items[0].category_id == 3);
        assert(!category_find_by_id(&categories, 3)->is_active);

        assert(analytics_calculate_daily_summary(
            &expenses,
            &leap_day,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.transaction_count == 1);
        assert(summary.total_paise == 1);
        assert(summary.minimum_paise == 1 && summary.maximum_paise == 1);
        analytics_summary_destroy(&summary);

        expenses.items[0].category_id = 999;
        assert(analytics_calculate_daily_summary(
            &expenses,
            &leap_day,
            &summary
        ) == ANALYTICS_SUCCESS);
        assert(summary.total_paise == 1 && summary.transaction_count == 1);
        analytics_summary_destroy(&summary);
        expenses.items[0].category_id = original_category_id;
    }
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
        ExpenseList breakdown_expenses;
        AnalyticsCategoryBreakdown breakdown;
        AnalyticsPeriod period = {0};
        unsigned int percentage_sum;

        expense_list_init(&breakdown_expenses);
        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(30, 3, 2026, 9, 20, 300, "inactive A")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(31, 2, 2026, 9, 20, 200, "category B")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(32, 3, 2026, 9, 20, 500, "inactive A again")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(33, 2, 2026, 9, 21, 500, "category B next day")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(34, 4, 2025, 12, 31, 2000, "previous year")
        ) == EXPENSE_SUCCESS);

        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            NULL,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 3);
        assert(breakdown.transaction_count == 5);
        assert(breakdown.total_paise == 3500);
        assert(breakdown.items[0].category_id == 3);
        assert(strcmp(breakdown.items[0].category_name, "Petrol") == 0);
        assert(breakdown.items[0].total_paise == 800);
        assert(breakdown.items[0].transaction_count == 2);
        assert(breakdown.items[0].percentage_basis_points == 2286);
        assert(breakdown.items[1].category_id == 2);
        assert(breakdown.items[1].total_paise == 700);
        assert(breakdown.items[1].transaction_count == 2);
        assert(breakdown.items[1].percentage_basis_points == 2000);
        assert(breakdown.items[2].category_id == 4);
        assert(breakdown.items[2].total_paise == 2000);
        assert(breakdown.items[2].transaction_count == 1);
        assert(breakdown.items[2].percentage_basis_points == 5714);
        percentage_sum = breakdown.items[0].percentage_basis_points
            + breakdown.items[1].percentage_basis_points
            + breakdown.items[2].percentage_basis_points;
        assert(percentage_sum == 10000);
        analytics_category_breakdown_destroy(&breakdown);
        assert(breakdown.items == NULL && breakdown.size == 0);

        period.type = ANALYTICS_PERIOD_DAY;
        period.date = (Timestamp){2026, 9, 20, 0, 0, 0};
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 2);
        assert(breakdown.total_paise == 1000);
        assert(breakdown.items[0].total_paise == 800);
        assert(breakdown.items[0].transaction_count == 2);
        assert(breakdown.items[0].percentage_basis_points == 8000);
        assert(breakdown.items[1].total_paise == 200);
        assert(breakdown.items[1].percentage_basis_points == 2000);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_MONTH;
        period.year = 2026;
        period.month = 9;
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.transaction_count == 4);
        assert(breakdown.total_paise == 1500);
        assert(breakdown.items[0].total_paise == 800);
        assert(breakdown.items[1].total_paise == 700);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_YEAR;
        period.year = 2025;
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 1);
        assert(breakdown.items[0].category_id == 4);
        assert(breakdown.items[0].total_paise == 2000);
        assert(breakdown.items[0].percentage_basis_points == 10000);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_DATE_RANGE;
        period.start_date = (Timestamp){2026, 9, 20, 0, 0, 0};
        period.end_date = (Timestamp){2026, 9, 21, 0, 0, 0};
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.transaction_count == 4);
        assert(breakdown.total_paise == 1500);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_DATE_RANGE;
        period.start_date = (Timestamp){2026, 9, 21, 0, 0, 0};
        period.end_date = period.start_date;
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.transaction_count == 1);
        assert(breakdown.total_paise == 500);
        assert(breakdown.items[0].category_id == 2);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_YEAR;
        period.year = 2027;
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 0);
        assert(breakdown.total_paise == 0);
        assert(breakdown.transaction_count == 0);
        analytics_category_breakdown_destroy(&breakdown);

        period.type = ANALYTICS_PERIOD_MONTH;
        period.year = 2026;
        period.month = 13;
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            &period,
            &breakdown
        ) == ANALYTICS_INVALID_INPUT);

        {
            AnalyticsComparison comparison;
            AnalyticsPeriod period_a = {0};
            AnalyticsPeriod period_b = {0};

            period_a.type = ANALYTICS_PERIOD_MONTH;
            period_a.year = 2026;
            period_a.month = 9;
            period_b.type = ANALYTICS_PERIOD_MONTH;
            period_b.year = 2025;
            period_b.month = 12;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.period_a.total_paise == 1500);
            assert(comparison.period_a.transaction_count == 4);
            assert(comparison.period_a.average_paise == 375);
            assert(comparison.period_b.total_paise == 2000);
            assert(comparison.period_b.transaction_count == 1);
            assert(comparison.period_b.average_paise == 2000);
            assert(comparison.absolute_change_paise == 500);
            assert(comparison.percentage_change.state
                == ANALYTICS_PERCENTAGE_DEFINED);
            assert(comparison.percentage_change.is_negative == 0);
            assert(comparison.percentage_change.basis_points == 3333);
            assert(comparison.category_count == 3);
            assert(comparison.categories[0].category_id == 3);
            assert(comparison.categories[0].period_a_total_paise == 800);
            assert(comparison.categories[0].period_b_total_paise == 0);
            assert(comparison.categories[0].absolute_change_paise == -800);
            assert(comparison.categories[0].percentage_change.basis_points
                == 10000);
            assert(comparison.categories[0].percentage_change.is_negative);
            assert(comparison.categories[1].category_id == 2);
            assert(comparison.categories[1].period_a_total_paise == 700);
            assert(comparison.categories[1].period_b_total_paise == 0);
            assert(comparison.categories[2].category_id == 4);
            assert(comparison.categories[2].period_a_total_paise == 0);
            assert(comparison.categories[2].period_b_total_paise == 2000);
            assert(comparison.categories[2].percentage_change.state
                == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
            assert(comparison.categories[2].period_b_percentage_basis_points
                == 10000);
            analytics_comparison_destroy(&comparison);
            assert(comparison.categories == NULL);
            assert(comparison.period_a.category_totals_paise == NULL);
            assert(comparison.period_b.category_totals_paise == NULL);

            period_b = period_a;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.absolute_change_paise == 0);
            assert(comparison.percentage_change.state
                == ANALYTICS_PERCENTAGE_DEFINED);
            assert(comparison.percentage_change.basis_points == 0);
            analytics_comparison_destroy(&comparison);

            period_a.type = ANALYTICS_PERIOD_YEAR;
            period_a.year = 2025;
            period_b.type = ANALYTICS_PERIOD_YEAR;
            period_b.year = 2026;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.period_a.total_paise == 2000);
            assert(comparison.period_b.total_paise == 1500);
            assert(comparison.absolute_change_paise == -500);
            assert(comparison.percentage_change.is_negative);
            assert(comparison.percentage_change.basis_points == 2500);
            analytics_comparison_destroy(&comparison);

            period_a.type = ANALYTICS_PERIOD_DATE_RANGE;
            period_a.start_date = (Timestamp){2026, 9, 20, 0, 0, 0};
            period_a.end_date = period_a.start_date;
            period_b.type = ANALYTICS_PERIOD_DATE_RANGE;
            period_b.start_date = period_a.start_date;
            period_b.end_date = (Timestamp){2026, 9, 21, 0, 0, 0};
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.period_a.total_paise == 1000);
            assert(comparison.period_a.transaction_count == 3);
            assert(comparison.period_b.total_paise == 1500);
            assert(comparison.period_b.transaction_count == 4);
            assert(comparison.absolute_change_paise == 500);
            assert(comparison.percentage_change.basis_points == 5000);
            analytics_comparison_destroy(&comparison);

            period_a.start_date = (Timestamp){2026, 9, 21, 0, 0, 0};
            period_a.end_date = (Timestamp){2026, 9, 20, 0, 0, 0};
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_INVALID_INPUT);

            period_a.type = ANALYTICS_PERIOD_YEAR;
            period_a.year = 2027;
            period_b.type = ANALYTICS_PERIOD_YEAR;
            period_b.year = 2026;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.period_a.transaction_count == 0);
            assert(comparison.period_b.total_paise == 1500);
            assert(comparison.absolute_change_paise == 1500);
            assert(comparison.percentage_change.state
                == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
            analytics_comparison_destroy(&comparison);

            period_b.year = 2028;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.period_a.total_paise == 0);
            assert(comparison.period_b.total_paise == 0);
            assert(comparison.absolute_change_paise == 0);
            assert(comparison.percentage_change.state
                == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
            analytics_comparison_destroy(&comparison);

            period_a.year = 2026;
            period_b.year = 2027;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.absolute_change_paise == -1500);
            assert(comparison.percentage_change.is_negative);
            assert(comparison.percentage_change.basis_points == 10000);
            analytics_comparison_destroy(&comparison);

            period_a.year = 2027;
            assert(analytics_compare_periods(
                &breakdown_expenses,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.categories == NULL);
            assert(comparison.category_count == 0);
            analytics_comparison_destroy(&comparison);

            period_a.year = 2026;
            {
                int original_category_id = breakdown_expenses.items[0]
                    .category_id;
                breakdown_expenses.items[0].category_id = 999;
                assert(analytics_compare_periods(
                    &breakdown_expenses,
                    &categories,
                    &period_a,
                    &period_b,
                    &comparison
                ) == ANALYTICS_INVALID_CATEGORY);
                assert(comparison.categories == NULL);
                breakdown_expenses.items[0].category_id = original_category_id;
            }
        }

        assert(expense_list_add(
            &breakdown_expenses,
            make_expense(35, 999, 2026, 9, 20, 10, "missing category")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_category_breakdown(
            &breakdown_expenses,
            &categories,
            NULL,
            &breakdown
        ) == ANALYTICS_INVALID_CATEGORY);
        assert(breakdown.items == NULL && breakdown.size == 0);

        expense_list_destroy(&breakdown_expenses);
    }
    {
        ExpenseList precision_expenses;
        AnalyticsComparison comparison;
        AnalyticsPeriod period_a = {0};
        AnalyticsPeriod period_b = {0};

        expense_list_init(&precision_expenses);
        assert(expense_list_add(
            &precision_expenses,
            make_expense(40, 2, 2026, 8, 31, 480000, "August")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &precision_expenses,
            make_expense(41, 2, 2026, 9, 27, 620000, "September")
        ) == EXPENSE_SUCCESS);
        period_a.type = ANALYTICS_PERIOD_MONTH;
        period_a.year = 2026;
        period_a.month = 8;
        period_b.type = ANALYTICS_PERIOD_MONTH;
        period_b.year = 2026;
        period_b.month = 9;
        assert(analytics_compare_periods(
            &precision_expenses,
            &categories,
            &period_a,
            &period_b,
            &comparison
        ) == ANALYTICS_SUCCESS);
        assert(comparison.period_a.total_paise == 480000);
        assert(comparison.period_b.total_paise == 620000);
        assert(comparison.absolute_change_paise == 140000);
        assert(comparison.percentage_change.basis_points == 2917);
        analytics_comparison_destroy(&comparison);
        expense_list_destroy(&precision_expenses);
    }
    {
        ExpenseList percentage_overflow_expenses;
        AnalyticsComparison comparison;
        AnalyticsPeriod period_a = {0};
        AnalyticsPeriod period_b = {0};

        expense_list_init(&percentage_overflow_expenses);
        assert(expense_list_add(
            &percentage_overflow_expenses,
            make_expense(42, 2, 2025, 1, 1, 1, "small baseline")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &percentage_overflow_expenses,
            make_expense(43, 2, 2026, 1, 1, INT64_MAX, "large result")
        ) == EXPENSE_SUCCESS);
        period_a.type = ANALYTICS_PERIOD_YEAR;
        period_a.year = 2025;
        period_b.type = ANALYTICS_PERIOD_YEAR;
        period_b.year = 2026;
        assert(analytics_compare_periods(
            &percentage_overflow_expenses,
            &categories,
            &period_a,
            &period_b,
            &comparison
        ) == ANALYTICS_OVERFLOW);
        assert(comparison.categories == NULL);
        assert(comparison.period_a.category_totals_paise == NULL);
        assert(comparison.period_b.category_totals_paise == NULL);
        expense_list_destroy(&percentage_overflow_expenses);
    }
    {
        ExpenseList trend_expenses;
        AnalyticsTrend trend;
        expense_list_init(&trend_expenses);
        assert(expense_list_add(
            &trend_expenses,
            make_expense(50, 2, 2026, 11, 1, 100, "November")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &trend_expenses,
            make_expense(51, 2, 2026, 12, 1, 120, "December part one")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &trend_expenses,
            make_expense(52, 2, 2026, 12, 2, 80, "December part two")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &trend_expenses,
            make_expense(53, 2, 2027, 1, 1, 200, "January")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &trend_expenses,
            make_expense(54, 2, 2027, 3, 1, 50, "March")
        ) == EXPENSE_SUCCESS);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            11,
            5,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.period_count == 5);
        assert(trend.periods[0].year == 2026
            && trend.periods[0].month == 11);
        assert(trend.periods[0].total_paise == 100);
        assert(trend.periods[0].transaction_count == 1);
        assert(trend.periods[0].average_paise == 100);
        assert(trend.periods[1].year == 2026
            && trend.periods[1].month == 12);
        assert(trend.periods[1].total_paise == 200);
        assert(trend.periods[1].transaction_count == 2);
        assert(trend.periods[1].average_paise == 100);
        assert(trend.periods[2].year == 2027
            && trend.periods[2].month == 1);
        assert(trend.periods[2].total_paise == 200);
        assert(trend.periods[3].year == 2027
            && trend.periods[3].month == 2);
        assert(trend.periods[3].total_paise == 0);
        assert(trend.periods[3].transaction_count == 0);
        assert(trend.periods[4].year == 2027
            && trend.periods[4].month == 3);
        assert(trend.periods[4].total_paise == 50);
        assert(trend.total_paise == 550);
        assert(trend.average_period_paise == 110);
        assert(trend.average_period_remainder_paise == 0);
        assert(trend.highest_period_index == 1);
        assert(trend.lowest_period_index == 3);
        assert(trend.increasing_transitions == 2);
        assert(trend.decreasing_transitions == 1);
        assert(trend.unchanged_transitions == 1);
        assert(trend.first_to_last_change_paise == -50);
        assert(trend.first_to_last_percentage_change.is_negative);
        assert(trend.first_to_last_percentage_change.basis_points == 5000);
        analytics_trend_destroy(&trend);
        assert(trend.periods == NULL && trend.period_count == 0);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            11,
            2,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.increasing_transitions == 1);
        assert(trend.decreasing_transitions == 0);
        assert(trend.unchanged_transitions == 0);
        assert(trend.first_to_last_change_paise == 100);
        assert(trend.first_to_last_percentage_change.basis_points == 10000);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            12,
            1,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.period_count == 1);
        assert(trend.periods[0].total_paise == 200);
        assert(trend.total_paise == 200);
        assert(trend.average_period_paise == 200);
        assert(trend.increasing_transitions == 0);
        assert(trend.first_to_last_change_paise == 0);
        assert(trend.first_to_last_percentage_change.basis_points == 0);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_YEARLY,
            2026,
            0,
            3,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.periods[0].year == 2026);
        assert(trend.periods[0].total_paise == 300);
        assert(trend.periods[1].year == 2027);
        assert(trend.periods[1].total_paise == 250);
        assert(trend.periods[2].year == 2028);
        assert(trend.periods[2].total_paise == 0);
        assert(trend.total_paise == 550);
        assert(trend.average_period_paise == 183);
        assert(trend.average_period_remainder_paise == 1);
        assert(trend.highest_period_index == 0);
        assert(trend.lowest_period_index == 2);
        assert(trend.decreasing_transitions == 2);
        assert(trend.first_to_last_change_paise == -300);
        assert(trend.first_to_last_percentage_change.basis_points == 10000);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_YEARLY,
            2026,
            0,
            1,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.period_count == 1);
        assert(trend.periods[0].total_paise == 300);
        assert(trend.increasing_transitions == 0
            && trend.decreasing_transitions == 0
            && trend.unchanged_transitions == 0);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2027,
            1,
            3,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.periods[0].total_paise == 200);
        assert(trend.periods[1].total_paise == 0);
        assert(trend.periods[2].total_paise == 50);
        assert(trend.first_to_last_change_paise == -150);
        assert(trend.first_to_last_percentage_change.is_negative);
        assert(trend.first_to_last_percentage_change.basis_points == 7500);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            0,
            2,
            &trend
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            1,
            0,
            &trend
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            2026,
            1,
            ANALYTICS_MAX_MONTHLY_TREND_PERIODS + 1,
            &trend
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_YEARLY,
            2026,
            1,
            1,
            &trend
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_MONTHLY,
            9999,
            12,
            2,
            &trend
        ) == ANALYTICS_INVALID_INPUT);
        assert(analytics_calculate_trend(
            &trend_expenses,
            ANALYTICS_TREND_YEARLY,
            9999,
            0,
            2,
            &trend
        ) == ANALYTICS_INVALID_INPUT);

        assert(analytics_calculate_trend(
            &empty_expenses,
            ANALYTICS_TREND_YEARLY,
            2020,
            0,
            3,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.total_paise == 0);
        assert(trend.average_period_paise == 0);
        assert(trend.periods[0].transaction_count == 0
            && trend.periods[1].transaction_count == 0
            && trend.periods[2].transaction_count == 0);
        assert(trend.highest_period_index == 0
            && trend.lowest_period_index == 0);
        assert(trend.unchanged_transitions == 2);
        assert(trend.first_to_last_change_paise == 0);
        assert(trend.first_to_last_percentage_change.state
            == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
        analytics_trend_destroy(&trend);
        expense_list_destroy(&trend_expenses);
    }
    {
        ExpenseList trend_overflow_expenses;
        AnalyticsTrend trend;

        expense_list_init(&trend_overflow_expenses);
        assert(expense_list_add(
            &trend_overflow_expenses,
            make_expense(60, 2, 2025, 12, 1, INT64_MAX, "first maximum")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_trend(
            &trend_overflow_expenses,
            ANALYTICS_TREND_YEARLY,
            2025,
            0,
            1,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(trend.total_paise == INT64_MAX);
        analytics_trend_destroy(&trend);
        assert(expense_list_add(
            &trend_overflow_expenses,
            make_expense(61, 2, 2026, 1, 1, 1, "second period")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_trend(
            &trend_overflow_expenses,
            ANALYTICS_TREND_YEARLY,
            2025,
            0,
            2,
            &trend
        ) == ANALYTICS_OVERFLOW);
        assert(trend.periods == NULL && trend.period_count == 0);
        expense_list_destroy(&trend_overflow_expenses);
    }
    {
        CategoryList growth_categories;
        ExpenseList growth_expenses;
        AnalyticsCategoryBreakdown breakdown;

        category_list_init(&growth_categories);
        expense_list_init(&growth_expenses);
        for (int index = 0; index < 5; index++) {
            char name[16];
            Expense expense;

            assert(snprintf(name, sizeof(name), "Group %d", index + 1) > 0);
            assert(category_create(&growth_categories, name)
                == CATEGORY_SUCCESS);
            expense = make_expense(
                index + 1,
                index + 1,
                2026,
                9,
                20,
                100,
                "growth"
            );
            assert(expense_list_add(&growth_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        assert(analytics_calculate_category_breakdown(
            &growth_expenses,
            &growth_categories,
            NULL,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 5 && breakdown.capacity >= 5);
        assert(breakdown.total_paise == 500);
        for (size_t index = 0; index < breakdown.size; index++) {
            assert(breakdown.items[index].transaction_count == 1);
            assert(breakdown.items[index].total_paise == 100);
            assert(breakdown.items[index].percentage_basis_points == 2000);
        }
        analytics_category_breakdown_destroy(&breakdown);
        expense_list_destroy(&growth_expenses);
        category_list_destroy(&growth_categories);
    }

    {
        ExpenseList one_expense;
        AnalyticsCategoryBreakdown breakdown;
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
        {
            Timestamp date = {2026, 9, 20, 0, 0, 0};
            assert(analytics_calculate_daily_summary(
                &one_expense,
                &date,
                &summary
            ) == ANALYTICS_SUCCESS);
            assert(summary.total_paise == INT64_MAX);
            assert(summary.average_paise == INT64_MAX);
            analytics_summary_destroy(&summary);
        }
        {
            AnalyticsComparison comparison;
            AnalyticsPeriod period_a = {0};
            AnalyticsPeriod period_b = {0};

            period_a.type = ANALYTICS_PERIOD_YEAR;
            period_a.year = 2026;
            period_b.type = ANALYTICS_PERIOD_YEAR;
            period_b.year = 2027;
            assert(analytics_compare_periods(
                &one_expense,
                &categories,
                &period_a,
                &period_b,
                &comparison
            ) == ANALYTICS_SUCCESS);
            assert(comparison.absolute_change_paise == -INT64_MAX);
            assert(comparison.percentage_change.is_negative);
            assert(comparison.percentage_change.basis_points == 10000);
            analytics_comparison_destroy(&comparison);
        }
        assert(analytics_calculate_category_breakdown(
            &one_expense,
            &categories,
            NULL,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.size == 1);
        assert(breakdown.total_paise == INT64_MAX);
        assert(breakdown.items[0].total_paise == INT64_MAX);
        assert(breakdown.items[0].percentage_basis_points == 10000);
        analytics_category_breakdown_destroy(&breakdown);

        {
            Expense extra = make_expense(
                21,
                2,
                2026,
                9,
                20,
                1,
                "overflow"
            );
            assert(expense_list_add(&one_expense, extra) == EXPENSE_SUCCESS);
            assert(analytics_calculate_category_breakdown(
                &one_expense,
                &categories,
                NULL,
                &breakdown
            ) == ANALYTICS_OVERFLOW);
            assert(breakdown.items == NULL && breakdown.size == 0);
            {
                Timestamp date = {2026, 9, 20, 0, 0, 0};
                assert(analytics_calculate_daily_summary(
                    &one_expense,
                    &date,
                    &summary
                ) == ANALYTICS_OVERFLOW);
                assert(summary.category_totals_paise == NULL);
            }
        }
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

    {
        CategoryList insight_categories;
        ExpenseList insight_expenses;
        ExpenseList no_expenses;
        ExpenseList invalid_reference_expenses;
        AnalyticsCategoryBreakdown breakdown;
        AnalyticsCategoryInsights category_insights;
        AnalyticsTrend trend;
        AnalyticsPeriodInsights period_insights;
        AnalyticsComparison comparison;
        AnalyticsComparisonInsights comparison_insights;
        AnalyticsPeriod period_a = {0};
        AnalyticsPeriod period_b = {0};
        AnalyticsCategoryTotal overflowing_categories[2] = {0};
        AnalyticsCategoryBreakdown overflowing_breakdown = {0};
        AnalyticsTrendPeriod overflowing_periods[2] = {0};
        AnalyticsTrend overflowing_trend = {0};
        AnalyticsReport report;
        Expense expenses_before[6];
        Category categories_before[3];

        category_list_init(&insight_categories);
        expense_list_init(&insight_expenses);
        expense_list_init(&no_expenses);
        expense_list_init(&invalid_reference_expenses);

        assert(category_create(&insight_categories, "Alpha") == CATEGORY_SUCCESS);
        assert(category_create(&insight_categories, "Beta") == CATEGORY_SUCCESS);
        assert(category_create(&insight_categories, "Gamma") == CATEGORY_SUCCESS);
        assert(category_deactivate(&insight_categories, 3) == CATEGORY_SUCCESS);

        assert(expense_list_add(
            &insight_expenses,
            make_expense(1, 1, 2030, 1, 1, 1000, "Alpha A")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &insight_expenses,
            make_expense(2, 2, 2030, 1, 1, 1000, "Beta A")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &insight_expenses,
            make_expense(3, 3, 2030, 1, 1, 1500, "Gamma A")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &insight_expenses,
            make_expense(4, 1, 2031, 1, 1, 1500, "Alpha B")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &insight_expenses,
            make_expense(5, 2, 2031, 1, 1, 1500, "Beta B")
        ) == EXPENSE_SUCCESS);
        assert(expense_list_add(
            &insight_expenses,
            make_expense(6, 3, 2031, 1, 1, 500, "Gamma B")
        ) == EXPENSE_SUCCESS);
        assert(insight_expenses.size == 6);
        memcpy(
            expenses_before,
            insight_expenses.items,
            sizeof(expenses_before)
        );
        memcpy(
            categories_before,
            insight_categories.items,
            sizeof(categories_before)
        );

        period_a.type = ANALYTICS_PERIOD_ALL_TIME;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 7000);
        assert(report.summary.transaction_count == 6);
        assert(report.summary.average_paise == 1166);
        assert(report.summary.average_remainder_paise == 4);
        assert(report.summary.minimum_paise == 500);
        assert(report.summary.maximum_paise == 1500);
        assert(report.category_breakdown.size == 3);
        assert(report.category_breakdown.total_paise == 7000);
        assert(report.category_breakdown.items[0].category_id == 1);
        assert(report.category_breakdown.items[0].total_paise == 2500);
        assert(report.category_breakdown.items[0].percentage_basis_points
            == 3571);
        assert(report.category_breakdown.items[2].category_id == 3);
        assert(report.category_insights.highest_count == 2);
        assert(report.category_insights.highest_spending[0].category_id == 1);
        assert(report.category_insights.highest_spending[1].category_id == 2);
        assert(memcmp(
            expenses_before,
            insight_expenses.items,
            sizeof(expenses_before)
        ) == 0);
        assert(memcmp(
            categories_before,
            insight_categories.items,
            sizeof(categories_before)
        ) == 0);
        analytics_report_destroy(&report);
        assert(report.summary.category_totals_paise == NULL);
        assert(report.category_breakdown.items == NULL);
        assert(report.category_insights.highest_spending == NULL);

        period_a.type = ANALYTICS_PERIOD_MONTH;
        period_a.year = 2030;
        period_a.month = 1;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 3500);
        assert(report.summary.transaction_count == 3);
        assert(report.summary.average_paise == 1166);
        assert(report.summary.average_remainder_paise == 2);
        assert(report.summary.minimum_paise == 1000);
        assert(report.summary.maximum_paise == 1500);
        assert(report.category_breakdown.size == 3);
        assert(report.category_breakdown.items[2].category_id == 3);
        analytics_report_destroy(&report);

        period_a.type = ANALYTICS_PERIOD_YEAR;
        period_a.year = 2031;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 3500);
        assert(report.summary.transaction_count == 3);
        assert(report.category_breakdown.items[2].total_paise == 500);
        analytics_report_destroy(&report);

        period_a.type = ANALYTICS_PERIOD_DATE_RANGE;
        period_a.start_date.year = 2030;
        period_a.start_date.month = 1;
        period_a.start_date.day = 1;
        period_a.end_date.year = 2030;
        period_a.end_date.month = 12;
        period_a.end_date.day = 31;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 3500);
        assert(report.summary.transaction_count == 3);
        assert(report.category_breakdown.total_paise == 3500);
        analytics_report_destroy(&report);

        period_a.type = ANALYTICS_PERIOD_YEAR;
        period_a.year = 2040;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 0);
        assert(report.summary.transaction_count == 0);
        assert(report.summary.minimum_paise == 0);
        assert(report.summary.maximum_paise == 0);
        assert(report.category_breakdown.size == 0);
        assert(report.category_insights.highest_count == 0);
        analytics_report_destroy(&report);

        period_a.type = ANALYTICS_PERIOD_ALL_TIME;
        assert(analytics_calculate_report(
            &no_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_SUCCESS);
        assert(report.summary.total_paise == 0);
        assert(report.summary.transaction_count == 0);
        assert(report.summary.minimum_paise == 0);
        assert(report.summary.maximum_paise == 0);
        assert(report.category_breakdown.transaction_count == 0);
        analytics_report_destroy(&report);

        period_a.type = ANALYTICS_PERIOD_DATE_RANGE;
        period_a.start_date.year = 2031;
        period_a.start_date.month = 1;
        period_a.start_date.day = 1;
        period_a.end_date.year = 2030;
        period_a.end_date.month = 12;
        period_a.end_date.day = 31;
        assert(analytics_calculate_report(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_INVALID_INPUT);
        assert(report.summary.category_totals_paise == NULL);
        assert(report.category_breakdown.items == NULL);

        assert(memcmp(
            expenses_before,
            insight_expenses.items,
            sizeof(expenses_before)
        ) == 0);
        assert(memcmp(
            categories_before,
            insight_categories.items,
            sizeof(categories_before)
        ) == 0);

        assert(analytics_calculate_category_breakdown(
            &insight_expenses,
            &insight_categories,
            NULL,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_category_insights(
            &breakdown,
            &category_insights
        ) == ANALYTICS_SUCCESS);
        assert(category_insights.total_paise == 7000);
        assert(category_insights.highest_share_state
            == ANALYTICS_PERCENTAGE_DEFINED);
        assert(category_insights.highest_count == 2);
        assert(category_insights.highest_spending[0].category_id == 1);
        assert(category_insights.highest_spending[1].category_id == 2);
        assert(category_insights.highest_spending[0].total_paise == 2500);
        assert(category_insights.highest_spending[0].percentage_basis_points
            == 3571);
        assert(category_insights.lowest_count == 1);
        assert(category_insights.lowest_spending[0].category_id == 3);
        assert(insight_categories.items[2].is_active == 0);
        analytics_category_insights_destroy(&category_insights);
        analytics_category_breakdown_destroy(&breakdown);

        period_a.type = ANALYTICS_PERIOD_YEAR;
        period_a.year = 2030;
        period_b.type = ANALYTICS_PERIOD_YEAR;
        period_b.year = 2031;
        assert(analytics_compare_periods(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &period_b,
            &comparison
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_comparison_insights(
            &comparison,
            &comparison_insights
        ) == ANALYTICS_SUCCESS);
        assert(comparison_insights.largest_absolute_count == 1);
        assert(comparison_insights.largest_absolute_change[0].category_id == 3);
        assert(comparison_insights.largest_absolute_change[0]
            .absolute_change_paise == -1000);
        assert(comparison_insights.largest_increase_count == 2);
        assert(comparison_insights.largest_increase[0].category_id == 1);
        assert(comparison_insights.largest_increase[1].category_id == 2);
        assert(comparison_insights.largest_increase[0]
            .absolute_change_paise == 500);
        assert(comparison_insights.largest_decrease_count == 1);
        assert(comparison_insights.largest_decrease[0].category_id == 3);
        assert(comparison_insights.largest_decrease[0]
            .absolute_change_paise == -1000);
        analytics_comparison_insights_destroy(&comparison_insights);
        analytics_comparison_destroy(&comparison);

        assert(analytics_compare_periods(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &period_a,
            &comparison
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_comparison_insights(
            &comparison,
            &comparison_insights
        ) == ANALYTICS_SUCCESS);
        assert(comparison_insights.largest_absolute_count == 3);
        assert(comparison_insights.largest_increase_count == 0);
        assert(comparison_insights.largest_decrease_count == 0);
        analytics_comparison_insights_destroy(&comparison_insights);
        analytics_comparison_destroy(&comparison);

        assert(analytics_calculate_trend(
            &insight_expenses,
            ANALYTICS_TREND_YEARLY,
            2030,
            0,
            3,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_period_insights(
            &trend,
            &period_insights
        ) == ANALYTICS_SUCCESS);
        assert(period_insights.highest_count == 2);
        assert(period_insights.highest_spending[0].year == 2030);
        assert(period_insights.highest_spending[1].year == 2031);
        assert(period_insights.lowest_count == 1);
        assert(period_insights.lowest_spending[0].year == 2032);
        assert(period_insights.lowest_spending[0].total_paise == 0);
        assert(period_insights.total_paise == 7000);
        assert(period_insights.average_period_paise == 2333);
        assert(period_insights.average_period_remainder_paise == 1);
        analytics_period_insights_destroy(&period_insights);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_trend(
            &no_expenses,
            ANALYTICS_TREND_YEARLY,
            2030,
            0,
            3,
            &trend
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_period_insights(
            &trend,
            &period_insights
        ) == ANALYTICS_SUCCESS);
        assert(period_insights.highest_count == 3);
        assert(period_insights.lowest_count == 3);
        assert(period_insights.total_paise == 0);
        assert(period_insights.average_period_paise == 0);
        analytics_period_insights_destroy(&period_insights);
        analytics_trend_destroy(&trend);

        assert(analytics_calculate_category_breakdown(
            &no_expenses,
            &insight_categories,
            NULL,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_category_insights(
            &breakdown,
            &category_insights
        ) == ANALYTICS_SUCCESS);
        assert(category_insights.highest_count == 0);
        assert(category_insights.lowest_count == 0);
        assert(category_insights.total_paise == 0);
        assert(category_insights.highest_share_state
            == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
        analytics_category_insights_destroy(&category_insights);
        analytics_category_breakdown_destroy(&breakdown);

        period_a.type = ANALYTICS_PERIOD_YEAR;
        period_a.year = 2040;
        assert(analytics_calculate_category_breakdown(
            &insight_expenses,
            &insight_categories,
            &period_a,
            &breakdown
        ) == ANALYTICS_SUCCESS);
        assert(analytics_extract_category_insights(
            &breakdown,
            &category_insights
        ) == ANALYTICS_SUCCESS);
        assert(breakdown.transaction_count == 0);
        assert(category_insights.highest_count == 0);
        assert(category_insights.lowest_count == 0);
        assert(category_insights.highest_share_state
            == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE);
        analytics_category_insights_destroy(&category_insights);
        analytics_category_breakdown_destroy(&breakdown);

        assert(expense_list_add(
            &invalid_reference_expenses,
            make_expense(1, 999, 2030, 1, 1, 1, "invalid category")
        ) == EXPENSE_SUCCESS);
        assert(analytics_calculate_category_breakdown(
            &invalid_reference_expenses,
            &insight_categories,
            NULL,
            &breakdown
        ) == ANALYTICS_INVALID_CATEGORY);
        period_a.type = ANALYTICS_PERIOD_ALL_TIME;
        assert(analytics_calculate_report(
            &invalid_reference_expenses,
            &insight_categories,
            &period_a,
            &report
        ) == ANALYTICS_INVALID_CATEGORY);
        assert(report.summary.category_totals_paise == NULL);
        assert(report.category_breakdown.items == NULL);
        assert(report.category_insights.highest_spending == NULL);

        overflowing_categories[0].category_id = 1;
        strcpy(overflowing_categories[0].category_name, "Alpha");
        overflowing_categories[0].total_paise = INT64_MAX;
        overflowing_categories[0].transaction_count = 1;
        overflowing_categories[1].category_id = 2;
        strcpy(overflowing_categories[1].category_name, "Beta");
        overflowing_categories[1].total_paise = 1;
        overflowing_categories[1].transaction_count = 1;
        overflowing_breakdown.items = overflowing_categories;
        overflowing_breakdown.size = 2;
        overflowing_breakdown.capacity = 2;
        overflowing_breakdown.total_paise = INT64_MAX;
        overflowing_breakdown.transaction_count = 2;
        assert(analytics_extract_category_insights(
            &overflowing_breakdown,
            &category_insights
        ) == ANALYTICS_OVERFLOW);

        overflowing_periods[0].year = 2030;
        overflowing_periods[0].total_paise = INT64_MAX;
        overflowing_periods[0].transaction_count = 1;
        overflowing_periods[0].average_paise = INT64_MAX;
        overflowing_periods[1].year = 2031;
        overflowing_periods[1].total_paise = 1;
        overflowing_periods[1].transaction_count = 1;
        overflowing_periods[1].average_paise = 1;
        overflowing_trend.periods = overflowing_periods;
        overflowing_trend.period_count = 2;
        overflowing_trend.total_paise = INT64_MAX;
        assert(analytics_extract_period_insights(
            &overflowing_trend,
            &period_insights
        ) == ANALYTICS_OVERFLOW);

        analytics_category_insights_destroy(NULL);
        analytics_period_insights_destroy(NULL);
        analytics_comparison_insights_destroy(NULL);
        expense_list_destroy(&invalid_reference_expenses);
        expense_list_destroy(&no_expenses);
        expense_list_destroy(&insight_expenses);
        category_list_destroy(&insight_categories);
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
