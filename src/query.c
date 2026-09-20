#include "query.h"

#include <ctype.h>
#include <time.h>

static int query_date_is_valid(QueryDate date)
{
    int days_in_month;

    if (date.year < 1 || date.month < 1 || date.month > 12 || date.day < 1) {
        return 0;
    }

    days_in_month = 31;
    if (date.month == 4 || date.month == 6
        || date.month == 9 || date.month == 11) {
        days_in_month = 30;
    } else if (date.month == 2) {
        int leap_year = date.year % 400 == 0
            || (date.year % 4 == 0 && date.year % 100 != 0);
        days_in_month = leap_year ? 29 : 28;
    }

    return date.day <= days_in_month;
}

static int compare_dates(QueryDate left, QueryDate right)
{
    if (left.year != right.year) {
        return left.year < right.year ? -1 : 1;
    }
    if (left.month != right.month) {
        return left.month < right.month ? -1 : 1;
    }
    if (left.day != right.day) {
        return left.day < right.day ? -1 : 1;
    }
    return 0;
}

static int note_contains(const char *note, const char *needle)
{
    if (*needle == '\0') {
        return *note == '\0';
    }

    while (*note != '\0') {
        const char *left = note;
        const char *right = needle;

        while (*left != '\0' && *right != '\0'
            && tolower((unsigned char)*left)
                == tolower((unsigned char)*right)) {
            left++;
            right++;
        }

        if (*right == '\0') {
            return 1;
        }
        note++;
    }

    return 0;
}

static QueryDate expense_date(const Expense *expense)
{
    QueryDate date = {
        expense->timestamp.year,
        expense->timestamp.month,
        expense->timestamp.day
    };
    return date;
}

static int matches_filter(
    const Expense *expense,
    const QueryFilter *filter
)
{
    if (filter->category_enabled
        && expense->category_id != filter->category_id) {
        return 0;
    }
    if (filter->amount_enabled
        && (expense->amount_paise < filter->minimum_amount_paise
            || expense->amount_paise > filter->maximum_amount_paise)) {
        return 0;
    }
    if (filter->date_enabled
        && (compare_dates(expense_date(expense), filter->start_date) < 0
            || compare_dates(expense_date(expense), filter->end_date) > 0)) {
        return 0;
    }
    if (filter->note_enabled && !note_contains(expense->note, filter->note)) {
        return 0;
    }
    return 1;
}

static int same_date(const Timestamp *left, const struct tm *right)
{
    return left->year == right->tm_year + 1900
        && left->month == right->tm_mon + 1
        && left->day == right->tm_mday;
}

static time_t timestamp_date(const Timestamp *timestamp)
{
    struct tm date = {0};

    date.tm_year = timestamp->year - 1900;
    date.tm_mon = timestamp->month - 1;
    date.tm_mday = timestamp->day;
    date.tm_hour = 12;
    date.tm_isdst = -1;
    return mktime(&date);
}

static time_t next_week_start(const struct tm *current_time)
{
    struct tm date = *current_time;

    date.tm_hour = 12;
    date.tm_min = 0;
    date.tm_sec = 0;
    date.tm_mday += 7 - date.tm_wday;
    date.tm_isdst = -1;
    return mktime(&date);
}

static int matches_time(
    const Timestamp *timestamp,
    QueryTimeFilter filter,
    const struct tm *current_time,
    time_t week_start,
    time_t next_week_start
)
{
    if (filter == QUERY_TODAY) {
        return same_date(timestamp, current_time);
    }
    if (filter == QUERY_THIS_MONTH) {
        return timestamp->year == current_time->tm_year + 1900
            && timestamp->month == current_time->tm_mon + 1;
    }
    if (filter == QUERY_THIS_YEAR) {
        return timestamp->year == current_time->tm_year + 1900;
    }

    {
        time_t expense_date = timestamp_date(timestamp);
        return expense_date >= week_start && expense_date < next_week_start;
    }
}

static QueryResult validate_query_inputs(
    const ExpenseList *expenses,
    QueryMatchCallback callback
)
{
    if (expenses == NULL || callback == NULL) {
        return QUERY_INVALID_INPUT;
    }

    return QUERY_SUCCESS;
}

QueryResult query_by_category(
    const ExpenseList *expenses,
    int category_id,
    QueryMatchCallback callback,
    void *context
)
{
    QueryResult result = validate_query_inputs(expenses, callback);

    if (result != QUERY_SUCCESS || category_id < 1) {
        return QUERY_INVALID_INPUT;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        if (expenses->items[index].category_id == category_id) {
            callback(&expenses->items[index], context);
        }
    }

    return QUERY_SUCCESS;
}

