#include "expense_management.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(char *buffer, size_t buffer_size)
{
    size_t length;
    int character;

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
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

static int parse_integer(const char *input, int *value)
{
    char *end;
    long parsed;

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

static void print_amount(int64_t amount_paise)
{
    printf(
        "Rs.%lld.%02lld",
        (long long)(amount_paise / 100),
        (long long)(amount_paise % 100)
    );
}

static void print_expense_details(
    const Expense *expense,
    const CategoryList *categories
)
{
    const Category *category = category_find_by_id(
        categories,
        expense->category_id
    );

    printf("Expense ID: %d\n", expense->id);
    printf("Amount: ");
    print_amount(expense->amount_paise);
    printf("\nCategory: %s\n", category == NULL ? "Unknown" : category->name);
    printf("Note: %s\n", expense->note);
}

static int select_edit_category(
    const CategoryList *categories,
    int current_category_id,
    int *category_id
)
{
    char input[100];
    int choice;
    int active_count = (int)category_active_count(categories);
    const Category *current_category = category_find_by_id(
        categories,
        current_category_id
    );

    printf("\nSelect new category:\n");
    printf("[0] Keep current (%s)\n",
        current_category == NULL ? "Unknown" : current_category->name);

    for (int index = 0; index < active_count; index++) {
        const Category *category = category_find_active_by_index(
            categories,
            (size_t)index
        );
        printf("[%d] %s\n", index + 1, category->name);
    }

    printf("Choice (0-%d): ", active_count);
    if (read_line(input, sizeof(input)) <= 0
        || !parse_integer(input, &choice)
        || choice < 0
        || choice > active_count) {
        printf("Invalid category selection.\n");
        return 0;
    }

    if (choice == 0) {
        *category_id = current_category_id;
        return 1;
    }

    {
        const Category *category = category_find_active_by_index(
            categories,
            (size_t)(choice - 1)
        );
        if (category == NULL) {
            printf("Invalid category selection.\n");
            return 0;
        }
        *category_id = category->id;
    }

    return 1;
}

static void print_update_error(ExpenseResult result)
{
    if (result == EXPENSE_NOT_FOUND) {
        printf("No expense exists with that ID.\n");
    } else if (result == EXPENSE_CATEGORY_NOT_FOUND) {
        printf("The selected category does not exist.\n");
    } else if (result == EXPENSE_CATEGORY_INACTIVE) {
        printf("An inactive category cannot be selected for an edit.\n");
    } else {
        printf("Unable to update the expense because the entered values "
            "are invalid.\n");
    }
}

void expense_management_edit(
    ExpenseList *expenses,
    const CategoryList *categories
)
{
    char input[sizeof(((Expense *)0)->note)];
    char note[sizeof(((Expense *)0)->note)];
    int expense_id;
    int category_id;
    int64_t amount_paise;
    int read_result;
    const Expense *expense;
    ExpenseResult result;

    printf("\n------------- EDIT EXPENSE -------------\n");
    printf("Expense ID: ");
    if (read_line(input, sizeof(input)) <= 0
        || !parse_integer(input, &expense_id)) {
        printf("Invalid expense ID.\n");
        return;
    }

    expense = expense_find_by_id(expenses, expense_id);
    if (expense == NULL) {
        printf("No expense exists with that ID.\n");
        return;
    }

    print_expense_details(expense, categories);

    printf("Enter new amount: Rs.");
    if (read_line(input, sizeof(input)) <= 0
        || expense_parse_amount_paise(input, &amount_paise)
            != EXPENSE_SUCCESS) {
        printf("Invalid amount.\n");
        return;
    }

    if (!select_edit_category(categories, expense->category_id, &category_id)) {
        return;
    }

    printf("Enter new note: ");
    read_result = read_line(note, sizeof(note));
    if (read_result == 0) {
        return;
    }
    if (read_result < 0) {
        printf("Note is too long. Please use fewer than 200 characters.\n");
        return;
    }

    result = expense_update(
        expenses,
        categories,
        expense_id,
        amount_paise,
        category_id,
        note
    );
    if (result == EXPENSE_SUCCESS) {
        printf("Expense %d updated successfully.\n", expense_id);
    } else {
        print_update_error(result);
    }
}

void expense_management_delete(ExpenseList *expenses)
{
    char input[100];
    int expense_id;
    const Expense *expense;
    ExpenseResult result;

    printf("\n------------- DELETE EXPENSE -------------\n");
    printf("Expense ID: ");
    if (read_line(input, sizeof(input)) <= 0
        || !parse_integer(input, &expense_id)) {
        printf("Invalid expense ID.\n");
        return;
    }

    expense = expense_find_by_id(expenses, expense_id);
    if (expense == NULL) {
        printf("No expense exists with that ID.\n");
        return;
    }

    printf("Delete expense ID %d? [Y/N]: ", expense_id);
    if (read_line(input, sizeof(input)) <= 0) {
        return;
    }

    if (input[0] != 'Y' && input[0] != 'y') {
        printf("Deletion cancelled.\n");
        return;
    }

    result = expense_delete(expenses, expense_id);
    if (result == EXPENSE_SUCCESS) {
        printf("Expense %d deleted successfully.\n", expense_id);
    } else if (result == EXPENSE_NOT_FOUND) {
        printf("No expense exists with that ID.\n");
    } else {
        printf("Unable to delete the expense.\n");
    }
}
