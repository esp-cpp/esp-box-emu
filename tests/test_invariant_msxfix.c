#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include the actual production code
#include "components/msx/fmsx/msxfix.c"

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "A",  // Valid input (minimal)
        "12345678901234567890123456789012345678901234567890",  // Boundary case (50 chars)
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // Exploit case (200 chars)
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Test msx_chdir with adversarial input
        int result = msx_chdir(payloads[i]);
        ck_assert_int_eq(result, 0);
        
        // Verify msx_getcwd doesn't overflow
        char test_buffer[256];
        const char *cwd_result = msx_getcwd(test_buffer, sizeof(test_buffer));
        ck_assert_msg(cwd_result != NULL, "msx_getcwd should succeed with sufficient buffer");
        
        // Verify the buffer wasn't overflowed by checking null termination
        ck_assert_msg(strlen(test_buffer) < sizeof(test_buffer), 
                     "Buffer read exceeded declared length");
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}