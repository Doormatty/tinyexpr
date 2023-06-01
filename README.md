# TinyIntegerExpr

TinyIntegerExpr is a very small recursive descent parser and evaluation engine for
 _**integer**_ math expressions. It's handy when you want to add the ability to evaluate
_**integer**_ math expressions at runtime without adding a bunch of cruft to your project.

In addition to the standard math operators and precedence, TinyIntegerExpr also supports
the standard C math functions and runtime binding of variables.

## Features

- **C99 with no dependencies**.
- Single source file and header file.
- Simple and fast.
- Implements standard operators precedence.
- Can add custom functions and variables easily.
- Can bind variables at eval-time.
- Released under the zlib license - free for nearly any use.
- Easy to use and integrate with your code
- Thread-safe, provided that your *malloc* is.

## Building

TinyIntegerExpr is self-contained in two files: `tinyintegerexpr.c` and `tinyintegerexpr.h`. To use
TinyIntegerExpr, simply add those two files to your project.

## Short Example

Here is a minimal example to evaluate an expression at runtime.

```C
    #include "tinyintegerexpr.h"
    printf("%f\n", tie_interp("5*5", 0)); /* Prints 25. */
```


## Usage

TinyIntegerExpr defines only four functions:

```C
    double tie_interp(const char *expression, int *error);
    tie_expression *tie_compile(const char *expression, const tie_variable *variables, int var_count, int *error);
    double tie_eval(const tie_expression *expr);
    void tie_free(tie_expression *expr);
```

## tie_interp
```C
    double tie_interp(const char *expression, int *error);
```

`tie_interp()` takes an expression and immediately returns the result of it. If there
is a parse error, `tie_interp()` returns NaN.

If the `error` pointer argument is not 0, then `tie_interp()` will set `*error` to the position
of the parse error on failure, and set `*error` to 0 on success.

**example usage:**

```C
    int error;

    double a = tie_interp("(5+5)", 0); /* Returns 10. */
    double b = tie_interp("(5+5)", &error); /* Returns 10, error is set to 0. */
    double c = tie_interp("(5+5", &error); /* Returns NaN, error is set to 4. */
```

## tie_compile, tie_eval, tie_free
```C
    tie_expression *tie_compile(const char *expression, const tie_variable *lookup, int lookup_len, int *error);
    double tie_eval(const tie_expression *n);
    void tie_free(tie_expression *n);
```

Give `tie_compile()` an expression with unbound variables and a list of
variable names and pointers. `tie_compile()` will return a `tie_expression*` which can
be evaluated later using `tie_eval()`. On failure, `tie_compile()` will return 0
and optionally set the passed in `*error` to the location of the parse error.

You may also compile expressions without variables by passing `tie_compile()`'s second
and third arguments as 0.

Give `tie_eval()` a `tie_expression*` from `tie_compile()`. `tie_eval()` will evaluate the expression
using the current variable values.

After you're finished, make sure to call `tie_free()`.

**example usage:**

```C
    double x, y;
    /* Store variable names and pointers. */
    tie_variable vars[] = {{"x", &x}, {"y", &y}};

    int err;
    /* Compile the expression with variables. */
    tie_expression *expr = tie_compile("(x*2+y*2)", vars, 2, &err);

    if (expr) {
        x = 3; y = 4;
        const double h1 = tie_eval(expr); /* Returns 5. */

        x = 5; y = 12;
        const double h2 = tie_eval(expr); /* Returns 13. */

        tie_free(expr);
    } else {
        printf("Parse error at %d\n", err);
    }

```

## Longer Example

Here is a complete example that will evaluate an expression passed in from the command
line. It also does error checking and binds the variables `x` and `y` to *3* and *4*, respectively.

