#ifndef ANALYTICS_UI_H
#define ANALYTICS_UI_H

#include "category.h"
#include "expense.h"

void analytics_ui_show_summary(
    const ExpenseList *expenses,
    const CategoryList *categories
);

#endif
