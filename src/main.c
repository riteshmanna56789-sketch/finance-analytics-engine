#include "category.h"
#include "expense.h"
#include "analytics.h"
#include "query.h"
#include "sorting.h"
#include "storage.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_main_menu(void)
{
    printf("\n[1] Add Expense\n");
    printf("[2] View Expenses\n");
    printf("[3] Categories\n");
    printf("[4] Spending Summary\n");
    printf("[5] Save Data\n");
    printf("[0] Exit\n\n");
    printf("Choice (0-5):\n");
    printf("> ");
}

static int read_line(char *buffer, size_t buffer_size)
{
    size_t length;
    int character;

    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';

        if (length > 1 && buffer[length - 2] == '\r') {
            buffer[length - 2] = '\0';
        }

        return 1;
    }

    while ((character = getchar()) != '\n' && character != EOF) {
    }

    return -1;
}

static int parse_menu_choice(const char *input, int *choice)
{
    char *end;
    long parsed_choice;

    errno = 0;
    parsed_choice = strtol(input, &end, 10);

    if (input == end
        || errno == ERANGE
        || parsed_choice < INT_MIN
        || parsed_choice > INT_MAX) {
        return 0;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        return 0;
    }

    *choice = (int)parsed_choice;
    return 1;
}

static int is_blank(const char *text)
{
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }

        text++;
    }

    return 1;
}

static void print_category_error(CategoryResult result)
{
    if (result == CATEGORY_DUPLICATE) {
        printf("A category with that name already exists.\n");
    } else if (result == CATEGORY_INVALID_INPUT) {
        printf(
            "Category name is invalid. Enter a non-empty name of fewer "
            "than 50 characters.\n"
        );
    } else if (result == CATEGORY_MEMORY_ERROR) {
        printf(
            "Unable to create the category because memory allocation "
            "failed.\n"
        );
    }
}

static void print_deactivation_error(CategoryResult result)
{
    if (result == CATEGORY_NOT_FOUND) {
        printf("No category exists with that ID.\n");
    } else if (result == CATEGORY_ALREADY_INACTIVE) {
        printf("That category is already inactive.\n");
    } else if (result == CATEGORY_INVALID_INPUT) {
        printf("Invalid category ID.\n");
    } else {
        printf("Unable to deactivate the category.\n");
    }
}

static void view_categories(const CategoryList *categories)
{
    size_t active_count = category_active_count(categories);

    printf("\n------------- CATEGORIES -------------\n");

    for (size_t index = 0; index < active_count; index++) {
        const Category *category = category_find_active_by_index(
            categories,
            index
        );

        printf("%d. %s\n", category->id, category->name);
    }

    if (active_count == 0) {
        printf("No active categories.\n");
    }
}

static int create_category(CategoryList *categories)
{
    char name[100];
    int read_result;
    CategoryResult result;

    printf("\n------------- CREATE CATEGORY -------------\n");
    printf("Name: ");

    read_result = read_line(name, sizeof(name));

    if (read_result == 0) {
        return 0;
    }

    if (read_result < 0 || is_blank(name)) {
        printf("Category name cannot be empty or whitespace-only.\n");
        return 1;
    }

    result = category_create(categories, name);

    if (result == CATEGORY_SUCCESS) {
        printf("Category created successfully.\n");
    } else {
        print_category_error(result);
    }

    return 1;
}

static int deactivate_category(CategoryList *categories)
{
    char input[100];
    int choice;
    int active_count;
    const Category *category;
    CategoryResult result;

    active_count = (int)category_active_count(categories);
    if (active_count == 0) {
        printf("No active categories available.\n");
        return 1;
    }

    printf("\n------------- DEACTIVATE CATEGORY -------------\n");
    for (int index = 0; index < active_count; index++) {
        category = category_find_active_by_index(
            categories,
            (size_t)index
        );
        printf("[%d] %s\n", category->id, category->name);
    }
    printf("[0] Cancel\n");
    printf("Category ID: ");

    if (read_line(input, sizeof(input)) <= 0
        || !parse_menu_choice(input, &choice)) {
        printf("Invalid category ID.\n");
        return 1;
    }

    if (choice == 0) {
        return 1;
    }

    result = category_deactivate(categories, choice);
    if (result == CATEGORY_SUCCESS) {
        printf("Category deactivated successfully.\n");
    } else {
        print_deactivation_error(result);
    }

    return 1;
}

