/* =========================================================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
============================================================================ */

#include "unity.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Global Unity structure */
struct UNITY_STORAGE_T Unity;

/* Error messages */
const char UnityStrErrFloat[]  = "Unity Floating Point Disabled";
const char UnityStrErrDouble[] = "Unity Double Precision Disabled";
const char UnityStrErr64[]     = "Unity 64-bit Support Disabled";
const char UnityStrNull[]      = "NULL";
const char UnityStrSpacer[]    = ". ";
const char UnityStrExpected[]  = " Expected ";
const char UnityStrWas[]       = " Was ";
const char UnityStrElement[]   = " Element ";
const char UnityStrMemory[]    = " Memory Mismatch.";

/*-----------------------------------------------
 * Pretty Printers & Test Result Output Handlers
 *-----------------------------------------------*/
void UnityPrint(const char* string)
{
    const char* pch = string;
    if (pch != NULL)
    {
        while (*pch)
        {
            UNITY_OUTPUT_CHAR(*pch);
            pch++;
        }
    }
}

void UnityPrintLen(const char* string, const UNITY_UINT32 length)
{
    const char* pch = string;
    if (pch != NULL)
    {
        for (UNITY_UINT32 i = 0; i < length; i++)
        {
            UNITY_OUTPUT_CHAR(*pch);
            pch++;
        }
    }
}

void UnityPrintNumberByStyle(const UNITY_INT number, const UNITY_DISPLAY_STYLE_T style)
{
    if ((style & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT)
    {
        UnityPrintNumber(number);
    }
    else if ((style & UNITY_DISPLAY_RANGE_UINT) == UNITY_DISPLAY_RANGE_UINT)
    {
        UnityPrintNumberUnsigned((UNITY_UINT)number);
    }
    else
    {
        UnityPrintNumberHex((UNITY_UINT)number, (char)((style & 0x000F) * 2));
    }
}

void UnityPrintNumber(const UNITY_INT number_to_print)
{
    UNITY_UINT number = (UNITY_UINT)number_to_print;
    if (number_to_print < 0)
    {
        UNITY_OUTPUT_CHAR('-');
        number = (UNITY_UINT)(-number_to_print);
    }
    UnityPrintNumberUnsigned(number);
}

void UnityPrintNumberUnsigned(const UNITY_UINT number)
{
    UNITY_UINT divisor = 1;
    UNITY_UINT next_divisor;
    UNITY_UINT number_to_print = number;

    if (number_to_print == 0)
    {
        UNITY_OUTPUT_CHAR('0');
        return;
    }

    while (number_to_print / divisor >= 10)
    {
        next_divisor = divisor * 10;
        if (next_divisor < divisor)
            break; /* prevent overflow */
        divisor = next_divisor;
    }

    while (divisor > 0)
    {
        UNITY_OUTPUT_CHAR((char)('0' + (number_to_print / divisor % 10)));
        divisor /= 10;
    }
}

void UnityPrintNumberHex(const UNITY_UINT number, const char nibbles_to_print)
{
    int nibble;
    char nibbles = nibbles_to_print;

    UNITY_OUTPUT_CHAR('0');
    UNITY_OUTPUT_CHAR('x');

    while (nibbles > 0)
    {
        nibbles--;
        nibble = (int)(number >> (nibbles * 4)) & 0x0F;
        if (nibble <= 9)
        {
            UNITY_OUTPUT_CHAR((char)('0' + nibble));
        }
        else
        {
            UNITY_OUTPUT_CHAR((char)('A' - 10 + nibble));
        }
    }
}

void UnityPrintMask(const UNITY_UINT mask, const UNITY_UINT number)
{
    UNITY_UINT current_bit = (UNITY_UINT)1 << (sizeof(UNITY_UINT) * 8 - 1);
    UNITY_INT32 i;

    for (i = 0; i < (UNITY_INT32)(sizeof(UNITY_UINT) * 8); i++)
    {
        if (current_bit & mask)
        {
            if (current_bit & number)
            {
                UNITY_OUTPUT_CHAR('1');
            }
            else
            {
                UNITY_OUTPUT_CHAR('0');
            }
        }
        else
        {
            UNITY_OUTPUT_CHAR('X');
        }
        current_bit = current_bit >> 1;
    }
}

#ifndef UNITY_EXCLUDE_FLOAT_PRINT
void UnityPrintFloat(const UNITY_DOUBLE input_number)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", input_number);
    UnityPrint(buf);
}
#endif

