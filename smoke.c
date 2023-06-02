/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015-2020 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "tinyintegerexpr.h"
#include <stdio.h>
#include "minctest.h"


typedef struct {
  const char *expr;
  int answer;
} test_case;

typedef struct {
  const char *expr1;
  const char *expr2;
} test_equ;


void test_results() {
  test_case cases[] = {
      {"1", 1},
      {"1 ", 1},
      {"(1)", 1},

      {"2+1", 2 + 1},
      {"(((2+(1))))", 2 + 1},
      {"3+2", 3 + 2},

      {"3+2+4", 3 + 2 + 4},
      {"(3+2)+4", 3 + 2 + 4},
      {"3+(2+4)", 3 + 2 + 4},
      {"(3+2+4)", 3 + 2 + 4},

      {"3*2*4", 3 * 2 * 4},
      {"(3*2)*4", 3 * 2 * 4},
      {"3*(2*4)", 3 * 2 * 4},
      {"(3*2*4)", 3 * 2 * 4},

      {"3-2-4", 3 - 2 - 4},
      {"(3-2)-4", (3 - 2) - 4},
      {"3-(2-4)", 3 - (2 - 4)},
      {"(3-2-4)", 3 - 2 - 4},

      {"3/2/4", 3 / 2 / 4},
      {"(3/2)/4", (3 / 2) / 4},
      {"3/(2/4)", 3 / (2 / 4)},
      {"(3/2/4)", 3 / 2 / 4},

      {"(3*2/4)", 3 * 2 / 4},
      {"(3/2*4)", 3 / 2 * 4},
      {"3*(2/4)", 3 * (2 / 4)},
      {"10^5*5e-5", 5},
      {"1,2", 2},
      {"1,2+1", 3},
      {"1+1,2+2,2+1", 3},
      {"1,2,3", 3},
      {"(1,2),3", 3},
      {"1,(2,3)", 3},
      {"-(1,(2,3))", -3},
  };


  int i;
  for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
    const char *expr = cases[i].expr;
    const int answer = cases[i].answer;

    int err;
    const int ev = tie_interp(expr, &err);
    lok(!err);
    lfequal(ev, answer);

    if (err) {
      printf("FAILED: %s (%d)\n", expr, err);
    }
  }
}


void test_syntax() {
  test_case errors[] = {
      {"",         1},
      {"1+",       2},
      {"1)",       2},
      {"(1",       2},
      {"1**1",     3},
      {"1*2(+4",   4},
      {"1*2(1+4",  4},
      {"a+5",      1},
      {"!+5",      1},
      {"_a+5",     1},
      {"#a+5",     1},
      {"1^^5",     3},
      {"1**5",     3},
      {"sin(cos5", 8},
  };


  int i;
  for (i = 0; i < sizeof(errors) / sizeof(test_case); ++i) {
    const char *expr = errors[i].expr;
    const int e = errors[i].answer;

    int err;
    const int r = tie_interp(expr, &err);
    lequal(err, e);
    lok(r != r);

    tie_expression *n = tie_compile(expr, 0, 0, &err);
    lequal(err, e);
    lok(!n);

    if (err != e) {
      printf("FAILED: %s\n", expr);
    }

    const int k = tie_interp(expr, 0);
    lok(k != k);
  }
}


void test_nans() {

  const char *nans[] = {
      "0/0",
      "1%0",
      "1%(1%0)",
      "(1%0)%1",
  };

  int i;
  for (i = 0; i < sizeof(nans) / sizeof(const char *); ++i) {
    const char *expr = nans[i];

    int err;
    const int r = tie_interp(expr, &err);
    lequal(err, 0);
    lok(r != r);

    tie_expression *n = tie_compile(expr, 0, 0, &err);
    lok(n);
    lequal(err, 0);
    const int c = tie_eval(n);
    lok(c != c);
    tie_free(n);
  }
}


