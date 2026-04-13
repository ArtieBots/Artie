/* =========================================================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
============================================================================ */

#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stddef.h>

#ifdef UNITY_INCLUDE_CONFIG_H
#include "unity_config.h"
#endif

/*-------------------------------------------------------
 * Int Support
 *-------------------------------------------------------*/
#ifndef UNITY_INT_WIDTH
#define UNITY_INT_WIDTH (32)
#endif

#if (UNITY_INT_WIDTH == 32)
    typedef unsigned char   UNITY_UINT8;
    typedef unsigned short  UNITY_UINT16;
    typedef unsigned int    UNITY_UINT32;
    typedef signed char     UNITY_INT8;
    typedef signed short    UNITY_INT16;
    typedef signed int      UNITY_INT32;
#elif (UNITY_INT_WIDTH == 16)
    typedef unsigned char   UNITY_UINT8;
    typedef unsigned int    UNITY_UINT16;
    typedef unsigned long   UNITY_UINT32;
    typedef signed char     UNITY_INT8;
    typedef signed int      UNITY_INT16;
    typedef signed long     UNITY_INT32;
#else
    #error Invalid UNITY_INT_WIDTH specified! (16 or 32 are supported)
#endif

typedef UNITY_INT32 UNITY_INT;
typedef UNITY_UINT32 UNITY_UINT;

/*-------------------------------------------------------
 * 64-bit Support
 *-------------------------------------------------------*/
#ifndef UNITY_SUPPORT_64
#ifdef UNITY_EXCLUDE_STDINT_H
    #define UNITY_SUPPORT_64
#else
    #include <stdint.h>
    #define UNITY_SUPPORT_64
#endif
#endif

/*-------------------------------------------------------
 * Pointer Support
 *-------------------------------------------------------*/
#ifndef UNITY_POINTER_WIDTH
#if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
    #define UNITY_POINTER_WIDTH (64)
#else
    #define UNITY_POINTER_WIDTH (32)
#endif
#endif

#if (UNITY_POINTER_WIDTH == 32)
    typedef UNITY_UINT32 UNITY_PTR_TO_INT;
#elif (UNITY_POINTER_WIDTH == 64)
    typedef unsigned long long UNITY_PTR_TO_INT;
#elif (UNITY_POINTER_WIDTH == 16)
    typedef UNITY_UINT16 UNITY_PTR_TO_INT;
#else
    #error Invalid UNITY_POINTER_WIDTH specified!
#endif

#ifndef UNITY_PTR_ATTRIBUTE
    #define UNITY_PTR_ATTRIBUTE
#endif

#ifndef UNITY_INTERNAL_PTR
    #define UNITY_INTERNAL_PTR void*
#endif

/*-------------------------------------------------------
 * Float Support
 *-------------------------------------------------------*/
#ifndef UNITY_EXCLUDE_FLOAT
    #define UNITY_FLOAT float
#else
    #undef UNITY_INCLUDE_FLOAT
#endif

#ifndef UNITY_EXCLUDE_DOUBLE
    #define UNITY_DOUBLE double
#else
    #undef UNITY_INCLUDE_DOUBLE
#endif

/*-------------------------------------------------------
 * Output Method
 *-------------------------------------------------------*/
#ifndef UNITY_OUTPUT_CHAR
    #include <stdio.h>
    #define UNITY_OUTPUT_CHAR(a) (void)putchar(a)
#endif

#ifndef UNITY_OUTPUT_FLUSH
    #ifdef UNITY_USE_FLUSH_STDOUT
        #include <stdio.h>
        #define UNITY_OUTPUT_FLUSH() (void)fflush(stdout)
    #else
        #define UNITY_OUTPUT_FLUSH()
    #endif
#endif

#ifndef UNITY_OUTPUT_START
    #define UNITY_OUTPUT_START()
#endif

#ifndef UNITY_OUTPUT_COMPLETE
    #define UNITY_OUTPUT_COMPLETE()
#endif

/*-------------------------------------------------------
 * Footprint
 *-------------------------------------------------------*/