/*-----------------------------------------------
 * Test Running
 *-----------------------------------------------*/
void UnityBegin(const char* filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;

    UNITY_OUTPUT_START();
}

int UnityEnd(void)
{
    UNITY_OUTPUT_CHAR('\n');
    UnityPrint("-----------------------");
    UNITY_OUTPUT_CHAR('\n');
    UnityPrintNumber((UNITY_INT)(Unity.NumberOfTests));
    UnityPrint(" Tests ");
    UnityPrintNumber((UNITY_INT)(Unity.TestFailures));
    UnityPrint(" Failures ");
    UnityPrintNumber((UNITY_INT)(Unity.TestIgnores));
    UnityPrint(" Ignored");
    UNITY_OUTPUT_CHAR('\n');

    UNITY_OUTPUT_COMPLETE();

    if (Unity.TestFailures == 0U)
    {
        UnityPrint("OK");
    }
    else
    {
        UnityPrint("FAIL");
    }

    UNITY_OUTPUT_CHAR('\n');
    return (int)(Unity.TestFailures);
}

void UnityConcludeTest(void)
{
    if (Unity.CurrentTestIgnored)
    {
        Unity.TestIgnores++;
        UnityPrint("IGNORE");
    }
    else if (!Unity.CurrentTestFailed)
    {
        UnityPrint("PASS");
    }
    else
    {
        Unity.TestFailures++;
    }

    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
    UNITY_OUTPUT_CHAR('\n');
    UNITY_OUTPUT_FLUSH();
}

void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum)
{
    Unity.CurrentTestName = FuncName;
    Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)FuncLineNum;
    Unity.NumberOfTests++;

    if (TEST_PROTECT())
    {
        Func();
    }
    UnityConcludeTest();
}

/*-----------------------------------------------
 * Test Assertion Primitives
 *-----------------------------------------------*/
void UnityTestResultsBegin(const char* file, const UNITY_LINE_TYPE line)
{
    UnityPrint(file);
    UNITY_OUTPUT_CHAR(':');
    UnityPrintNumber((UNITY_INT)line);
    UNITY_OUTPUT_CHAR(':');
    UnityPrint(Unity.CurrentTestName);
    UNITY_OUTPUT_CHAR(':');
}

void UnityTestResultsFailBegin(const UNITY_LINE_TYPE line)
{
    UnityTestResultsBegin(Unity.TestFile, line);
    UnityPrint("FAIL:");
}

void UnityFail(const char* msg, const UNITY_LINE_TYPE line)
{
    UNITY_OUTPUT_CHAR('\n');
    UnityTestResultsFailBegin(line);

    if (msg != NULL)
    {
        UnityPrint(msg);
    }

    Unity.CurrentTestFailed = 1;
    longjmp(Unity.AbortFrame, 1);
}

void UnityIgnore(const char* msg, const UNITY_LINE_TYPE line)
{
    UNITY_OUTPUT_CHAR('\n');
    UnityTestResultsBegin(Unity.TestFile, line);
    UnityPrint("IGNORE:");

    if (msg != NULL)
    {
        UnityPrint(msg);
    }

    Unity.CurrentTestIgnored = 1;
    longjmp(Unity.AbortFrame, 1);
}

void UnityMessage(const char* message, const UNITY_LINE_TYPE line)
{
    UNITY_OUTPUT_CHAR('\n');
    UnityTestResultsBegin(Unity.TestFile, line);
    UnityPrint("INFO:");
    if (message != NULL)
    {
        UnityPrint(message);
    }
}