QueryResult query_by_amount(
    const ExpenseList *expenses,
    int64_t amount_paise,
    QueryMatchCallback callback,
    void *context
)
{
    QueryResult result = validate_query_inputs(expenses, callback);

    if (result != QUERY_SUCCESS || amount_paise <= 0) {
        return QUERY_INVALID_INPUT;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        if (expenses->items[index].amount_paise == amount_paise) {
            callback(&expenses->items[index], context);
        }
    }

    return QUERY_SUCCESS;
}

QueryResult query_by_time(
    const ExpenseList *expenses,
    QueryTimeFilter filter,
    QueryMatchCallback callback,
    void *context
)
{
    time_t now;
    time_t week_start;
    time_t following_week_start;
    struct tm *current_time;
    QueryResult result = validate_query_inputs(expenses, callback);

    if (result != QUERY_SUCCESS
        || filter < QUERY_TODAY || filter > QUERY_THIS_YEAR) {
        return QUERY_INVALID_INPUT;
    }

    now = time(NULL);
    current_time = localtime(&now);
    if (current_time == NULL) {
        return QUERY_TIME_ERROR;
    }

    week_start = timestamp_date(&(Timestamp){
        current_time->tm_year + 1900,
        current_time->tm_mon + 1,
        current_time->tm_mday - current_time->tm_wday,
        0,
        0,
        0
    });
    following_week_start = next_week_start(current_time);

    for (size_t index = 0; index < expenses->size; index++) {
        if (matches_time(
                &expenses->items[index].timestamp,
                filter,
                current_time,
                week_start,
                following_week_start
            )) {
            callback(&expenses->items[index], context);
        }
    }

    return QUERY_SUCCESS;
}

QueryResult query_by_note(
    const ExpenseList *expenses,
    const char *note,
    QueryMatchCallback callback,
    void *context
)
{
    QueryFilter filter = {0};

    if (note == NULL) {
        return QUERY_INVALID_INPUT;
    }

    filter.note_enabled = 1;
    filter.note = note;
    return query_expenses(expenses, &filter, callback, context);
}

QueryResult query_by_amount_range(
    const ExpenseList *expenses,
    int64_t minimum_amount_paise,
    int64_t maximum_amount_paise,
    QueryMatchCallback callback,
    void *context
)
{
    QueryFilter filter = {0};

    if (minimum_amount_paise < 0
        || maximum_amount_paise < 0
        || minimum_amount_paise > maximum_amount_paise) {
        return QUERY_INVALID_INPUT;
    }

    filter.amount_enabled = 1;
    filter.minimum_amount_paise = minimum_amount_paise;
    filter.maximum_amount_paise = maximum_amount_paise;
    return query_expenses(expenses, &filter, callback, context);
}

QueryResult query_by_date_range(
    const ExpenseList *expenses,
    QueryDate start_date,
    QueryDate end_date,
    QueryMatchCallback callback,
    void *context
)
{
    QueryFilter filter = {0};

    if (!query_date_is_valid(start_date)
        || !query_date_is_valid(end_date)
        || compare_dates(start_date, end_date) > 0) {
        return QUERY_INVALID_INPUT;
    }

    filter.date_enabled = 1;
    filter.start_date = start_date;
    filter.end_date = end_date;
    return query_expenses(expenses, &filter, callback, context);
}

QueryResult query_expenses(
    const ExpenseList *expenses,
    const QueryFilter *filter,
    QueryMatchCallback callback,
    void *context
)
{
    QueryResult result = validate_query_inputs(expenses, callback);

    if (result != QUERY_SUCCESS || filter == NULL) {
        return QUERY_INVALID_INPUT;
    }
    if (filter->category_enabled && filter->category_id < 1) {
        return QUERY_INVALID_INPUT;
    }
    if (filter->amount_enabled
        && (filter->minimum_amount_paise < 0
            || filter->maximum_amount_paise < 0
            || filter->minimum_amount_paise > filter->maximum_amount_paise)) {
        return QUERY_INVALID_INPUT;
    }
    if (filter->date_enabled
        && (!query_date_is_valid(filter->start_date)
            || !query_date_is_valid(filter->end_date)
            || compare_dates(filter->start_date, filter->end_date) > 0)) {
        return QUERY_INVALID_INPUT;
    }
    if (filter->note_enabled && filter->note == NULL) {
        return QUERY_INVALID_INPUT;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        if (matches_filter(&expenses->items[index], filter)) {
            callback(&expenses->items[index], context);
        }
    }

    return QUERY_SUCCESS;
}
