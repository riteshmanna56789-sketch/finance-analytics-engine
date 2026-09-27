#include "analytics_ui.h"

#include "analytics.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_input(char *buffer, size_t buffer_size)
{
    size_t length;
    int character;
    int read_anything = 0;
    int too_long = 0;

    if (buffer == NULL || buffer_size < 2) {
        return -1;
    }

    length = 0;
    while ((character = getchar()) != EOF && character != '\n') {
        read_anything = 1;
        if (character == '\0' || length + 1 >= buffer_size) {
            too_long = 1;
        } else if (!too_long) {
            buffer[length++] = (char)character;
        }
    }

    if (ferror(stdin)) {
        return 0;
    }
    if (!read_anything && character == EOF) {
        return 0;
    }

    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[--length] = '\0';
    }
    buffer[length] = '\0';
    return too_long ? -1 : 1;
}

static int read_integer(const char *prompt, int *value)
{
    char input[128];
    char *end;
    long parsed;
    int result;

    printf("%s", prompt);
    result = read_input(input, sizeof(input));
    if (result != 1) {
        return 0;
    }

    errno = 0;
    parsed = strtol(input, &end, 10);
    if (input == end || errno == ERANGE
        || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int read_date(const char *prompt, Timestamp *date)
{
    char input[32];
    int result;

    printf("%s", prompt);
    result = read_input(input, sizeof(input));
    if (result != 1 || strlen(input) != 10
        || input[4] != '-' || input[7] != '-') {
        return 0;
    }

    for (size_t index = 0; index < 10; index++) {
        if (index != 4 && index != 7
            && (input[index] < '0' || input[index] > '9')) {
            return 0;
        }
    }

    date->year = (input[0] - '0') * 1000
        + (input[1] - '0') * 100
        + (input[2] - '0') * 10
        + input[3] - '0';
    date->month = (input[5] - '0') * 10 + input[6] - '0';
    date->day = (input[8] - '0') * 10 + input[9] - '0';
    date->hour = 0;
    date->minute = 0;
    date->second = 0;

    return expense_date_is_valid(date->year, date->month, date->day);
}

static void print_amount(int64_t amount_paise)
{
    printf(
        "Rs.%lld.%02lld",
        (long long)(amount_paise / 100),
        (long long)(amount_paise % 100)
    );
}

static void print_summary_values(
    const char *title,
    const AnalyticsSummary *summary
)
{
    printf("\n%s\n", title);
    printf("Transactions: %zu\n", summary->transaction_count);
    printf("Total: ");
    print_amount(summary->total_paise);
    printf("\n");

    if (summary->transaction_count == 0) {
        printf("No transactions found for this period.\n");
        return;
    }

    printf("Average: ");
    print_amount(summary->average_paise);
    if (summary->average_remainder_paise != 0) {
        printf(
            " + %" PRId64 "/%zu paise",
            summary->average_remainder_paise,
            summary->transaction_count
        );
    }
    printf("\nMinimum: ");
    print_amount(summary->minimum_paise);
    printf("\nMaximum: ");
    print_amount(summary->maximum_paise);
    printf("\n");
}

static void report_result(
    AnalyticsResult result,
    const char *title,
    AnalyticsSummary *summary
)
{
    if (result == ANALYTICS_SUCCESS) {
        print_summary_values(title, summary);
        analytics_summary_destroy(summary);
    } else if (result == ANALYTICS_OVERFLOW) {
        printf("Unable to calculate this summary: total overflow.\n");
    } else if (result == ANALYTICS_MEMORY_ERROR) {
        printf("Unable to calculate this summary: memory allocation failed.\n");
    } else {
        printf("Invalid period or expense data.\n");
    }
}

static void show_overall_summary(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    AnalyticsSummary summary;
    AnalyticsResult result = analytics_calculate_summary(
        expenses,
        categories,
        &summary
    );

    if (result == ANALYTICS_INVALID_CATEGORY) {
        printf(
            "Unable to calculate the summary because an expense references "
            "an unknown category.\n"
        );
        return;
    }
    if (result != ANALYTICS_SUCCESS) {
        report_result(result, "Overall Spending Summary", &summary);
        return;
    }

    print_summary_values("Overall Spending Summary", &summary);
    printf("\nToday: ");
    print_amount(summary.today_paise);
    printf("\nThis Month: ");
    print_amount(summary.current_month_paise);
    printf("\n\nBy Category:\n");

    if (expenses->size == 0) {
        printf("No expenses recorded.\n");
    } else {
        for (size_t index = 0; index < summary.category_count; index++) {
            if (summary.category_totals_paise[index] > 0) {
                printf("%-18s ", categories->items[index].name);
                print_amount(summary.category_totals_paise[index]);
                printf("\n");
            }
        }
    }

    analytics_summary_destroy(&summary);
}

static int select_category_period(AnalyticsPeriod *period)
{
    int choice;

    printf("\nCategory Breakdown Period\n");
    printf("[1] All Time\n");
    printf("[2] Specific Day\n");
    printf("[3] Specific Month\n");
    printf("[4] Specific Year\n");
    printf("[5] Custom Date Range\n");

    if (!read_integer("Choice (1-5): ", &choice)
        || choice < 1 || choice > 5) {
        printf("Invalid period selection.\n");
        return 0;
    }

    if (choice == 1) {
        period->type = ANALYTICS_PERIOD_ALL_TIME;
        return 1;
    }
    if (choice == 2) {
        if (!read_date("Date (YYYY-MM-DD): ", &period->date)) {
            printf("Invalid date. Use a valid date in YYYY-MM-DD format.\n");
            return 0;
        }
        period->type = ANALYTICS_PERIOD_DAY;
        return 1;
    }
    if (choice == 3) {
        if (!read_integer("Year (1-9999): ", &period->year)
            || period->year < 1 || period->year > 9999
            || !read_integer("Month (1-12): ", &period->month)
            || period->month < 1 || period->month > 12) {
            printf("Invalid year or month.\n");
            return 0;
        }
        period->type = ANALYTICS_PERIOD_MONTH;
        return 1;
    }
    if (choice == 4) {
        if (!read_integer("Year (1-9999): ", &period->year)
            || period->year < 1 || period->year > 9999) {
            printf("Invalid year.\n");
            return 0;
        }
        period->type = ANALYTICS_PERIOD_YEAR;
        return 1;
    }

    if (!read_date("Start date (YYYY-MM-DD): ", &period->start_date)
        || !read_date("End date (YYYY-MM-DD): ", &period->end_date)) {
        printf("Invalid date. Use valid dates in YYYY-MM-DD format.\n");
        return 0;
    }
    period->type = ANALYTICS_PERIOD_DATE_RANGE;
    return 1;
}

static void show_category_breakdown(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    AnalyticsPeriod period = {0};
    AnalyticsCategoryBreakdown breakdown;
    AnalyticsResult result;
    const AnalyticsPeriod *selected_period = &period;

    if (!select_category_period(&period)) {
        return;
    }

    result = analytics_calculate_category_breakdown(
        expenses,
        categories,
        selected_period,
        &breakdown
    );
    if (result == ANALYTICS_INVALID_CATEGORY) {
        printf(
            "Unable to calculate category breakdown: an expense references "
            "an unknown category.\n"
        );
        return;
    }
    if (result == ANALYTICS_OVERFLOW) {
        printf("Unable to calculate category breakdown: total overflow.\n");
        return;
    }
    if (result == ANALYTICS_MEMORY_ERROR) {
        printf(
            "Unable to calculate category breakdown: memory allocation "
            "failed.\n"
        );
        return;
    }
    if (result != ANALYTICS_SUCCESS) {
        printf("Unable to calculate category breakdown: invalid input.\n");
        return;
    }

    printf("\nCategory Breakdown\n");
    if (breakdown.transaction_count == 0) {
        printf("No transactions found for this period.\n");
        analytics_category_breakdown_destroy(&breakdown);
        return;
    }

    printf("%-20s %-14s %12s %12s\n",
        "Category", "Transactions", "Total", "Share");
    printf("---------------------------------------------------------------\n");
    for (size_t index = 0; index < breakdown.size; index++) {
        const AnalyticsCategoryTotal *category = &breakdown.items[index];

        printf(
            "%-20s %-14zu ",
            category->category_name,
            category->transaction_count
        );
        print_amount(category->total_paise);
        printf(
            " %5u.%02u%%\n",
            category->percentage_basis_points / 100,
            category->percentage_basis_points % 100
        );
    }
    printf("---------------------------------------------------------------\n");
    printf("%-36s ", "Total");
    print_amount(breakdown.total_paise);
    printf("\n");
    analytics_category_breakdown_destroy(&breakdown);
}

static void print_category_insight_group(
    const char *label,
    const AnalyticsCategoryTotal *categories,
    size_t count,
    int show_share
)
{
    printf("%s", label);
    if (count > 1) {
        printf(" (tie):\n");
    } else if (count == 1) {
        printf(":\n");
    } else {
        printf(": none\n");
        return;
    }

    for (size_t index = 0; index < count; index++) {
        const AnalyticsCategoryTotal *category = &categories[index];
        printf("  %s (ID %d) - ", category->category_name, category->category_id);
        print_amount(category->total_paise);
        if (show_share) {
            printf(
                " - %u.%02u%% of total",
                category->percentage_basis_points / 100,
                category->percentage_basis_points % 100
            );
        }
        printf("\n");
    }
}

static void show_category_insights(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    AnalyticsPeriod period = {0};
    AnalyticsCategoryBreakdown breakdown;
    AnalyticsCategoryInsights insights;
    AnalyticsResult result;

    if (!select_category_period(&period)) {
        return;
    }

    result = analytics_calculate_category_breakdown(
        expenses,
        categories,
        &period,
        &breakdown
    );
    if (result != ANALYTICS_SUCCESS) {
        if (result == ANALYTICS_INVALID_CATEGORY) {
            printf("Unable to calculate insights: an expense references an unknown category.\n");
        } else if (result == ANALYTICS_MEMORY_ERROR) {
            printf("Unable to calculate category insights: memory allocation failed.\n");
        } else if (result == ANALYTICS_OVERFLOW) {
            printf("Unable to calculate category insights: total overflow.\n");
        } else {
            printf("Unable to calculate category insights: invalid input.\n");
        }
        return;
    }

    result = analytics_extract_category_insights(&breakdown, &insights);
    if (result != ANALYTICS_SUCCESS) {
        analytics_category_breakdown_destroy(&breakdown);
        printf(
            result == ANALYTICS_MEMORY_ERROR
                ? "Unable to calculate category insights: memory allocation failed.\n"
                : "Unable to calculate category insights: invalid analytics result.\n"
        );
        return;
    }

    printf("\nCategory Insights\n");
    if (insights.highest_count == 0) {
        printf("No category data for the selected period.\n");
        printf("Highest category share: undefined because spending is zero.\n");
    } else {
        print_category_insight_group(
            "Highest-spending category",
            insights.highest_spending,
            insights.highest_count,
            1
        );
        print_category_insight_group(
            "Lowest-spending category among categories with transactions",
            insights.lowest_spending,
            insights.lowest_count,
            0
        );
    }

    analytics_category_insights_destroy(&insights);
    analytics_category_breakdown_destroy(&breakdown);
}

static void show_daily_summary(const ExpenseList *expenses)
{
    AnalyticsSummary summary;
    Timestamp date = {0};
    char title[64];

    if (!read_date("Date (YYYY-MM-DD): ", &date)) {
        printf("Invalid date. Use a valid date in YYYY-MM-DD format.\n");
        return;
    }

    (void)snprintf(
        title,
        sizeof(title),
        "Daily Spending - %04d-%02d-%02d",
        date.year,
        date.month,
        date.day
    );
    report_result(
        analytics_calculate_daily_summary(expenses, &date, &summary),
        title,
        &summary
    );
}

static void show_monthly_summary(const ExpenseList *expenses)
{
    AnalyticsSummary summary;
    char title[64];
    int year;
    int month;

    if (!read_integer("Year (1-9999): ", &year) || year < 1 || year > 9999
        || !read_integer("Month (1-12): ", &month)
        || month < 1 || month > 12) {
        printf("Invalid year or month.\n");
        return;
    }

    (void)snprintf(title, sizeof(title), "Monthly Spending - %04d-%02d",
        year, month);
    report_result(
        analytics_calculate_monthly_summary(
            expenses,
            year,
            month,
            &summary
        ),
        title,
        &summary
    );
}

static void show_yearly_summary(const ExpenseList *expenses)
{
    AnalyticsSummary summary;
    char title[64];
    int year;

    if (!read_integer("Year (1-9999): ", &year) || year < 1 || year > 9999) {
        printf("Invalid year.\n");
        return;
    }

    (void)snprintf(title, sizeof(title), "Yearly Spending - %04d", year);
    report_result(
        analytics_calculate_yearly_summary(expenses, year, &summary),
        title,
        &summary
    );
}

static void show_date_range_summary(const ExpenseList *expenses)
{
    AnalyticsSummary summary;
    Timestamp start_date = {0};
    Timestamp end_date = {0};
    char title[96];

    if (!read_date("Start date (YYYY-MM-DD): ", &start_date)
        || !read_date("End date (YYYY-MM-DD): ", &end_date)) {
        printf("Invalid date. Use valid dates in YYYY-MM-DD format.\n");
        return;
    }

    (void)snprintf(
        title,
        sizeof(title),
        "Date Range Spending - %04d-%02d-%02d to %04d-%02d-%02d",
        start_date.year,
        start_date.month,
        start_date.day,
        end_date.year,
        end_date.month,
        end_date.day
    );
    report_result(
        analytics_calculate_date_range_summary(
            expenses,
            &start_date,
            &end_date,
            &summary
        ),
        title,
        &summary
    );
}

static int read_comparison_period(
    int comparison_type,
    const char *label,
    AnalyticsPeriod *period
)
{
    if (comparison_type == 1) {
        period->type = ANALYTICS_PERIOD_MONTH;
        printf("%s\n", label);
        return read_integer("Year (1-9999): ", &period->year)
            && period->year >= 1 && period->year <= 9999
            && read_integer("Month (1-12): ", &period->month)
            && period->month >= 1 && period->month <= 12;
    }

    if (comparison_type == 2) {
        period->type = ANALYTICS_PERIOD_YEAR;
        printf("%s\n", label);
        return read_integer("Year (1-9999): ", &period->year)
            && period->year >= 1 && period->year <= 9999;
    }

    period->type = ANALYTICS_PERIOD_DATE_RANGE;
    printf("%s\n", label);
    if (!read_date("Start date (YYYY-MM-DD): ", &period->start_date)
        || !read_date("End date (YYYY-MM-DD): ", &period->end_date)) {
        return 0;
    }
    return period->start_date.year < period->end_date.year
        || (period->start_date.year == period->end_date.year
            && (period->start_date.month < period->end_date.month
                || (period->start_date.month == period->end_date.month
                    && period->start_date.day <= period->end_date.day)));
}

static void print_signed_amount(int64_t amount_paise)
{
    if (amount_paise < 0) {
        printf("-");
        print_amount(-amount_paise);
    } else {
        printf("+");
        print_amount(amount_paise);
    }
}

static void print_percentage_change(
    const AnalyticsPercentageChange *percentage
)
{
    if (percentage->state
        == ANALYTICS_PERCENTAGE_UNDEFINED_ZERO_BASELINE) {
        printf("N/A (zero baseline)");
        return;
    }

    printf(
        "%s%" PRIu64 ".%02" PRIu64 "%%",
        percentage->is_negative ? "-" : "+",
        percentage->basis_points / 100,
        percentage->basis_points % 100
    );
}

static void print_comparison_summary(
    const char *label,
    const AnalyticsSummary *summary
)
{
    printf("%s: ", label);
    print_amount(summary->total_paise);
    printf(" (%zu transactions; average ", summary->transaction_count);
    if (summary->transaction_count == 0) {
        printf("N/A");
    } else {
        print_amount(summary->average_paise);
        if (summary->average_remainder_paise != 0) {
            printf(
                " + %" PRId64 "/%zu paise",
                summary->average_remainder_paise,
                summary->transaction_count
            );
        }
    }
    printf(")\n");
}

static void show_period_comparison(
    const ExpenseList *expenses,
    const CategoryList *categories,
    int comparison_type
)
{
    AnalyticsPeriod period_a = {0};
    AnalyticsPeriod period_b = {0};
    AnalyticsComparison comparison;
    AnalyticsResult result;

    if (!read_comparison_period(comparison_type, "Period A", &period_a)
        || !read_comparison_period(comparison_type, "Period B", &period_b)) {
        printf("Invalid comparison period. No periods were changed.\n");
        return;
    }

    result = analytics_compare_periods(
        expenses,
        categories,
        &period_a,
        &period_b,
        &comparison
    );
    if (result == ANALYTICS_INVALID_CATEGORY) {
        printf(
            "Unable to compare categories: an expense references an unknown "
            "category.\n"
        );
        return;
    }
    if (result == ANALYTICS_MEMORY_ERROR) {
        printf("Unable to compare periods: memory allocation failed.\n");
        return;
    }
    if (result == ANALYTICS_OVERFLOW) {
        printf("Unable to compare periods: a total or percentage overflowed.\n");
        return;
    }
    if (result != ANALYTICS_SUCCESS) {
        printf("Unable to compare periods: invalid input.\n");
        return;
    }

    printf("\nComparative Spending Analysis\n");
    print_comparison_summary("Period A", &comparison.period_a);
    print_comparison_summary("Period B", &comparison.period_b);
    printf("Absolute change (B - A): ");
    print_signed_amount(comparison.absolute_change_paise);
    printf("\nPercentage change: ");
    print_percentage_change(&comparison.percentage_change);
    printf("\n\nCategory comparison (B - A):\n");

    if (comparison.category_count == 0) {
        printf("No category transactions in either period.\n");
    } else {
        printf("%-16s %11s %11s %11s %8s %8s %12s\n",
            "Category", "A total", "B total", "Change", "A share",
            "B share", "Change %");
        for (size_t index = 0; index < comparison.category_count; index++) {
            const AnalyticsCategoryComparison *category =
                &comparison.categories[index];

            printf("%-16s ", category->category_name);
            print_amount(category->period_a_total_paise);
            printf(" ");
            print_amount(category->period_b_total_paise);
            printf(" ");
            print_signed_amount(category->absolute_change_paise);
            printf(" ");
            printf(
                "%3u.%02u%% ",
                category->period_a_percentage_basis_points / 100,
                category->period_a_percentage_basis_points % 100
            );
            printf(
                "%3u.%02u%% ",
                category->period_b_percentage_basis_points / 100,
                category->period_b_percentage_basis_points % 100
            );
            print_percentage_change(&category->percentage_change);
            printf("\n");
        }
    }

    analytics_comparison_destroy(&comparison);
}

static void show_comparison_menu(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    int choice;

    printf("\nCompare Periods\n");
    printf("[1] Month vs Month\n");
    printf("[2] Year vs Year\n");
    printf("[3] Date Range vs Date Range\n");
    printf("[0] Back\n");
    if (!read_integer("Choice (0-3): ", &choice)
        || choice < 0 || choice > 3) {
        printf("Invalid comparison choice.\n");
        return;
    }
    if (choice != 0) {
        show_period_comparison(expenses, categories, choice);
    }
}

static void print_comparison_insight_group(
    const char *label,
    const AnalyticsCategoryComparison *categories,
    size_t count
)
{
    printf("%s", label);
    if (count > 1) {
        printf(" (tie):\n");
    } else if (count == 1) {
        printf(":\n");
    } else {
        printf(": none\n");
        return;
    }

    for (size_t index = 0; index < count; index++) {
        const AnalyticsCategoryComparison *category = &categories[index];
        printf("  %s (ID %d) - ", category->category_name, category->category_id);
        print_signed_amount(category->absolute_change_paise);
        printf("\n");
    }
}

static void show_comparison_insights(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    AnalyticsPeriod period_a = {0};
    AnalyticsPeriod period_b = {0};
    AnalyticsComparison comparison;
    AnalyticsComparisonInsights insights;
    AnalyticsResult result;
    int comparison_type;

    printf("\nComparison Insight Periods\n");
    printf("[1] Month vs Month\n");
    printf("[2] Year vs Year\n");
    printf("[3] Date Range vs Date Range\n");
    if (!read_integer("Choice (1-3): ", &comparison_type)
        || comparison_type < 1 || comparison_type > 3) {
        printf("Invalid comparison choice.\n");
        return;
    }

    if (!read_comparison_period(comparison_type, "Period A", &period_a)
        || !read_comparison_period(comparison_type, "Period B", &period_b)) {
        printf("Invalid comparison period.\n");
        return;
    }

    result = analytics_compare_periods(
        expenses,
        categories,
        &period_a,
        &period_b,
        &comparison
    );
    if (result != ANALYTICS_SUCCESS) {
        if (result == ANALYTICS_INVALID_CATEGORY) {
            printf("Unable to calculate comparison insights: unknown category reference.\n");
        } else if (result == ANALYTICS_MEMORY_ERROR) {
            printf("Unable to calculate comparison insights: memory allocation failed.\n");
        } else if (result == ANALYTICS_OVERFLOW) {
            printf("Unable to calculate comparison insights: amount overflow.\n");
        } else {
            printf("Unable to calculate comparison insights: invalid input.\n");
        }
        return;
    }

    result = analytics_extract_comparison_insights(&comparison, &insights);
    if (result != ANALYTICS_SUCCESS) {
        analytics_comparison_destroy(&comparison);
        if (result == ANALYTICS_MEMORY_ERROR) {
            printf("Unable to derive comparison insights: memory allocation failed.\n");
        } else {
            printf("Unable to derive comparison insights: invalid analytics result.\n");
        }
        return;
    }

    printf("\nCategory Comparison Insights (Period B - Period A)\n");
    print_comparison_insight_group(
        "Largest absolute category change",
        insights.largest_absolute_change,
        insights.largest_absolute_count
    );
    if (insights.largest_increase_count == 0) {
        printf("No category increases between the selected periods.\n");
    } else {
        print_comparison_insight_group(
            "Largest category increase",
            insights.largest_increase,
            insights.largest_increase_count
        );
    }
    if (insights.largest_decrease_count == 0) {
        printf("No category decreases between the selected periods.\n");
    } else {
        print_comparison_insight_group(
            "Largest category decrease",
            insights.largest_decrease,
            insights.largest_decrease_count
        );
    }

    analytics_comparison_insights_destroy(&insights);
    analytics_comparison_destroy(&comparison);
}

static void print_trend_period(const AnalyticsTrendPeriod *period, int monthly)
{
    if (monthly) {
        printf(
            "%04d-%02d",
            period->year,
            period->month
        );
    } else {
        printf("%04d", period->year);
    }

    printf("  ");
    print_amount(period->total_paise);
    printf("  %zu transactions  Average: ", period->transaction_count);
    if (period->transaction_count == 0) {
        printf("N/A");
    } else {
        print_amount(period->average_paise);
        if (period->average_remainder_paise != 0) {
            printf(
                " + %" PRId64 "/%zu paise",
                period->average_remainder_paise,
                period->transaction_count
            );
        }
    }
    printf("\n");
}

static int calculate_trend_from_input(
    const ExpenseList *expenses,
    AnalyticsTrendType type,
    AnalyticsTrend *trend
)
{
    int year;
    int month = 0;
    int period_count;
    int monthly = type == ANALYTICS_TREND_MONTHLY;
    AnalyticsResult result;

    if (!read_integer("Starting year (1-9999): ", &year)
        || year < 1 || year > 9999) {
        printf("Invalid starting year.\n");
        return 0;
    }
    if (monthly
        && (!read_integer("Starting month (1-12): ", &month)
            || month < 1 || month > 12)) {
        printf("Invalid starting month.\n");
        return 0;
    }

    printf(
        "Maximum periods: %d\n",
        monthly
            ? ANALYTICS_MAX_MONTHLY_TREND_PERIODS
            : ANALYTICS_MAX_YEARLY_TREND_PERIODS
    );
    if (!read_integer(
            monthly ? "Number of months: " : "Number of years: ",
            &period_count
        )
        || period_count <= 0) {
        printf("Invalid period count.\n");
        return 0;
    }

    result = analytics_calculate_trend(
        expenses,
        type,
        year,
        month,
        (size_t)period_count,
        trend
    );
    if (result == ANALYTICS_INVALID_INPUT) {
        printf("Invalid trend range or expense data.\n");
    } else if (result == ANALYTICS_MEMORY_ERROR) {
        printf("Unable to calculate trend: memory allocation failed.\n");
    } else if (result == ANALYTICS_OVERFLOW) {
        printf("Unable to calculate trend: an amount would overflow.\n");
    } else if (result != ANALYTICS_SUCCESS) {
        printf("Unable to calculate trend.\n");
    }
    return result == ANALYTICS_SUCCESS;
}

static void print_period_insight_group(
    const char *label,
    const AnalyticsTrendPeriod *periods,
    size_t count
)
{
    printf("%s", label);
    if (count > 1) {
        printf(" (tie):\n");
    } else if (count == 1) {
        printf(":\n");
    } else {
        printf(": none\n");
        return;
    }

    for (size_t index = 0; index < count; index++) {
        print_trend_period(&periods[index], periods[index].month != 0);
    }
}

static void show_period_insights(
    const ExpenseList *expenses,
    AnalyticsTrendType type
)
{
    AnalyticsTrend trend;
    AnalyticsPeriodInsights insights;
    int monthly = type == ANALYTICS_TREND_MONTHLY;

    if (!calculate_trend_from_input(expenses, type, &trend)) {
        return;
    }

    {
        AnalyticsResult result = analytics_extract_period_insights(
            &trend,
            &insights
        );
        if (result != ANALYTICS_SUCCESS) {
            analytics_trend_destroy(&trend);
            if (result == ANALYTICS_MEMORY_ERROR) {
                printf("Unable to derive period insights: memory allocation failed.\n");
            } else {
                printf("Unable to derive period insights: invalid trend result.\n");
            }
            return;
        }
    }

    printf("\n%s Period Insights\n", monthly ? "Monthly" : "Yearly");
    print_period_insight_group(
        "Highest-spending period",
        insights.highest_spending,
        insights.highest_count
    );
    print_period_insight_group(
        "Lowest-spending period",
        insights.lowest_spending,
        insights.lowest_count
    );
    printf("Total spending across periods: ");
    print_amount(insights.total_paise);
    printf("\nAverage spending per period: ");
    print_amount(insights.average_period_paise);
    if (insights.average_period_remainder_paise != 0) {
        printf(
            " + %" PRId64 "/%zu paise",
            insights.average_period_remainder_paise,
            trend.period_count
        );
    }
    printf("\n");
    analytics_period_insights_destroy(&insights);
    analytics_trend_destroy(&trend);
}

static void show_period_insights_menu(const ExpenseList *expenses)
{
    int choice;

    printf("\nPeriod Insights\n");
    printf("[1] Monthly Period Insights\n");
    printf("[2] Yearly Period Insights\n");
    printf("[0] Back\n");
    if (!read_integer("Choice (0-2): ", &choice)
        || choice < 0 || choice > 2) {
        printf("Invalid period insight choice.\n");
        return;
    }
    if (choice == 1) {
        show_period_insights(expenses, ANALYTICS_TREND_MONTHLY);
    } else if (choice == 2) {
        show_period_insights(expenses, ANALYTICS_TREND_YEARLY);
    }
}

static void show_financial_insights(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    int choice;

    printf("\nFinancial Insights\n");
    printf("[1] Category Insights\n");
    printf("[2] Period Insights\n");
    printf("[3] Comparison Insights\n");
    printf("[0] Back\n");
    if (!read_integer("Choice (0-3): ", &choice)
        || choice < 0 || choice > 3) {
        printf("Invalid insight choice.\n");
        return;
    }
    if (choice == 1) {
        show_category_insights(expenses, categories);
    } else if (choice == 2) {
        show_period_insights_menu(expenses);
    } else if (choice == 3) {
        show_comparison_insights(expenses, categories);
    }
}

static void print_report_period(const AnalyticsPeriod *period)
{
    static const char *const month_names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    switch (period->type) {
    case ANALYTICS_PERIOD_ALL_TIME:
        printf("All Time");
        break;
    case ANALYTICS_PERIOD_MONTH:
        printf(
            "%s %04d",
            month_names[period->month - 1],
            period->year
        );
        break;
    case ANALYTICS_PERIOD_YEAR:
        printf("%04d", period->year);
        break;
    case ANALYTICS_PERIOD_DATE_RANGE:
        printf(
            "%04d-%02d-%02d to %04d-%02d-%02d",
            period->start_date.year,
            period->start_date.month,
            period->start_date.day,
            period->end_date.year,
            period->end_date.month,
            period->end_date.day
        );
        break;
    case ANALYTICS_PERIOD_DAY:
        printf(
            "%04d-%02d-%02d",
            period->date.year,
            period->date.month,
            period->date.day
        );
        break;
    }
}

static void print_report_categories(
    const AnalyticsReport *report,
    const CategoryList *categories
)
{
    if (report->category_breakdown.transaction_count == 0) {
        printf("No transactions found for this period.\n");
        return;
    }

    printf("%-24s %14s %12s %10s\n",
        "Category", "Transactions", "Total", "Share");
    for (size_t index = 0; index < report->category_breakdown.size; index++) {
        const AnalyticsCategoryTotal *category =
            &report->category_breakdown.items[index];
        const Category *source_category = category_find_by_id(
            categories,
            category->category_id
        );

        printf(
            "%-20s%s %-14zu ",
            category->category_name,
            source_category != NULL && !source_category->is_active
                ? " (inactive)"
                : "",
            category->transaction_count
        );
        print_amount(category->total_paise);
        printf(
            " %5u.%02u%%\n",
            category->percentage_basis_points / 100,
            category->percentage_basis_points % 100
        );
    }
}

static void print_report(const AnalyticsReport *report, const CategoryList *categories)
{
    const AnalyticsSummary *summary = &report->summary;

    printf("\n========================================\n");
    printf("FINANCIAL REPORT - ");
    print_report_period(&report->period);
    printf("\n========================================\n");

    printf("\nOVERVIEW\n");
    printf("----------------------------------------\n");
    printf("Total Spending:       ");
    print_amount(summary->total_paise);
    printf("\nTransactions:         %zu\n", summary->transaction_count);
    if (summary->transaction_count == 0) {
        printf("No transactions found for this period.\n");
    } else {
        printf("Average Transaction: ");
        print_amount(summary->average_paise);
        if (summary->average_remainder_paise != 0) {
            printf(
                " + %" PRId64 "/%zu paise",
                summary->average_remainder_paise,
                summary->transaction_count
            );
        }
        printf("\nMinimum Expense:     ");
        print_amount(summary->minimum_paise);
        printf("\nMaximum Expense:     ");
        print_amount(summary->maximum_paise);
        printf("\n");
    }

    printf("\nCATEGORY BREAKDOWN\n");
    printf("----------------------------------------\n");
    print_report_categories(report, categories);

    printf("\nINSIGHTS\n");
    printf("----------------------------------------\n");
    if (report->category_insights.highest_count == 0) {
        printf("No category data for this period.\n");
    } else {
        print_category_insight_group(
            "Highest-spending category",
            report->category_insights.highest_spending,
            report->category_insights.highest_count,
            1
        );
        print_category_insight_group(
            "Lowest-spending category among categories with transactions",
            report->category_insights.lowest_spending,
            report->category_insights.lowest_count,
            0
        );
    }
}

static void show_report(
    const ExpenseList *expenses,
    const CategoryList *categories,
    AnalyticsPeriodType type
)
{
    AnalyticsPeriod period = {0};
    AnalyticsReport report;
    AnalyticsResult result;

    period.type = type;
    if (type == ANALYTICS_PERIOD_MONTH) {
        if (!read_integer("Year (1-9999): ", &period.year)
            || period.year < 1 || period.year > 9999
            || !read_integer("Month (1-12): ", &period.month)
            || period.month < 1 || period.month > 12) {
            printf("Invalid year or month.\n");
            return;
        }
    } else if (type == ANALYTICS_PERIOD_YEAR) {
        if (!read_integer("Year (1-9999): ", &period.year)
            || period.year < 1 || period.year > 9999) {
            printf("Invalid year.\n");
            return;
        }
    } else if (type == ANALYTICS_PERIOD_DATE_RANGE) {
        if (!read_date("Start date (YYYY-MM-DD): ", &period.start_date)
            || !read_date("End date (YYYY-MM-DD): ", &period.end_date)
            || period.start_date.year > period.end_date.year
            || (period.start_date.year == period.end_date.year
                && (period.start_date.month > period.end_date.month
                    || (period.start_date.month == period.end_date.month
                        && period.start_date.day > period.end_date.day)))) {
            printf("Invalid date range.\n");
            return;
        }
    }

    result = analytics_calculate_report(
        expenses,
        categories,
        &period,
        &report
    );
    if (result == ANALYTICS_INVALID_CATEGORY) {
        printf("Unable to generate report: an expense references an unknown category.\n");
    } else if (result == ANALYTICS_MEMORY_ERROR) {
        printf("Unable to generate report: memory allocation failed.\n");
    } else if (result == ANALYTICS_OVERFLOW) {
        printf("Unable to generate report: a total overflowed.\n");
    } else if (result != ANALYTICS_SUCCESS) {
        printf("Unable to generate report: invalid period or expense data.\n");
    } else {
        print_report(&report, categories);
        analytics_report_destroy(&report);
    }
}

static void show_reports_menu(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    int choice;

    printf("\nReports\n");
    printf("[1] Overall Report\n");
    printf("[2] Monthly Report\n");
    printf("[3] Yearly Report\n");
    printf("[4] Custom Date Range Report\n");
    printf("[0] Back\n");
    if (!read_integer("Choice (0-4): ", &choice)
        || choice < 0 || choice > 4) {
        printf("Invalid report selection.\n");
        return;
    }

    if (choice == 1) {
        show_report(expenses, categories, ANALYTICS_PERIOD_ALL_TIME);
    } else if (choice == 2) {
        show_report(expenses, categories, ANALYTICS_PERIOD_MONTH);
    } else if (choice == 3) {
        show_report(expenses, categories, ANALYTICS_PERIOD_YEAR);
    } else if (choice == 4) {
        show_report(expenses, categories, ANALYTICS_PERIOD_DATE_RANGE);
    }
}

static void show_trend(
    const ExpenseList *expenses,
    AnalyticsTrendType type
)
{
    AnalyticsTrend trend;
    int monthly = type == ANALYTICS_TREND_MONTHLY;
    const char *title = monthly
        ? "Monthly Spending Trend"
        : "Yearly Spending Trend";

    if (!calculate_trend_from_input(expenses, type, &trend)) {
        return;
    }

    printf("\n%s\n", title);
    for (size_t index = 0; index < trend.period_count; index++) {
        print_trend_period(&trend.periods[index], monthly);
    }

    printf("\nTrend Summary\n");
    printf("Periods analyzed: %zu\n", trend.period_count);
    printf("Total spending: ");
    print_amount(trend.total_paise);
    printf("\nAverage spending per period: ");
    print_amount(trend.average_period_paise);
    if (trend.average_period_remainder_paise != 0) {
        printf(
            " + %" PRId64 "/%zu paise",
            trend.average_period_remainder_paise,
            trend.period_count
        );
    }
    printf("\nHighest-spending period: ");
    print_trend_period(
        &trend.periods[trend.highest_period_index],
        monthly
    );
    printf("Lowest-spending period: ");
    print_trend_period(
        &trend.periods[trend.lowest_period_index],
        monthly
    );
    printf("Increasing transitions: %zu\n", trend.increasing_transitions);
    printf("Decreasing transitions: %zu\n", trend.decreasing_transitions);
    printf("Unchanged transitions: %zu\n", trend.unchanged_transitions);
    printf("First-to-last change: ");
    print_signed_amount(trend.first_to_last_change_paise);
    printf("\nFirst-to-last percentage change: ");
    print_percentage_change(&trend.first_to_last_percentage_change);
    printf("\n");
    analytics_trend_destroy(&trend);
}

static void show_trend_menu(const ExpenseList *expenses)
{
    int choice;

    printf("\nSpending Trends\n");
    printf("[1] Monthly Trend\n");
    printf("[2] Yearly Trend\n");
    printf("[0] Back\n");
    if (!read_integer("Choice (0-2): ", &choice)
        || choice < 0 || choice > 2) {
        printf("Invalid trend choice.\n");
        return;
    }
    if (choice == 1) {
        show_trend(expenses, ANALYTICS_TREND_MONTHLY);
    } else if (choice == 2) {
        show_trend(expenses, ANALYTICS_TREND_YEARLY);
    }
}

void analytics_ui_show_summary(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[32];
    int running = 1;

    while (running) {
        int choice;
        int read_result;

        printf("\n---------- SPENDING ANALYSIS ----------\n");
        printf("[1] Overall Summary\n");
        printf("[2] Category Breakdown\n");
        printf("[3] Daily Spending\n");
        printf("[4] Monthly Spending\n");
        printf("[5] Yearly Spending\n");
        printf("[6] Custom Date Range\n");
        printf("[7] Compare Periods\n");
        printf("[8] Spending Trends\n");
        printf("[9] Financial Insights\n");
        printf("[10] Reports\n");
        printf("[0] Back\n");
        printf("Choice (0-10): ");

        read_result = read_input(input, sizeof(input));
        if (read_result == 0) {
            printf("Unable to read choice. Returning to main menu.\n");
            return;
        }
        if (read_result < 0) {
            printf("Choice is too long. Enter a number from 0 to 10.\n");
            continue;
        }

        {
            char *end;
            long parsed;

            errno = 0;
            parsed = strtol(input, &end, 10);
            while (isspace((unsigned char)*end)) {
                end++;
            }
            if (input == end || errno == ERANGE || *end != '\0'
                || parsed < 0 || parsed > 10) {
                printf("Invalid choice. Enter a number from 0 to 10.\n");
                continue;
            }
            choice = (int)parsed;
        }

        switch (choice) {
        case 1:
            show_overall_summary(expenses, categories);
            break;
        case 2:
            show_category_breakdown(expenses, categories);
            break;
        case 3:
            show_daily_summary(expenses);
            break;
        case 4:
            show_monthly_summary(expenses);
            break;
        case 5:
            show_yearly_summary(expenses);
            break;
        case 6:
            show_date_range_summary(expenses);
            break;
        case 7:
            show_comparison_menu(expenses, categories);
            break;
        case 8:
            show_trend_menu(expenses);
            break;
        case 9:
            show_financial_insights(expenses, categories);
            break;
        case 10:
            show_reports_menu(expenses, categories);
            break;
        case 0:
            running = 0;
            break;
        default:
            break;
        }
    }
}