static void manage_categories(CategoryList *categories)
{
    char input[100];
    int choice;
    int running = 1;

    while (running) {
        printf("\n[1] Create Category\n");
        printf("[2] View Categories\n");
        printf("[3] Deactivate Category\n");
        printf("[0] Back\n\n");
        printf("Choice (0-3):\n");
        printf("> ");

        if (read_line(input, sizeof(input)) <= 0) {
            printf("Unable to read input. Returning to main menu.\n");
            return;
        }

        if (!parse_menu_choice(input, &choice)
            || choice < 0
            || choice > 3) {
            printf("Invalid choice. Please enter a number from 0 to 3.\n");
            continue;
        }

        switch (choice) {
        case 1:
            if (!create_category(categories)) {
                running = 0;
            }
            break;

        case 2:
            view_categories(categories);
            break;

        case 3:
            if (!deactivate_category(categories)) {
                running = 0;
            }
            break;

        case 0:
            running = 0;
            break;

        default:
            break;
        }
    }
}

static int choose_category(
    CategoryList *categories,
    int *category_id
)
{
    char input[100];
    int choice;
    int active_count;

    for (;;) {
        active_count = (int)category_active_count(categories);

        printf("\nSelect Category:\n");

        for (int index = 0; index < active_count; index++) {
            const Category *category = category_find_active_by_index(
                categories,
                (size_t)index
            );

            printf("[%d] %s\n", index + 1, category->name);
        }

        printf("[%d] Create new category\n", active_count + 1);
        printf("[0] Cancel\n");
        printf("Choice (0-%d): ", active_count + 1);

        if (read_line(input, sizeof(input)) <= 0
            || !parse_menu_choice(input, &choice)
            || choice < 0
            || choice > active_count + 1) {
            printf("Invalid category selection.\n");
            return 0;
        }

        if (choice == 0) {
            return 0;
        }

        if (choice == active_count + 1) {
            if (!create_category(categories)) {
                return 0;
            }

            continue;
        }

        {
            const Category *category = category_find_active_by_index(
                categories,
                (size_t)(choice - 1)
            );

            if (category != NULL) {
                *category_id = category->id;
                return 1;
            }
        }
    }
}

static int add_expense(
    ExpenseList *expenses,
    CategoryList *categories
)
{
    char input[256];
    char note[sizeof(((Expense *)0)->note)];
    int64_t amount_paise;
    ExpenseResult result;
    int category_id;
    int read_result;

    printf("\n------------- ADD EXPENSE -------------\n");
    printf("Amount: Rs.");

    read_result = read_line(input, sizeof(input));

    if (read_result <= 0
        || expense_parse_amount_paise(input, &amount_paise)
            != EXPENSE_SUCCESS) {
        printf(
            "Invalid amount. Enter a positive value with at most two "
            "decimal places.\n"
        );
        return 1;
    }

    if (!choose_category(categories, &category_id)) {
        return 1;
    }

    printf("Note (optional): ");

    read_result = read_line(note, sizeof(note));

    if (read_result == 0) {
        return 0;
    }

    if (read_result < 0) {
        printf("Note is too long. Please use fewer than 200 characters.\n");
        return 1;
    }

    result = expense_create(
        expenses,
        category_id,
        amount_paise,
        note
    );

    if (result != EXPENSE_SUCCESS) {
        if (result == EXPENSE_MEMORY_ERROR) {
            printf(
                "Unable to add the expense because memory allocation "
                "failed.\n"
            );
        } else {
            printf("Unable to prepare the expense.\n");
        }

        return 1;
    }

    printf("Expense added successfully.\n");
    return 1;
}

static void print_amount(int64_t amount_paise)
{
    printf(
        "Rs.%lld.%02lld",
        (long long)(amount_paise / 100),
        (long long)(amount_paise % 100)
    );
}

static void print_expense_header(void)
{
    printf(
        "\nID    Date/Time             Amount       Category       Note\n"
    );
    printf(
        "---------------------------------------------------------------\n"
    );
}

static void print_expense_row(
    const Expense *expense,
    void *context
)
{
    const CategoryList *categories = context;

    const Category *category = category_find_by_id(
        categories,
        expense->category_id
    );

    const char *category_name = category == NULL
        ? "Unknown"
        : category->name;

    printf(
        "%-5d %04d-%02d-%02d %02d:%02d:%02d ",
        expense->id,
        expense->timestamp.year,
        expense->timestamp.month,
        expense->timestamp.day,
        expense->timestamp.hour,
        expense->timestamp.minute,
        expense->timestamp.second
    );

    print_amount(expense->amount_paise);

    printf(" %-14s %s\n", category_name, expense->note);
}

static void count_match(
    const Expense *expense,
    void *context
)
{
    (void)expense;
    (*(size_t *)context)++;
}