void test_infs() {

  const char *infs[] = {
      "1/0",
  };

  int i;
  for (i = 0; i < sizeof(infs) / sizeof(const char *); ++i) {
    const char *expr = infs[i];

    int err;
    const int r = tie_interp(expr, &err);
    lequal(err, 0);
    lok(r == r + 1);

    tie_expression *n = tie_compile(expr, 0, 0, &err);
    lok(n);
    lequal(err, 0);
    const int c = tie_eval(n);
    lok(c == c + 1);
    tie_free(n);
  }
}


void test_variables() {

  int x, y, test;
  tie_variable lookup[] = {{"x",      &x},
                           {"y",      &y},
                           {"tie_st", &test}};

  int err;

  tie_expression *expr1 = tie_compile("x + y", lookup, 2, &err);
  lok(expr1);
  lok(!err);

  tie_expression *expr2 = tie_compile("x+x+x-y", lookup, 2, &err);
  lok(expr2);
  lok(!err);

  tie_expression *expr3 = tie_compile("x*y^3", lookup, 2, &err);
  lok(expr3);
  lok(!err);

  tie_expression *expr4 = tie_compile("tie_st+5", lookup, 3, &err);
  lok(expr4);
  lok(!err);

  for (y = 2; y < 3; ++y) {
    for (x = 0; x < 5; ++x) {
      int ev;

      ev = tie_eval(expr1);
      lfequal(ev, x + y);

      ev = tie_eval(expr2);
      lfequal(ev, x + x + x - y);

      ev = tie_eval(expr3);
      lfequal(ev, x * y ^ 3);

      test = x;
      ev = tie_eval(expr4);
      lfequal(ev, x + 5);
    }
  }

  tie_free(expr1);
  tie_free(expr2);
  tie_free(expr3);
  tie_free(expr4);


  tie_expression *expr5 = tie_compile("xx*y^3", lookup, 2, &err);
  lok(!expr5);
  lok(err);

  tie_expression *expr6 = tie_compile("tes", lookup, 3, &err);
  lok(!expr6);
  lok(err);

  tie_expression *expr7 = tie_compile("if(1,x,y)", lookup, 2, &err);
  lok(!expr7);
  lok(err);

  tie_expression *expr8 = tie_compile("x<<y", lookup, 2, &err);
  lok(!expr8);
  lok(err);
}


#define cross_check(a, b) do {\
    if ((b)!=(b)) break;\
    expr = tie_compile((a), lookup, 2, &err);\
    lfequal(tie_eval(expr), (b));\
    lok(!err);\
    tie_free(expr);\
}while(0)

void test_functions() {

  int x, y;
  tie_variable lookup[] = {{"x", &x},
                           {"y", &y}};

  int err;
  tie_expression *expr;

  for (x = -5; x < 5; x++) {
    cross_check("abs x", abs(x));

    for (y = -2; y < 2; y++) {
      cross_check("if(x,y,-1)", x ? y : -99);
    }
  }
}


int sum0() {
  return 6;
}

int sum1(int a) {
  return a * 2;
}

int sum2(int a, int b) {
  return a + b;
}

int sum3(int a, int b, int c) {
  return a + b + c;
}

int sum4(int a, int b, int c, int d) {
  return a + b + c + d;
}

int sum5(int a, int b, int c, int d, int e) {
  return a + b + c + d + e;
}

int sum6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

int sum7(int a, int b, int c, int d, int e, int f, int g) {
  return a + b + c + d + e + f + g;
}


