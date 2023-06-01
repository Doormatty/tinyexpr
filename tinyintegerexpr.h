#ifndef TINYINTEGEREXPR_H
#define TINYINTEGEREXPR_H


#ifdef __cplusplus
extern "C" {
#endif


typedef struct tie_expression {
  int type;
  union {
    int value;
    const int *bound;
    const void *function;
  };
  void *parameters[1];
} tie_expression;


enum {
  TIE_VARIABLE = 0,

  TIE_FUNCTION0 = 8, TIE_FUNCTION1, TIE_FUNCTION2, TIE_FUNCTION3,
  TIE_FUNCTION4, TIE_FUNCTION5, TIE_FUNCTION6, TIE_FUNCTION7,

  TIE_CLOSURE0 = 16, TIE_CLOSURE1, TIE_CLOSURE2, TIE_CLOSURE3,
  TIE_CLOSURE4, TIE_CLOSURE5, TIE_CLOSURE6, TIE_CLOSURE7,

  TIE_FLAG_PURE = 32
};

typedef struct tie_variable {
  const char *name;
  const void *address;
  int type;
  void *context;
} tie_variable;



/* Parses the input expression, evaluates it, and frees it. */
/* Returns NaN on error. */
int tie_interp(const char *expression, int *error);

/* Parses the input expression and binds variables. */
/* Returns NULL on error. */
tie_expression *tie_compile(const char *expression, const tie_variable *variables, int var_count, int *error);

/* Evaluates the expression. */
int tie_eval(const tie_expression *n);

/* Prints debugging information on the syntax tree. */
void tie_print(const tie_expression *n);

/* Frees the expression. (safe to call on NULL pointers) */
void tie_free(tie_expression *n);


#ifdef __cplusplus
}
#endif

#endif