static void view_all_expenses(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    print_expense_header();

    if (expenses->size == 0) {
        printf("No expenses recorded.\n");
        return;
    }

    for (size_t index = 0; index < expenses->size; index++) {
        print_expense_row(
            &expenses->items[index],
            (void *)categories
        );
    }
}

static void sort_expenses(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[100];
    int choice;
    ExpenseView view;
    SortingResult result;

    printf("\n[1] Date: Newest -> Oldest\n");
    printf("[2] Date: Oldest -> Newest\n");
    printf("[3] Amount: Low -> High\n");
    printf("[4] Amount: High -> Low\n");
    printf("[5] Category: A -> Z\n");
    printf("[0] Back\n");
    printf("Choice (0-5): ");

    if (read_line(input, sizeof(input)) <= 0
        || !parse_menu_choice(input, &choice)
        || choice < 0
        || choice > 5) {
        printf("Invalid sorting option.\n");
        return;
    }

    if (choice == 0) {
        return;
    }

    result = sorting_create_view(
        expenses,
        categories,
        (SortingOption)choice,
        &view
    );
    if (result == SORTING_MEMORY_ERROR) {
        printf("Unable to sort expenses because memory allocation failed.\n");
        return;
    }
    if (result != SORTING_SUCCESS) {
        printf("Unable to sort expenses because the option is invalid.\n");
        return;
    }

    print_expense_header();
    if (view.size == 0) {
        printf("No expenses recorded.\n");
    } else {
        for (size_t index = 0; index < view.size; index++) {
            print_expense_row(view.items[index], (void *)categories);
        }
    }

    sorting_view_destroy(&view);
}

static int select_active_category(
    const CategoryList *categories,
    int *category_id
)
{
    char input[100];
    int choice;
    int active_count = (int)category_active_count(categories);

    printf("\nActive Categories:\n");

    for (int index = 0; index < active_count; index++) {
        const Category *category = category_find_active_by_index(
            categories,
            (size_t)index
        );

        printf("[%d] %s\n", index + 1, category->name);
    }

    if (active_count == 0) {
        printf("No active categories available.\n");
        return 0;
    }

    printf("[0] Cancel\n");
    printf("Choice (0-%d): ", active_count);

    if (read_line(input, sizeof(input)) <= 0
        || !parse_menu_choice(input, &choice)
        || choice < 0
        || choice > active_count) {
        printf("Invalid category selection.\n");
        return 0;
    }

    if (choice == 0) {
        return 0;
    }

    {
        const Category *category = category_find_active_by_index(
            categories,
            (size_t)(choice - 1)
        );

        if (category != NULL) {
            *category_id = category->id;
            return 1;
        }
    }

    return 0;
}

static void report_matches(size_t match_count)
{
    if (match_count == 0) {
        printf("No matching expenses found.\n");
    }
}

static void search_by_category(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    int category_id;
    size_t match_count = 0;

    if (!select_active_category(categories, &category_id)) {
        return;
    }

    print_expense_header();

    query_by_category(
        expenses,
        category_id,
        print_expense_row,
        (void *)categories
    );

    query_by_category(
        expenses,
        category_id,
        count_match,
        &match_count
    );

    report_matches(match_count);
}

static void search_by_amount(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[100];
    int64_t amount_paise;
    size_t match_count = 0;

    printf("Exact amount: Rs.");

    if (read_line(input, sizeof(input)) <= 0
        || expense_parse_amount_paise(input, &amount_paise)
            != EXPENSE_SUCCESS) {
        printf("Invalid amount.\n");
        return;
    }

    print_expense_header();

    query_by_amount(
        expenses,
        amount_paise,
        print_expense_row,
        (void *)categories
    );

    query_by_amount(
        expenses,
        amount_paise,
        count_match,
        &match_count
    );

    report_matches(match_count);
}

static void filter_by_time(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[100];
    int choice;
    size_t match_count = 0;

    printf("\n[1] Today\n");
    printf("[2] This Week\n");
    printf("[3] This Month\n");
    printf("[4] This Year\n");
    printf("[0] Back\n");
    printf("Choice (0-4): ");

    if (read_line(input, sizeof(input)) <= 0
        || !parse_menu_choice(input, &choice)
        || choice < 0
        || choice > 4) {
        printf("Invalid time filter.\n");
        return;
    }

    if (choice == 0) {
        return;
    }

    print_expense_header();

    if (query_by_time(
            expenses,
            (QueryTimeFilter)choice,
            print_expense_row,
            (void *)categories
        ) != QUERY_SUCCESS) {
        printf("Unable to apply the time filter.\n");
        return;
    }

    query_by_time(
        expenses,
        (QueryTimeFilter)choice,
        count_match,
        &match_count
    );

    report_matches(match_count);
}

