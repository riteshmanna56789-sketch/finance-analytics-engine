#include "query.h"

#include <time.h>

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
