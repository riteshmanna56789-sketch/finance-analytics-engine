#include "query.h"

#include <ctype.h>
#include <time.h>

static int query_date_is_valid(QueryDate date)
{
    return expense_date_is_valid(date.year, date.month, date.day);
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

static int matches_time(
    const Timestamp *timestamp,
    QueryTimeFilter filter,
    const struct tm *current_time,
    QueryDate week_start,
    QueryDate following_week_start
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
        QueryDate date = {timestamp->year, timestamp->month, timestamp->day};
        return compare_dates(date, week_start) >= 0
            && compare_dates(date, following_week_start) < 0;
    }
}

static QueryResult validate_query_inputs(
    const ExpenseList *expenses,
    QueryMatchCallback callback
)
{
    if (expenses == NULL || callback == NULL
        || expenses->size > expenses->capacity
        || (expenses->size > 0 && expenses->items == NULL)) {
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
    QueryDate week_start;
    QueryDate following_week_start;
    struct tm *current_time;
    struct tm week_date;
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

    week_date = *current_time;
    week_date.tm_mday -= week_date.tm_wday;
    week_date.tm_hour = 12;
    week_date.tm_min = 0;
    week_date.tm_sec = 0;
    week_date.tm_isdst = -1;
    if (mktime(&week_date) == (time_t)-1) {
        return QUERY_TIME_ERROR;
    }
    week_start = (QueryDate){
        week_date.tm_year + 1900,
        week_date.tm_mon + 1,
        week_date.tm_mday
    };

    week_date.tm_mday += 7;
    if (mktime(&week_date) == (time_t)-1) {
        return QUERY_TIME_ERROR;
    }
    following_week_start = (QueryDate){
        week_date.tm_year + 1900,
        week_date.tm_mon + 1,
        week_date.tm_mday
    };

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