static void view_expenses(
    const ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[100];
    int choice;
    int running = 1;

    while (running) {
        printf("\n---------- VIEW EXPENSES ----------\n");
        printf("[1] View All Expenses\n");
        printf("[2] Search by Category\n");
        printf("[3] Search by Amount\n");
        printf("[4] Filter by Time\n");
        printf("[5] Sort Expenses\n");
        printf("[0] Back\n");
        printf("Choice (0-5):\n> ");

        if (read_line(input, sizeof(input)) <= 0) {
            return;
        }

        if (!parse_menu_choice(input, &choice)
            || choice < 0
            || choice > 5) {
            printf(
                "Invalid choice. Please enter a number from 0 to 5.\n"
            );
            continue;
        }

        switch (choice) {
        case 1:
            view_all_expenses(expenses, categories);
            break;

        case 2:
            search_by_category(expenses, categories);
            break;

        case 3:
            search_by_amount(expenses, categories);
            break;

        case 4:
            filter_by_time(expenses, categories);
            break;

        case 5:
            sort_expenses(expenses, categories);
            break;

        case 0:
            running = 0;
            break;

        default:
            break;
        }
    }
}

static void view_spending_summary(
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

    if (result != ANALYTICS_SUCCESS) {
        printf("Unable to calculate the spending summary.\n");
        return;
    }

    printf("\n---------- SPENDING SUMMARY ----------\n");

    printf("Total Spent: ");
    print_amount(summary.total_paise);

    printf("\nToday: ");
    print_amount(summary.today_paise);

    printf("\nThis Month: ");
    print_amount(summary.current_month_paise);

    printf("\n\nBy Category:\n");

    if (expenses->size == 0) {
        printf("No expenses recorded.\n");
    } else {
        for (size_t index = 0; index < categories->size; index++) {
            if (summary.category_totals_paise[index] > 0) {
                printf(
                    "%-18s ",
                    categories->items[index].name
                );

                print_amount(
                    summary.category_totals_paise[index]
                );

                printf("\n");
            }
        }
    }

    analytics_summary_destroy(&summary);
}

int main(void)
{
    static const char *DATA_FILE = "data/finance.dat";

    ExpenseList expenses;
    CategoryList categories;

    char input[100];

    int choice;
    int running = 1;

    StorageResult load_result;

    expense_list_init(&expenses);
    category_list_init(&categories);

    load_result = storage_load(
        DATA_FILE,
        &categories,
        &expenses
    );

    if (load_result == STORAGE_NOT_FOUND) {
        printf("No saved data found. Starting with empty data.\n");
    } else if (load_result == STORAGE_INVALID_DATA) {
        printf(
            "Stored data is invalid or corrupted. "
            "Exiting without overwriting it.\n"
        );

        category_list_destroy(&categories);
        expense_list_destroy(&expenses);

        return EXIT_FAILURE;
    } else if (load_result == STORAGE_FILE_ERROR) {
        printf("Unable to access the data file. Exiting.\n");

        category_list_destroy(&categories);
        expense_list_destroy(&expenses);

        return EXIT_FAILURE;
    } else if (load_result == STORAGE_MEMORY_ERROR) {
        printf(
            "Unable to load data because memory allocation failed.\n"
        );

        category_list_destroy(&categories);
        expense_list_destroy(&expenses);

        return EXIT_FAILURE;
    }

    while (running) {
        print_main_menu();

        if (read_line(input, sizeof(input)) <= 0) {
            printf("\nUnable to read input. Exiting.\n");
            break;
        }

        if (!parse_menu_choice(input, &choice)
            || choice < 0
            || choice > 5) {
            printf(
                "Invalid choice. Please enter a number from 0 to 5.\n"
            );
            continue;
        }

        switch (choice) {
        case 1:
            if (!add_expense(&expenses, &categories)) {
                running = 0;
            }
            break;

        case 2:
            view_expenses(&expenses, &categories);
            break;

        case 3:
            manage_categories(&categories);
            break;

        case 4:
            view_spending_summary(&expenses, &categories);
            break;

        case 5:
            if (storage_save(
                    DATA_FILE,
                    &categories,
                    &expenses
                ) == STORAGE_SUCCESS) {
                printf("Data saved successfully.\n");
            } else {
                printf("Unable to save data.\n");
            }
            break;

        case 0:
            running = 0;
            printf(
                "Goodbye. Save Data before exiting to keep your changes.\n"
            );
            break;

        default:
            break;
        }
    }

    category_list_destroy(&categories);
    expense_list_destroy(&expenses);

    return EXIT_SUCCESS;
}