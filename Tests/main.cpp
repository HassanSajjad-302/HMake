#include <gtest/gtest.h>
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) = "Exam*";
    /*    RUN_ALL_TESTS();
        ::testing::GTEST_FLAG(filter) = "tB.*";
        RUN_ALL_TESTS();
        ::testing::GTEST_FLAG(filter) = "tC.*";*/
    return RUN_ALL_TESTS();
    return RUN_ALL_TESTS();
}