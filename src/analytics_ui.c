#include "analytics_ui.h"

#include "analytics.h"

#include <inttypes.h>
#include <stdio.h>

static void print_amount(int64_t amount_paise)
{
    printf(
        "Rs.%lld.%02lld",
        (long long)(amount_paise / 100),
        (long long)(amount_paise % 100)
    );
}

void analytics_ui_show_summary(
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

    if (result == ANALYTICS_MEMORY_ERROR) {
        printf(
            "Unable to calculate the summary because memory allocation "
            "failed.\n"
        );
        return;
    }
    if (result == ANALYTICS_OVERFLOW) {
        printf(
            "Unable to calculate the summary because a total would "
            "overflow int64_t.\n"
        );
        return;
    }
    if (result == ANALYTICS_INVALID_CATEGORY) {
        printf(
            "Unable to calculate the summary because an expense references "
            "an unknown category.\n"
        );
        return;
    }
    if (result != ANALYTICS_SUCCESS) {
        printf("Unable to calculate the spending summary.\n");
        return;
    }

    printf("\n---------- SPENDING SUMMARY ----------\n");

    printf("Total Spent: ");
    print_amount(summary.total_paise);

    printf("\nTransactions: %zu", summary.transaction_count);

    printf("\nToday: ");
    print_amount(summary.today_paise);

    printf("\nThis Month: ");
    print_amount(summary.current_month_paise);

    if (summary.transaction_count == 0) {
        printf("\nAverage Transaction: N/A");
        printf("\nMinimum Transaction: N/A");
        printf("\nMaximum Transaction: N/A");
    } else {
        printf("\nAverage Transaction: ");
        print_amount(summary.average_paise);
        if (summary.average_remainder_paise != 0) {
            printf(
                " + %" PRId64 "/%zu paise",
                summary.average_remainder_paise,
                summary.transaction_count
            );
        }

        printf("\nMinimum Transaction: ");
        print_amount(summary.minimum_paise);

        printf("\nMaximum Transaction: ");
        print_amount(summary.maximum_paise);
    }

    printf("\n\nBy Category:\n");
    if (summary.transaction_count == 0) {
        printf("No expenses recorded.\n");
    } else {
        for (size_t index = 0; index < summary.category_count; index++) {
            if (summary.category_totals_paise[index] > 0) {
                printf(
                    "%-18s ",
                    categories->items[index].name
                );
                print_amount(summary.category_totals_paise[index]);
                printf("\n");
            }
        }
    }

    analytics_summary_destroy(&summary);
}
