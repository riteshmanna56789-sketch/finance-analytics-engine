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
        printf("[0] Back\n");
        printf("Choice (0-6): ");

        read_result = read_input(input, sizeof(input));
        if (read_result == 0) {
            printf("Unable to read choice. Returning to main menu.\n");
            return;
        }
        if (read_result < 0) {
            printf("Choice is too long. Enter a number from 0 to 6.\n");
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
                || parsed < 0 || parsed > 6) {
                printf("Invalid choice. Enter a number from 0 to 6.\n");
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
        case 0:
            running = 0;
            break;
        default:
            break;
        }
    }
}
