#include "test_support.h"

void test_category(void);
void test_expense(void);
void test_query(void);
void test_sorting(void);
void test_analytics(void);
void test_storage(void);
void test_storage_failure_paths(void);
void test_storage_v1_compatibility(void);
void test_workflows(void);

int main(void)
{
    test_category();
    test_expense();
    test_query();
    test_sorting();
    test_analytics();
    test_storage();
    test_storage_failure_paths();
    test_storage_v1_compatibility();
    test_workflows();
    return 0;
}
