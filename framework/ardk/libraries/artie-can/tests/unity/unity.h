/* =========================================================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
============================================================================ */

#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include <setjmp.h>
#include <math.h>
#include "unity_internals.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*-------------------------------------------------------
 * Test Setup / Teardown
 *-------------------------------------------------------*/
void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

/*-------------------------------------------------------
 * Test Reset and Verify
 *-------------------------------------------------------*/
void UnitySetTestFile(const char* filename);
void UnitySetTestLine(UNITY_LINE_TYPE line);
void UnityTestResultsBegin(const char* file, const UNITY_LINE_TYPE line);
void UnityTestResultsFailBegin(const UNITY_LINE_TYPE line);
void UnityConcludeTest(void);

/*-------------------------------------------------------
 * Configuration Options
 *-------------------------------------------------------*/
#ifndef UNITY_EXCLUDE_SETJMP_H
#define UNITY_EXCLUDE_SETJMP_H
#endif

/*-------------------------------------------------------
 * Test Asserts
 *-------------------------------------------------------*/
void UnityAssertEqualNumber(const UNITY_INT expected,
                            const UNITY_INT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_INTERNAL_CONTEXT style);

void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected,
                              UNITY_INTERNAL_PTR actual,
                              const UNITY_UINT32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_INTERNAL_CONTEXT style,
                              const UNITY_FLAGS_T flags);

void UnityAssertBits(const UNITY_INT mask,
                     const UNITY_INT expected,
                     const UNITY_INT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber);

void UnityAssertEqualStringArray(UNITY_INTERNAL_PTR expected,
                                 const char** actual,
                                 const UNITY_UINT32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber,
                                 const UNITY_FLAGS_T flags);

void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected,
                            UNITY_INTERNAL_PTR actual,
                            const UNITY_UINT32 length,
                            const UNITY_UINT32 num_elements,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_FLAGS_T flags);

void UnityAssertNumbersWithin(const UNITY_UINT delta,
                              const UNITY_INT expected,
                              const UNITY_INT actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_INTERNAL_CONTEXT style);

void UnityFail(const char* msg, const UNITY_LINE_TYPE line);

void UnityIgnore(const char* msg, const UNITY_LINE_TYPE line);

void UnityAssertFloatsWithin(const UNITY_FLOAT delta,
                             const UNITY_FLOAT expected,
                             const UNITY_FLOAT actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber);

void UnityAssertDoublesWithin(const UNITY_DOUBLE delta,
                              const UNITY_DOUBLE expected,
                              const UNITY_DOUBLE actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber);

/*-------------------------------------------------------
 * Helpers
 *-------------------------------------------------------*/
UNITY_INTERNAL_PTR UnityNumToPtr(const UNITY_INT num, const UNITY_UINT8 size);
#ifndef UNITY_EXCLUDE_FLOAT
UNITY_INTERNAL_PTR UnityFloatToPtr(const float num);
#endif
#ifndef UNITY_EXCLUDE_DOUBLE
UNITY_INTERNAL_PTR UnityDoubleToPtr(const double num);
#endif

/*-------------------------------------------------------
 * Error Strings We Commonly Use
 *-------------------------------------------------------*/
extern const char UnityStrErrFloat[];
extern const char UnityStrErrDouble[];
extern const char UnityStrErr64[];

/*-------------------------------------------------------
 * Test Running Macros
 *-------------------------------------------------------*/
#define TEST_PROTECT() (setjmp(Unity.AbortFrame) == 0)

#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

/*-------------------------------------------------------
 * Basic Fail and Ignore
 *-------------------------------------------------------*/
#define TEST_FAIL_MESSAGE(message)    UnityFail((message), (UNITY_LINE_TYPE)(__LINE__))
#define TEST_FAIL()                   UnityFail(NULL, (UNITY_LINE_TYPE)(__LINE__))
#define TEST_IGNORE_MESSAGE(message)  UnityIgnore((message), (UNITY_LINE_TYPE)(__LINE__))
#define TEST_IGNORE()                 UnityIgnore(NULL, (UNITY_LINE_TYPE)(__LINE__))
#define TEST_MESSAGE(message)         UnityMessage((message), (UNITY_LINE_TYPE)(__LINE__))

/*-------------------------------------------------------
 * Test Asserts (simple)
 *-------------------------------------------------------*/
#define TEST_ASSERT_TRUE(condition)      { if (!(condition)) { TEST_FAIL_MESSAGE("Expected TRUE Was FALSE"); } }
#define TEST_ASSERT_FALSE(condition)     { if (condition) { TEST_FAIL_MESSAGE("Expected FALSE Was TRUE"); } }
#define TEST_ASSERT(condition)           TEST_ASSERT_TRUE(condition)
#define TEST_ASSERT_UNLESS(condition)    TEST_ASSERT_FALSE(condition)

#define TEST_ASSERT_NULL(pointer)        TEST_ASSERT_MESSAGE((pointer == NULL), "Expected NULL")
#define TEST_ASSERT_NOT_NULL(pointer)    TEST_ASSERT_MESSAGE((pointer != NULL), "Expected Non-NULL")

/*-------------------------------------------------------
 * Test Asserts (integers)
 *-------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8)(expected), (UNITY_INT)(UNITY_UINT8)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT8)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT16)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT16)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT32)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT32)

/*-------------------------------------------------------
 * Test Asserts (size_t)
 *-------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_size_t(expected, actual) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT)

/*-------------------------------------------------------
 * Test Asserts (Strings)
 *-------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    UnityAssertEqualString((const char*)(expected), (const char*)(actual), NULL, (UNITY_LINE_TYPE)__LINE__)

#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message) \
    UnityAssertEqualString((const char*)(expected), (const char*)(actual), (message), (UNITY_LINE_TYPE)__LINE__)

/*-------------------------------------------------------
 * Test Asserts (Arrays)
 *-------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements) \
    UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_INT, UNITY_ARRAY_TO_ARRAY)

#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements) \
    UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_DISPLAY_STYLE_UINT8, UNITY_ARRAY_TO_ARRAY)

/*-------------------------------------------------------
 * Test Asserts (Memory)
 *-------------------------------------------------------*/
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    UnityAssertEqualMemory((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(len), 1, NULL, (UNITY_LINE_TYPE)__LINE__, UNITY_ARRAY_TO_ARRAY)

/*-------------------------------------------------------
 * Test Asserts (Floating Point)
 *-------------------------------------------------------*/
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) \
    UnityAssertFloatsWithin((UNITY_FLOAT)(delta), (UNITY_FLOAT)(expected), (UNITY_FLOAT)(actual), NULL, (UNITY_LINE_TYPE)__LINE__)

#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual) \
    UnityAssertDoublesWithin((UNITY_DOUBLE)(delta), (UNITY_DOUBLE)(expected), (UNITY_DOUBLE)(actual), NULL, (UNITY_LINE_TYPE)__LINE__)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FRAMEWORK_H */