#ifndef UNITY_LINE_TYPE
    #define UNITY_LINE_TYPE unsigned short
#endif

#ifndef UNITY_COUNTER_TYPE
    #define UNITY_COUNTER_TYPE unsigned short
#endif

/*-------------------------------------------------------
 * Language Features
 *-------------------------------------------------------*/
#ifndef UNITY_WEAK_ATTRIBUTE
    #if defined(__GNUC__) || defined(__ghs__)
        #define UNITY_WEAK_ATTRIBUTE __attribute__((weak))
    #else
        #define UNITY_WEAK_ATTRIBUTE
    #endif
#endif

#ifndef UNITY_WEAK_PRAGMA
    #define UNITY_WEAK_PRAGMA
#endif

/*-------------------------------------------------------
 * Internal Types
 *-------------------------------------------------------*/
typedef void (*UnityTestFunction)(void);

typedef enum
{
    UNITY_DISPLAY_STYLE_INT,
    UNITY_DISPLAY_STYLE_INT8,
    UNITY_DISPLAY_STYLE_INT16,
    UNITY_DISPLAY_STYLE_INT32,
    UNITY_DISPLAY_STYLE_UINT,
    UNITY_DISPLAY_STYLE_UINT8,
    UNITY_DISPLAY_STYLE_UINT16,
    UNITY_DISPLAY_STYLE_UINT32,
    UNITY_DISPLAY_STYLE_HEX8,
    UNITY_DISPLAY_STYLE_HEX16,
    UNITY_DISPLAY_STYLE_HEX32,
    UNITY_DISPLAY_STYLE_UNKNOWN
} UNITY_DISPLAY_STYLE_T;

typedef enum
{
    UNITY_ARRAY_TO_VAL = 0,
    UNITY_ARRAY_TO_ARRAY,
    UNITY_ARRAY_UNKNOWN
} UNITY_FLAGS_T;

typedef enum
{
    UNITY_ARRAY_START = 0
} UNITY_INTERNAL_CONTEXT;

struct UNITY_STORAGE_T
{
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    UNITY_COUNTER_TYPE TestIgnores;
    UNITY_COUNTER_TYPE CurrentTestFailed;
    UNITY_COUNTER_TYPE CurrentTestIgnored;
    jmp_buf AbortFrame;
};

extern struct UNITY_STORAGE_T Unity;

/*-------------------------------------------------------
 * Test Suite Management
 *-------------------------------------------------------*/
void UnityBegin(const char* filename);
int UnityEnd(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

/*-------------------------------------------------------
 * Details Support
 *-------------------------------------------------------*/
void UnityPrint(const char* string);
void UnityPrintMask(const UNITY_UINT mask, const UNITY_UINT number);
void UnityPrintNumberByStyle(const UNITY_INT number, const UNITY_DISPLAY_STYLE_T style);
void UnityPrintNumber(const UNITY_INT number_to_print);
void UnityPrintNumberUnsigned(const UNITY_UINT number);
void UnityPrintNumberHex(const UNITY_UINT number, const char nibbles_to_print);

#ifndef UNITY_EXCLUDE_FLOAT_PRINT
void UnityPrintFloat(const UNITY_DOUBLE input_number);
#endif

/*-------------------------------------------------------
 * Test Output
 *-------------------------------------------------------*/
void UnityPrintLen(const char* string, const UNITY_UINT32 length);
void UnityPrintMask(const UNITY_UINT mask, const UNITY_UINT number);
void UnityTestResultsBegin(const char* file, const UNITY_LINE_TYPE line);
void UnityTestResultsFailBegin(const UNITY_LINE_TYPE line);

void UnityMessage(const char* message, const UNITY_LINE_TYPE line);

/*-------------------------------------------------------
 * Display Ranges
 *-------------------------------------------------------*/
#define UNITY_DISPLAY_RANGE_INT  (0x10)
#define UNITY_DISPLAY_RANGE_UINT (0x20)
#define UNITY_DISPLAY_RANGE_HEX  (0x40)

#define UNITY_MAX_NIBBLES (16)

#endif /* UNITY_INTERNALS_H */
