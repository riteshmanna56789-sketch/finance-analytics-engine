#include "test_support.h"

#include "query.h"

#include <time.h>

static void count_match(const Expense *expense, void *context)
{
    (void)expense;
    (*(size_t *)context)++;
}

static void set_expense_date(Expense *expense, const struct tm *date)
{
    expense->timestamp.year = date->tm_year + 1900;
    expense->timestamp.month = date->tm_mon + 1;
    expense->timestamp.day = date->tm_mday;
}

void test_query(void)
{
    CategoryList categories;
    ExpenseList expenses;
    size_t matches = 0;

    test_init_categories(&categories);
    expense_list_init(&expenses);
    assert(expense_list_add(&expenses,
        test_make_expense(1, 3, 2024, 2, 29, 1, "leap")) == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(2, 3, 2026, 9, 20, 50000,
            "Lunch at restaurant")) == EXPENSE_SUCCESS);
    assert(expense_list_add(&expenses,
        test_make_expense(3, 2, 2026, 9, 21, 10001, "bus"))
        == EXPENSE_SUCCESS);
    assert(expense_update(&expenses, &categories, 2, INT64_MAX, 4, "updated")
        == EXPENSE_SUCCESS);

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

    {
        ExpenseList amount_expenses;
        size_t amount_matches = 0;

        expense_list_init(&amount_expenses);
        assert(expense_list_add(&amount_expenses,
            test_make_expense(10, 2, 2026, 1, 1, 1, "minimum"))
            == EXPENSE_SUCCESS);
        assert(expense_list_add(&amount_expenses,
            test_make_expense(11, 2, 2026, 1, 2, 250, "match"))
            == EXPENSE_SUCCESS);
        assert(expense_list_add(&amount_expenses,
            test_make_expense(12, 2, 2026, 1, 3, 251, "above"))
            == EXPENSE_SUCCESS);

        assert(query_by_amount(
            &amount_expenses,
            250,
            count_match,
            &amount_matches
        ) == QUERY_SUCCESS && amount_matches == 1);
        amount_matches = 0;
        assert(query_by_amount(
            &amount_expenses,
            249,
            count_match,
            &amount_matches
        ) == QUERY_SUCCESS && amount_matches == 0);
        assert(query_by_amount(
            &amount_expenses,
            1,
            count_match,
            &amount_matches
        ) == QUERY_SUCCESS && amount_matches == 1);
        assert(query_by_amount(
            &amount_expenses,
            0,
            count_match,
            &amount_matches
        ) == QUERY_INVALID_INPUT);

        expense_list_destroy(&amount_expenses);
        expense_list_init(&amount_expenses);
        amount_matches = 0;
        assert(query_by_amount(
            &amount_expenses,
            250,
            count_match,
            &amount_matches
        ) == QUERY_SUCCESS && amount_matches == 0);
        expense_list_destroy(&amount_expenses);
    }

    {
        ExpenseList today_expenses;
        ExpenseList week_expenses;
        ExpenseList empty_time_expenses;
        time_t now = time(NULL);
        struct tm *current_time = localtime(&now);
        struct tm today;
        struct tm week_start;
        struct tm following_week_start;
        struct tm week_before;
        struct tm last_week_day;
        size_t time_matches = 0;

        assert(current_time != NULL);
        today = *current_time;
        week_start = *current_time;
        week_start.tm_mday -= week_start.tm_wday;
        week_start.tm_hour = 12;
        week_start.tm_min = 0;
        week_start.tm_sec = 0;
        week_start.tm_isdst = -1;
        assert(mktime(&week_start) != (time_t)-1);
        following_week_start = week_start;
        following_week_start.tm_mday += 7;
        assert(mktime(&following_week_start) != (time_t)-1);
        week_before = week_start;
        week_before.tm_mday -= 1;
        assert(mktime(&week_before) != (time_t)-1);
        last_week_day = following_week_start;
        last_week_day.tm_mday -= 1;
        assert(mktime(&last_week_day) != (time_t)-1);

        expense_list_init(&today_expenses);
        expense_list_init(&week_expenses);
        expense_list_init(&empty_time_expenses);
        {
            Expense expense = test_make_expense(
                20, 2, today.tm_year + 1900, today.tm_mon + 1, today.tm_mday,
                100, "today start"
            );
            expense.timestamp.hour = 0;
            expense.timestamp.minute = 0;
            expense.timestamp.second = 0;
            assert(expense_list_add(&today_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        {
            Expense expense = test_make_expense(
                21, 2, today.tm_year + 1900, today.tm_mon + 1, today.tm_mday,
                200, "today end"
            );
            expense.timestamp.hour = 23;
            expense.timestamp.minute = 59;
            expense.timestamp.second = 59;
            assert(expense_list_add(&today_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        {
            Expense expense = test_make_expense(
                22, 2, week_before.tm_year + 1900, week_before.tm_mon + 1,
                week_before.tm_mday, 300, "before week"
            );
            set_expense_date(&expense, &week_before);
            assert(expense_list_add(&week_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        {
            Expense expense = test_make_expense(
                23, 2, week_start.tm_year + 1900, week_start.tm_mon + 1,
                week_start.tm_mday, 400, "week start"
            );
            set_expense_date(&expense, &week_start);
            assert(expense_list_add(&week_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        {
            Expense expense = test_make_expense(
                24, 2, last_week_day.tm_year + 1900, last_week_day.tm_mon + 1,
                last_week_day.tm_mday, 500, "last week day"
            );
            set_expense_date(&expense, &last_week_day);
            assert(expense_list_add(&week_expenses, expense)
                == EXPENSE_SUCCESS);
        }
        {
            Expense expense = test_make_expense(
                25, 2, following_week_start.tm_year + 1900,
                following_week_start.tm_mon + 1, following_week_start.tm_mday,
                600, "following week"
            );
            set_expense_date(&expense, &following_week_start);
            assert(expense_list_add(&week_expenses, expense)
                == EXPENSE_SUCCESS);
        }

        assert(query_by_time(
            &today_expenses,
            QUERY_TODAY,
            count_match,
            &time_matches
        ) == QUERY_SUCCESS && time_matches == 2);
        time_matches = 0;
        assert(query_by_time(
            &week_expenses,
            QUERY_THIS_WEEK,
            count_match,
            &time_matches
        ) == QUERY_SUCCESS && time_matches == 2);
        time_matches = 0;
        assert(query_by_time(
            &today_expenses,
            QUERY_THIS_MONTH,
            count_match,
            &time_matches
        ) == QUERY_SUCCESS && time_matches >= 2);
        time_matches = 0;
        assert(query_by_time(
            &today_expenses,
            QUERY_THIS_YEAR,
            count_match,
            &time_matches
        ) == QUERY_SUCCESS && time_matches >= 2);
        time_matches = 0;
        assert(query_by_time(
            &empty_time_expenses,
            QUERY_THIS_WEEK,
            count_match,
            &time_matches
        ) == QUERY_SUCCESS && time_matches == 0);
        assert(query_by_time(
            &week_expenses,
            (QueryTimeFilter)0,
            count_match,
            &time_matches
        ) == QUERY_INVALID_INPUT);

        expense_list_destroy(&empty_time_expenses);
        expense_list_destroy(&week_expenses);
        expense_list_destroy(&today_expenses);
    }

    expense_list_destroy(&expenses);
    category_list_destroy(&categories);
}