```C
    #include "tinyintegerexpr.h"
    #include <stdio.h>

    int main(int argc, char *argv[])
    {
        if (argc < 2) {
            printf("Usage: example2 \"expression\"\n");
            return 0;
        }

        const char *expression = argv[1];
        printf("Evaluating:\n\t%s\n", expression);

        /* This shows an example where the variables
         * x and y are bound at eval-time. */
        double x, y;
        tie_variable vars[] = {{"x", &x}, {"y", &y}};

        /* This will compile the expression and check for errors. */
        int err;
        tie_expression *n = tie_compile(expression, vars, 2, &err);

        if (n) {
            /* The variables can be changed here, and eval can be called as many
             * times as you like. This is fairly efficient because the parsing has
             * already been done. */
            x = 3; y = 4;
            const double r = tie_eval(n); printf("Result:\n\t%f\n", r);
            tie_free(n);
        } else {
            /* Show the user where the error is at. */
            printf("\t%*s^\nError near here", err-1, "");
        }

        return 0;
    }
```


This produces the output:

    $ example2 "(x*2+y2)"
        Evaluating:
                (x*2+y2)
                      ^
        Error near here


    $ example2 "(x*2+y*2)"
        Evaluating:
                (x*2+y*2)
        Result:
                14


## Binding to Custom Functions

TinyIntegerExpr can also call to custom functions implemented in C. Here is a short example:

```C
double my_sum(double a, double b) {
    /* Example C function that adds two numbers together. */
    return a + b;
}

tie_variable vars[] = {
    {"mysum", my_sum, TE_FUNCTION2} /* TIE_FUNCTION2 used because my_sum takes two arguments. */
};

tie_expression *n = tie_compile("mysum(5, 6)", vars, 1, 0);

```
## Speed


TinyIntegerExpr is pretty fast compared to C when the expression is short, when the
expression does hard calculations (e.g. exponentiation), and when some of the
work can be simplified by `tie_compile()`. TinyIntegerExpr is slow compared to C when the
expression is long and involves only basic arithmetic.

Here are some made up performance numbers taken from the included
**benchmark.c** program:

| Expression | tie_eval time | native C time | slowdown  |
| :------------- |-------------:| -----:|----:|
| a+5 | 765 ms | 563 ms | 36% slower |
| a+(5*2) | 765 ms | 563 ms | 36% slower |
| (a+5)*2 | 1422 ms | 563 ms | 153% slower |
| (1/(a+1)+2/(a+2)+3/(a+3)) | 5,516 ms | 1,266 ms | 336% slower |



## Grammar

TinyIntegerExpr parses the following grammar:

    <list>      =    <expr> {"," <expr>}
    <expr>      =    <term> {("+" | "-") <term>}
    <term>      =    <shift> {("*" | "/" | "%") <shift>}
    <shift>     =    <bitwise> {("<" | ">") <bitwise>}
    <bitwise>   =    <power> {("&" | "|" | "^" ) <power>}
    <power>     =    {("-" | "+")} <base>
    <base>      =    <constant>
                   | <variable>
                   | <function-0> {"(" ")"}
                   | <function-1> <power>
                   | <function-X> "(" <expr> {"," <expr>} ")"
                   | "(" <list> ")"

In addition, whitespace between tokens is ignored.

Valid variable names consist of a letter followed by any combination of:
letters, the digits *0* through *9*, and underscore. Constants **_must_** be integers, or in scientific notation (e.g.  *1e3* for *1000*). 

## Functions supported

TinyIntegerExpr supports:
* addition (`+`)
* subtraction/negation (`-`) 
* multiplication (`*`)
* division (`/`)
* modulus (`%`) 
* Bitwise AND (`&`)
* Bitwise OR (`|`)
* Bitwise XOR (`^`)
* Right Shift (`>`)
* Left Shift (`<`)
* 
with the normal operator precedence

## Hints

- All functions/types start with the letters *tie*.

- To allow constant optimization, surround constant expressions in parentheses.
  For example "x+(1+5)" will evaluate the "(1+5)" expression at compile time and
  compile the entire expression as "x+6", saving a runtime calculation. The
  parentheses are important, because TinyIntegerExpr will not change the order of
  evaluation. If you instead compiled "x+1+5" TinyIntegerExpr will insist that "1" is
  added to "x" first, and "5" is added the result second.