void UnityAssertEqualNumber(const UNITY_INT expected,
                            const UNITY_INT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_INTERNAL_CONTEXT style)
{
    (void)style; /* Unused parameter */

    if (expected != actual)
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrExpected);
        UnityPrintNumber(expected);
        UnityPrint(UnityStrWas);
        UnityPrintNumber(actual);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected,
                              UNITY_INTERNAL_PTR actual,
                              const UNITY_UINT32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_INTERNAL_CONTEXT style,
                              const UNITY_FLAGS_T flags)
{
    UNITY_UINT32 elements = num_elements;
    UNITY_INTERNAL_PTR ptr_exp = (UNITY_INTERNAL_PTR)expected;
    UNITY_INTERNAL_PTR ptr_act = (UNITY_INTERNAL_PTR)actual;

    (void)flags; /* Unused */

    if (elements == 0)
    {
        return;
    }

    if (expected == actual)
    {
        return; /* Both are NULL or same pointer */
    }

    if (expected == NULL || actual == NULL)
    {
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint("Expected Non-NULL");
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }

    /* Compare arrays based on display style */
    while (elements--)
    {
        UNITY_INT expect_val;
        UNITY_INT actual_val;

        switch (style)
        {
            case UNITY_DISPLAY_STYLE_UINT8:
                expect_val = *(UNITY_UINT8*)ptr_exp;
                actual_val = *(UNITY_UINT8*)ptr_act;
                ptr_exp = (UNITY_INTERNAL_PTR)((UNITY_UINT8*)ptr_exp + 1);
                ptr_act = (UNITY_INTERNAL_PTR)((UNITY_UINT8*)ptr_act + 1);
                break;
            default:
                expect_val = *(UNITY_INT*)ptr_exp;
                actual_val = *(UNITY_INT*)ptr_act;
                ptr_exp = (UNITY_INTERNAL_PTR)((UNITY_INT*)ptr_exp + 1);
                ptr_act = (UNITY_INTERNAL_PTR)((UNITY_INT*)ptr_act + 1);
                break;
        }

        if (expect_val != actual_val)
        {
            UNITY_OUTPUT_CHAR('\n');
            UnityTestResultsFailBegin(lineNumber);
            UnityPrint(UnityStrElement);
            UnityPrintNumber((UNITY_INT)(num_elements - elements - 1));
            UnityPrint(UnityStrExpected);
            UnityPrintNumber(expect_val);
            UnityPrint(UnityStrWas);
            UnityPrintNumber(actual_val);
            if (msg)
            {
                UnityPrint(UnityStrSpacer);
                UnityPrint(msg);
            }
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
    }
}

void UnityAssertBits(const UNITY_INT mask,
                     const UNITY_INT expected,
                     const UNITY_INT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber)
{
    if ((mask & expected) != (mask & actual))
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrExpected);
        UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)expected);
        UnityPrint(UnityStrWas);
        UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)actual);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber)
{
    UNITY_UINT32 i;

    /* Handle NULL strings */
    if (expected == actual)
    {
        return;
    }

    if (expected == NULL || actual == NULL)
    {
        UnityTestResultsFailBegin(lineNumber);
        if (expected == NULL)
            UnityPrint("Expected was NULL");
        else
            UnityPrint("Actual was NULL");
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }

    /* Compare strings */
    i = 0;
    while (expected[i] || actual[i])
    {
        if (expected[i] != actual[i])
        {
            UNITY_OUTPUT_CHAR('\n');
            UnityTestResultsFailBegin(lineNumber);
            UnityPrint(UnityStrExpected);
            UnityPrint(expected);
            UnityPrint(UnityStrWas);
            UnityPrint(actual);
            if (msg)
            {
                UnityPrint(UnityStrSpacer);
                UnityPrint(msg);
            }
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
        i++;
    }
}

void UnityAssertEqualStringArray(UNITY_INTERNAL_PTR expected,
                                 const char** actual,
                                 const UNITY_UINT32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber,
                                 const UNITY_FLAGS_T flags)
{
    (void)flags;
    const char** ptr_exp = (const char**)expected;
    const char** ptr_act = actual;
    UNITY_UINT32 i;

    for (i = 0; i < num_elements; i++)
    {
        UnityAssertEqualString(ptr_exp[i], ptr_act[i], msg, lineNumber);
    }
}

void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected,
                            UNITY_INTERNAL_PTR actual,
                            const UNITY_UINT32 length,
                            const UNITY_UINT32 num_elements,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const UNITY_FLAGS_T flags)
{
    (void)flags;
    (void)num_elements;

    if (expected == actual)
    {
        return;
    }

    if (expected == NULL || actual == NULL)
    {
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint("Expected Non-NULL");
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }

    if (memcmp(expected, actual, length) != 0)
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrMemory);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertNumbersWithin(const UNITY_UINT delta,
                              const UNITY_INT expected,
                              const UNITY_INT actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_INTERNAL_CONTEXT style)
{
    (void)style;
    UNITY_UINT diff;

    if (expected > actual)
        diff = (UNITY_UINT)(expected - actual);
    else
        diff = (UNITY_UINT)(actual - expected);

    if (diff > delta)
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrExpected);
        UnityPrintNumber(expected);
        UnityPrint(" +/- ");
        UnityPrintNumber((UNITY_INT)delta);
        UnityPrint(UnityStrWas);
        UnityPrintNumber(actual);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertFloatsWithin(const UNITY_FLOAT delta,
                             const UNITY_FLOAT expected,
                             const UNITY_FLOAT actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber)
{
#ifdef UNITY_EXCLUDE_FLOAT
    (void)delta; (void)expected; (void)actual; (void)msg;
    UnityFail(UnityStrErrFloat, lineNumber);
#else
    UNITY_FLOAT diff = expected - actual;
    UNITY_FLOAT pos_delta = delta;

    if (diff < 0.0f)
    {
        diff = -diff;
    }
    if (pos_delta < 0.0f)
    {
        pos_delta = -pos_delta;
    }

    if (diff > pos_delta)
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrExpected);
        UnityPrintFloat((UNITY_DOUBLE)expected);
        UnityPrint(" +/- ");
        UnityPrintFloat((UNITY_DOUBLE)delta);
        UnityPrint(UnityStrWas);
        UnityPrintFloat((UNITY_DOUBLE)actual);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