void test_dynamic() {

  int x, f;
  tie_variable lookup[] = {
      {"x",    &x},
      {"f",    &f},
      {"sum0", sum0, TIE_FUNCTION0},
      {"sum1", sum1, TIE_FUNCTION1},
      {"sum2", sum2, TIE_FUNCTION2},
      {"sum3", sum3, TIE_FUNCTION3},
      {"sum4", sum4, TIE_FUNCTION4},
      {"sum5", sum5, TIE_FUNCTION5},
      {"sum6", sum6, TIE_FUNCTION6},
      {"sum7", sum7, TIE_FUNCTION7},
  };

  test_case cases[] = {
      {"x",                   2},
      {"f+x",                 7},
      {"x+x",                 4},
      {"x+f",                 7},
      {"f+f",                 10},
      {"f+sum0",              11},
      {"sum0+sum0",           12},
      {"sum0()+sum0",         12},
      {"sum0+sum0()",         12},
      {"sum0()+(0)+sum0()",   12},
      {"sum1 sum0",           12},
      {"sum1(sum0)",          12},
      {"sum1 f",              10},
      {"sum1 x",              4},
      {"sum2 (sum0, x)",      8},
      {"sum3 (sum0, x, 2)",   10},
      {"sum2(2,3)",           5},
      {"sum3(2,3,4)",         9},
      {"sum4(2,3,4,5)",       14},
      {"sum5(2,3,4,5,6)",     20},
      {"sum6(2,3,4,5,6,7)",   27},
      {"sum7(2,3,4,5,6,7,8)", 35},
  };

  x = 2;
  f = 5;

  int i;
  for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
    const char *expr = cases[i].expr;
    const int answer = cases[i].answer;

    int err;
    tie_expression *ex = tie_compile(expr, lookup, sizeof(lookup) / sizeof(tie_variable), &err);
    lok(ex);
    lfequal(tie_eval(ex), answer);
    tie_free(ex);
  }
}


int clo0(void *context) {
  if (context) return *((int *) context) + 6;
  return 6;
}

int clo1(void *context, int a) {
  if (context) return *((int *) context) + a * 2;
  return a * 2;
}

int clo2(void *context, int a, int b) {
  if (context) return *((int *) context) + a + b;
  return a + b;
}

int cell(void *context, int a) {
  int *c = context;
  return c[(int) a];
}

void test_closure() {

  int extra;
  int c[] = {5, 6, 7, 8, 9};

  tie_variable lookup[] = {
      {"c0",   clo0, TIE_CLOSURE0, &extra},
      {"c1",   clo1, TIE_CLOSURE1, &extra},
      {"c2",   clo2, TIE_CLOSURE2, &extra},
      {"cell", cell, TIE_CLOSURE1, c},
  };

  test_case cases[] = {
      {"c0",          6},
      {"c1 4",        8},
      {"c2 (10, 20)", 30},
  };

  int i;
  for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
    const char *expr = cases[i].expr;
    const int answer = cases[i].answer;

    int err;
    tie_expression *ex = tie_compile(expr, lookup, sizeof(lookup) / sizeof(tie_variable), &err);
    lok(ex);

    extra = 0;
    lfequal(tie_eval(ex), answer + extra);

    extra = 10;
    lfequal(tie_eval(ex), answer + extra);

    tie_free(ex);
  }


  test_case cases2[] = {
      {"cell 0",                   5},
      {"cell 1",                   6},
      {"cell 0 + cell 1",          11},
      {"cell 1 * cell 3 + cell 4", 57},
  };

  for (i = 0; i < sizeof(cases2) / sizeof(test_case); ++i) {
    const char *expr = cases2[i].expr;
    const int answer = cases2[i].answer;

    int err;
    tie_expression *ex = tie_compile(expr, lookup, sizeof(lookup) / sizeof(tie_variable), &err);
    lok(ex);
    lfequal(tie_eval(ex), answer);
    tie_free(ex);
  }
}

void test_optimize() {

  test_case cases[] = {
      {"5+5",    10},
      {"22 * 2", 44},
  };

  int i;
  for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
    const char *expr = cases[i].expr;
    const int answer = cases[i].answer;

    int err;
    tie_expression *ex = tie_compile(expr, 0, 0, &err);
    lok(ex);

    /* The answer should be known without
     * even running eval. */
    lfequal(ex->value, answer);
    lfequal(tie_eval(ex), answer);

    tie_free(ex);
  }
}

int main(int argc, char *argv[]) {
  lrun("Results", test_results);
  lrun("Syntax", test_syntax);
  lrun("NaNs", test_nans);
  lrun("INFs", test_infs);
  lrun("Variables", test_variables);
  lrun("Functions", test_functions);
  lrun("Dynamic", test_dynamic);
  lrun("Closure", test_closure);
  lrun("Optimize", test_optimize);
  lresults();

  return lfails != 0;
}