#endif
}

void UnityAssertDoublesWithin(const UNITY_DOUBLE delta,
                              const UNITY_DOUBLE expected,
                              const UNITY_DOUBLE actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber)
{
#ifdef UNITY_EXCLUDE_DOUBLE
    (void)delta; (void)expected; (void)actual; (void)msg;
    UnityFail(UnityStrErrDouble, lineNumber);
#else
    UNITY_DOUBLE diff = expected - actual;
    UNITY_DOUBLE pos_delta = delta;

    if (diff < 0.0)
    {
        diff = -diff;
    }
    if (pos_delta < 0.0)
    {
        pos_delta = -pos_delta;
    }

    if (diff > pos_delta)
    {
        UNITY_OUTPUT_CHAR('\n');
        UnityTestResultsFailBegin(lineNumber);
        UnityPrint(UnityStrExpected);
        UnityPrintFloat(expected);
        UnityPrint(" +/- ");
        UnityPrintFloat(delta);
        UnityPrint(UnityStrWas);
        UnityPrintFloat(actual);
        if (msg)
        {
            UnityPrint(UnityStrSpacer);
            UnityPrint(msg);
        }
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
#endif
}

/*-----------------------------------------------
 * Helpers
 *-----------------------------------------------*/
UNITY_INTERNAL_PTR UnityNumToPtr(const UNITY_INT num, const UNITY_UINT8 size)
{
    (void)num;
    (void)size;
    return NULL;
}

#ifndef UNITY_EXCLUDE_FLOAT
UNITY_INTERNAL_PTR UnityFloatToPtr(const float num)
{
    (void)num;
    return NULL;
}
#endif

#ifndef UNITY_EXCLUDE_DOUBLE
UNITY_INTERNAL_PTR UnityDoubleToPtr(const double num)
{
    (void)num;
    return NULL;
}
#endif
