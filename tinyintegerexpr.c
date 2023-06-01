#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"

#include "tinyintegerexpr.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif


typedef int (*tie_fun2)(int, int);

enum {
  NULL_TOKEN = TIE_CLOSURE7 + 1, ERROR_TOKEN, END_TOKEN, SEPARATOR_TOKEN,
  OPEN_TOKEN, CLOSE_TOKEN, NUMBER_TOKEN, VARIABLE_TOKEN, INFIX_TOKEN
};


enum {
  TIE_CONSTANT = 1
};


typedef struct state {
  const char *start;
  const char *next;
  int type;
  union {
    int value;
    const int *bound;
    const void *function;
  };
  void *context;

  const tie_variable *lookup;
  int lookup_len;
} state;


#define TYPE_MASK(TYPE) ((TYPE)&0x0000001F)

#define IS_PURE(TYPE) (((TYPE) & TIE_FLAG_PURE) != 0)
#define IS_FUNCTION(TYPE) (((TYPE) & TIE_FUNCTION0) != 0)
#define IS_CLOSURE(TYPE) (((TYPE) & TIE_CLOSURE0) != 0)
#define ARITY(TYPE) ( ((TYPE) & (TIE_FUNCTION0 | TIE_CLOSURE0)) ? ((TYPE) & 0x00000007) : 0 )
#define NEW_EXPR(type, ...) new_expr((type), (const tie_expression*[]){__VA_ARGS__})
#define CHECK_NULL(ptr, ...) if ((ptr) == NULL) { __VA_ARGS__; return NULL; }

static tie_expression *new_expr(const int type, const tie_expression *parameters[]) {
  const int arity = ARITY(type);
  const int psize = sizeof(void *) * arity;
  const int size = (sizeof(tie_expression) - sizeof(void *)) + psize + (IS_CLOSURE(type) ? sizeof(void *) : 0);
  tie_expression *ret = malloc(size);
  CHECK_NULL(ret);

  memset(ret, 0, size);
  if (arity && parameters) {
    memcpy(ret->parameters, parameters, psize);
  }
  ret->type = type;
  ret->bound = 0;
  return ret;
}


#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArrayIndexOutOfBounds"

void tie_free_parameters(tie_expression *n) {
  if (!n) return;
  switch (TYPE_MASK(n->type)) {
    case TIE_FUNCTION7:
    case TIE_CLOSURE7:
      tie_free(n->parameters[6]);
    case TIE_FUNCTION6:
    case TIE_CLOSURE6:
      tie_free(n->parameters[5]);
    case TIE_FUNCTION5:
    case TIE_CLOSURE5:
      tie_free(n->parameters[4]);
    case TIE_FUNCTION4:
    case TIE_CLOSURE4:
      tie_free(n->parameters[3]);
    case TIE_FUNCTION3:
    case TIE_CLOSURE3:
      tie_free(n->parameters[2]);
    case TIE_FUNCTION2:
    case TIE_CLOSURE2:
      tie_free(n->parameters[1]);
    case TIE_FUNCTION1:
    case TIE_CLOSURE1:
      tie_free(n->parameters[0]);
  }
}

#pragma clang diagnostic pop


void tie_free(tie_expression *n) {
  if (!n) return;
  tie_free_parameters(n);
  free(n);
}

static int iffunc(int a, int b, int c) {
  return a ? b : c;
}

static const tie_variable functions[] = {
    /* must be in alphabetical order */
    {"abs",   fabs,   TIE_FUNCTION1 | TIE_FLAG_PURE, 0},
    {"floor", floor,  TIE_FUNCTION1 | TIE_FLAG_PURE, 0},
    {"if",    iffunc, TIE_FUNCTION3 | TIE_FLAG_PURE, 0},
    {0,       0,      0,                           0}
};

static const tie_variable *find_builtin(const char *name, int len) {
  int imin = 0;
  int imax = sizeof(functions) / sizeof(tie_variable) - 2;

  /*Binary search.*/
  while (imax >= imin) {
    const int i = (imin + ((imax - imin) / 2));
    int c = strncmp(name, functions[i].name, len);
    if (!c) c = '\0' - functions[i].name[len];
    if (c == 0) {
      return functions + i;
    } else if (c > 0) {
      imin = i + 1;
    } else {
      imax = i - 1;
    }
  }

  return 0;
}

static const tie_variable *find_lookup(const state *s, const char *name, int len) {
  int iters;
  const tie_variable *var;
  if (!s->lookup) return 0;

  for (var = s->lookup, iters = s->lookup_len; iters; ++var, --iters) {
    if (strncmp(name, var->name, len) == 0 && var->name[len] == '\0') {
      return var;
    }
  }
  return 0;
}


static int add(int a, int b) {
  return a + b;
}

static int sub(int a, int b) {
  return a - b;
}

static int mul(int a, int b) {
  return a * b;
}

static int divide(int a, int b) {
  return a / b;
}

static int negate(int a) {
  return -a;
}

static int compliment(int a) {
  return ~a;
}

static int comma(int a, int b) {
  (void) a;
  return b;
}

static int bitshift_right(int a, int b) {
  return a >> b;
}

static int bitshift_left(int a, int b) {
  return a << b;
}

static int bitwise_and(int a, int b) {
  return a & b;
}

static int bitwise_or(int a, int b) {
  return a | b;
}

static int bitwise_xor(int a, int b) {
  return a ^ b;
}


void next_token(state *s) {
  // Start off as a Null Token
  s->type = NULL_TOKEN;

  do {
    // If there is no next token then return an End Token
    if (!*s->next) {
      s->type = END_TOKEN;
      return;
    }

    // Is it a Number?
    if (isdigit(s->next[0]) || s->next[0] == '.') {
      s->value = strtod(s->next, (char **) &s->next);
      s->type = NUMBER_TOKEN;
    } else {
      // Is it a variable or a builtin function call?
      if (isalpha(s->next[0])) {
        const char *start;
        start = s->next;
        while (isalpha(s->next[0]) || isdigit(s->next[0]) || (s->next[0] == '_')) s->next++;

        const tie_variable *var = find_lookup(s, start, s->next - start);
        // If the variable couldn't be looked up, check to see if it's a builtin function
        if (!var) var = find_builtin(start, s->next - start);
        // If the variable _STILL_ doesn't exist, then it's an error.
        if (!var) {
          s->type = ERROR_TOKEN;
        } else {
          switch (TYPE_MASK(var->type)) {
            case TIE_VARIABLE:
              s->type = VARIABLE_TOKEN;
              s->bound = var->address;
              break;

            case TIE_CLOSURE0:
            case TIE_CLOSURE1:
            case TIE_CLOSURE2:
            case TIE_CLOSURE3:
            case TIE_CLOSURE4:
            case TIE_CLOSURE5:
            case TIE_CLOSURE6:
            case TIE_CLOSURE7:
              s->context = var->context;

            case TIE_FUNCTION0:
            case TIE_FUNCTION1:
            case TIE_FUNCTION2:
            case TIE_FUNCTION3:
            case TIE_FUNCTION4:
            case TIE_FUNCTION5:
            case TIE_FUNCTION6:
            case TIE_FUNCTION7:
              s->type = var->type;
              s->function = var->address;
              break;
          }
        }

      } else {
        // See if it's an operator or special character.
        switch (s->next++[0]) {
          case '&':
            s->type = INFIX_TOKEN;
            s->function = bitwise_and;
            break;
          case '|':
            s->type = INFIX_TOKEN;
            s->function = bitwise_or;
            break;
          case '^':
            s->type = INFIX_TOKEN;
            s->function = bitwise_xor;
            break;
          case '~':
            s->type = INFIX_TOKEN;
            s->function = compliment;
            break;
          case '>':
            s->type = INFIX_TOKEN;
            s->function = bitshift_right;
            break;
          case '<':
            s->type = INFIX_TOKEN;
            s->function = bitshift_left;
            break;
          case '+':
            s->type = INFIX_TOKEN;
            s->function = add;
            break;
          case '-':
            s->type = INFIX_TOKEN;
            s->function = sub;
            break;
          case '*':
            s->type = INFIX_TOKEN;
            s->function = mul;
            break;
          case '/':
            s->type = INFIX_TOKEN;
            s->function = divide;
            break;
          case '%':
            s->type = INFIX_TOKEN;
            s->function = fmod;
            break;
          case '(':
            s->type = OPEN_TOKEN;
            break;
          case ')':
            s->type = CLOSE_TOKEN;
            break;
          case ',':
            s->type = SEPARATOR_TOKEN;
            break;
          case ' ':
          case '\t':
          case '\n':
          case '\r':
            break;
          default:
            s->type = ERROR_TOKEN;
            break;
        }
      }
    }
  } while (s->type == NULL_TOKEN);
}


static tie_expression *list(state *s);

static tie_expression *expr(state *s);

static tie_expression *power(state *s);

static tie_expression *bitwise(state *s);

static tie_expression *shift(state *s);

static tie_expression *base(state *s) {
  /* <base>      =    <constant> | <variable> | <function-0> {"(" ")"} | <function-1> <power> | <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */
  tie_expression *ret;
  unsigned char arity;

  switch (TYPE_MASK(s->type)) {
    case NUMBER_TOKEN:
      ret = new_expr(TIE_CONSTANT, 0);
      CHECK_NULL(ret);

      ret->value = s->value;
      next_token(s);
      break;

    case VARIABLE_TOKEN:
      ret = new_expr(TIE_VARIABLE, 0);
      CHECK_NULL(ret);

      ret->bound = s->bound;
      next_token(s);
      break;

    case TIE_FUNCTION0:
    case TIE_CLOSURE0:
      ret = new_expr(s->type, 0);
      CHECK_NULL(ret);

      ret->function = s->function;
      if (IS_CLOSURE(s->type)) {
        ret->parameters[0] = s->context;
      }
      next_token(s);
      if (s->type == OPEN_TOKEN) {
        next_token(s);
        if (s->type != CLOSE_TOKEN) {
          s->type = ERROR_TOKEN;
        } else {
          next_token(s);
        }
      }
      break;

    case TIE_FUNCTION1:
    case TIE_CLOSURE1:
      ret = new_expr(s->type, 0);
      CHECK_NULL(ret);

      ret->function = s->function;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArrayIndexOutOfBounds"
      if (IS_CLOSURE(s->type)) {
        ret->parameters[1] = s->context;
      }
#pragma clang diagnostic pop
      next_token(s);
      ret->parameters[0] = power(s);
      CHECK_NULL(ret->parameters[0], tie_free(ret));
      break;

    case TIE_FUNCTION2:
    case TIE_FUNCTION3:
    case TIE_FUNCTION4:
    case TIE_FUNCTION5:
    case TIE_FUNCTION6:
    case TIE_FUNCTION7:
    case TIE_CLOSURE2:
    case TIE_CLOSURE3:
    case TIE_CLOSURE4:
    case TIE_CLOSURE5:
    case TIE_CLOSURE6:
    case TIE_CLOSURE7:
      arity = ARITY(s->type);

      ret = new_expr(s->type, 0);
      CHECK_NULL(ret);

      ret->function = s->function;
      if (IS_CLOSURE(s->type)) ret->parameters[arity] = s->context;
      next_token(s);

      if (s->type != OPEN_TOKEN) {
        s->type = ERROR_TOKEN;
      } else {
        unsigned char i;
        for (i = 0; i < arity; i++) {
          next_token(s);
          ret->parameters[i] = expr(s);
          CHECK_NULL(ret->parameters[i], tie_free(ret));

          if (s->type != SEPARATOR_TOKEN) {
            break;
          }
        }
        if (s->type != CLOSE_TOKEN || i != arity - 1) {
          s->type = ERROR_TOKEN;
        } else {
          next_token(s);
        }
      }

      break;

    case OPEN_TOKEN:
      next_token(s);
      ret = list(s);
      CHECK_NULL(ret);

      if (s->type != CLOSE_TOKEN) {
        s->type = ERROR_TOKEN;
      } else {
        next_token(s);
      }
      break;

    default:
      ret = new_expr(0, 0);
      CHECK_NULL(ret);

      s->type = ERROR_TOKEN;
      ret->value = NAN;
      break;
  }

  return ret;
}


static tie_expression *power(state *s) {
  /* <power> = {("-" | "+")} <base> */
  char sign = 1;
  while (s->type == INFIX_TOKEN && (s->function == add || s->function == sub)) {
    if (s->function == sub) sign = -sign;
    next_token(s);
  }

  tie_expression *ret;

  if (sign == 1) {
    ret = base(s);
  } else {
    tie_expression *b = base(s);
    CHECK_NULL(b);

    ret = NEW_EXPR(TIE_FUNCTION1 | TIE_FLAG_PURE, b);
    CHECK_NULL(ret, tie_free(b));

    ret->function = negate;
  }

  return ret;
}


static tie_expression *bitwise(state *s) {
  // <bitwise> = <power> {("&" | "|" | "^" ) <power>}
  tie_expression *ret = power(s);
  CHECK_NULL(ret);

  while (s->type == INFIX_TOKEN && (s->function == bitwise_and || s->function == bitwise_or || s->function == bitwise_xor)) {
    tie_fun2 t = s->function;
    next_token(s);
    tie_expression *f = power(s);
    CHECK_NULL(f, tie_free(ret));

    tie_expression *prev = ret;
    ret = NEW_EXPR(TIE_FUNCTION2 | TIE_FLAG_PURE, ret, f);
    CHECK_NULL(ret, tie_free(f), tie_free(prev));

    ret->function = t;
  }

  return ret;
}

static tie_expression *shift(state *s) {
  // <shift> = <bitwise> {("<" | ">") <bitwise>}
  tie_expression *ret = bitwise(s);
  CHECK_NULL(ret);

  while (s->type == INFIX_TOKEN && (s->function == bitshift_left || s->function == bitshift_right)) {
    tie_fun2 t = s->function;
    next_token(s);
    tie_expression *f = bitwise(s);
    CHECK_NULL(f, tie_free(ret));

    tie_expression *prev = ret;
    ret = NEW_EXPR(TIE_FUNCTION2 | TIE_FLAG_PURE, ret, f);
    CHECK_NULL(ret, tie_free(f), tie_free(prev));

    ret->function = t;
  }

  return ret;
}

static tie_expression *term(state *s) {
  // <term> = <shift> {("*" | "/" | "%") <shift>}
  tie_expression *ret = shift(s);
  CHECK_NULL(ret);

  while (s->type == INFIX_TOKEN && (s->function == mul || s->function == divide || s->function == fmod)) {
    tie_fun2 t = s->function;
    next_token(s);
    tie_expression *f = shift(s);
    CHECK_NULL(f, tie_free(ret));

    tie_expression *prev = ret;
    ret = NEW_EXPR(TIE_FUNCTION2 | TIE_FLAG_PURE, ret, f);
    CHECK_NULL(ret, tie_free(f), tie_free(prev));

    ret->function = t;
  }

  return ret;
}


static tie_expression *expr(state *s) {
  /* <expr> = <term> {("+" | "-") <term>} */
  tie_expression *ret = term(s);
  CHECK_NULL(ret);

  while (s->type == INFIX_TOKEN && (s->function == add || s->function == sub)) {
    tie_fun2 t = s->function;
    next_token(s);
    tie_expression *te = term(s);
    CHECK_NULL(te, tie_free(ret));

    tie_expression *prev = ret;
    ret = NEW_EXPR(TIE_FUNCTION2 | TIE_FLAG_PURE, ret, te);
    CHECK_NULL(ret, tie_free(te), tie_free(prev));

    ret->function = t;
  }

  return ret;
}


static tie_expression *list(state *s) {
  /* <list> = <expr> {"," <expr>} */
  tie_expression *ret = expr(s);
  CHECK_NULL(ret);

  while (s->type == SEPARATOR_TOKEN) {
    next_token(s);
    tie_expression *e = expr(s);
    CHECK_NULL(e, tie_free(ret));

    tie_expression *prev = ret;
    ret = NEW_EXPR(TIE_FUNCTION2 | TIE_FLAG_PURE, ret, e);
    CHECK_NULL(ret, tie_free(e), tie_free(prev));

    ret->function = comma;
  }

  return ret;
}


#define TIE_FUN(...) ((int(*)(__VA_ARGS__))n->function)
#define M(e) tie_eval(n->parameters[e])


int tie_eval(const tie_expression *n) {
  if (!n) return NAN;

  switch (TYPE_MASK(n->type)) {
    case TIE_CONSTANT:
      return n->value;
    case TIE_VARIABLE:
      return *n->bound;

    case TIE_FUNCTION0:
    case TIE_FUNCTION1:
    case TIE_FUNCTION2:
    case TIE_FUNCTION3:
    case TIE_FUNCTION4:
    case TIE_FUNCTION5:
    case TIE_FUNCTION6:
    case TIE_FUNCTION7:
      switch (ARITY(n->type)) {
        case 0:
          return TIE_FUN(void)();
        case 1:
          return TIE_FUN(int)(M(0));
        case 2:
          return TIE_FUN(int, int)(M(0), M(1));
        case 3:
          return TIE_FUN(int, int, int)(M(0), M(1), M(2));
        case 4:
          return TIE_FUN(int, int, int, int)(M(0), M(1), M(2), M(3));
        case 5:
          return TIE_FUN(int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4));
        case 6:
          return TIE_FUN(int, int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4), M(5));
        case 7:
          return TIE_FUN(int, int, int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4), M(5), M(6));
        default:
          return NAN;
      }

    case TIE_CLOSURE0:
    case TIE_CLOSURE1:
    case TIE_CLOSURE2:
    case TIE_CLOSURE3:
    case TIE_CLOSURE4:
    case TIE_CLOSURE5:
    case TIE_CLOSURE6:
    case TIE_CLOSURE7:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArrayIndexOutOfBounds"
      switch (ARITY(n->type)) {
        case 0:
          return TIE_FUN(void*)(n->parameters[0]);
        case 1:
          return TIE_FUN(void*, int)(n->parameters[1], M(0));
        case 2:
          return TIE_FUN(void*, int, int)(n->parameters[2], M(0), M(1));
        case 3:
          return TIE_FUN(void*, int, int, int)(n->parameters[3], M(0), M(1), M(2));
        case 4:
          return TIE_FUN(void*, int, int, int, int)(n->parameters[4], M(0), M(1), M(2), M(3));
        case 5:
          return TIE_FUN(void*, int, int, int, int, int)(n->parameters[5], M(0), M(1), M(2), M(3), M(4));
        case 6:
          return TIE_FUN(void*, int, int, int, int, int, int)(n->parameters[6], M(0), M(1), M(2), M(3), M(4), M(5));
        case 7:
          return TIE_FUN(void*, int, int, int, int, int, int, int)(n->parameters[7], M(0), M(1), M(2), M(3), M(4), M(5), M(6));
#pragma clang diagnostic pop
        default:
          return NAN;
      }

    default:
      return NAN;
  }

}

#undef TIE_FUN
#undef M

static void optimize(tie_expression *n) {
  /* Evaluates as much as possible. */
  if (n->type == TIE_CONSTANT) return;
  if (n->type == TIE_VARIABLE) return;

  /* Only optimize out functions flagged as pure. */
  if (IS_PURE(n->type)) {
    const int arity = ARITY(n->type);
    int known = 1;
    int i;
    for (i = 0; i < arity; ++i) {
      optimize(n->parameters[i]);
      if (((tie_expression *) (n->parameters[i]))->type != TIE_CONSTANT) {
        known = 0;
      }
    }
    if (known) {
      const int value = tie_eval(n);
      tie_free_parameters(n);
      n->type = TIE_CONSTANT;
      n->value = value;
    }
  }
}


tie_expression *tie_compile(const char *expression, const tie_variable *variables, int var_count, int *error) {
  state s;
  s.start = s.next = expression;
  s.lookup = variables;
  s.lookup_len = var_count;

  next_token(&s);
  tie_expression *root = list(&s);
  if (root == NULL) {
    if (error) *error = -1;
    return NULL;
  }

  if (s.type != END_TOKEN) {
    tie_free(root);
    if (error) {
      *error = (s.next - s.start);
      if (*error == 0) *error = 1;
    }
    return 0;
  } else {
    optimize(root);
    if (error) *error = 0;
    return root;
  }
}

int tie_interp(const char *expression, int *error) {
  tie_expression *n = tie_compile(expression, 0, 0, error);
  if (n == NULL) {
    return NAN;
  }

  int ret;
  if (n) {
    ret = tie_eval(n);
    tie_free(n);
  } else {
    ret = NAN;
  }
  return ret;
}

static void pn(const tie_expression *n, int depth) {
  int i, arity;
  printf("%*s", depth, "");

  switch (TYPE_MASK(n->type)) {
    case TIE_CONSTANT:
      printf("%d\n", n->value);
      break;
    case TIE_VARIABLE:
      printf("bound %p\n", n->bound);
      break;

    case TIE_FUNCTION0:
    case TIE_FUNCTION1:
    case TIE_FUNCTION2:
    case TIE_FUNCTION3:
    case TIE_FUNCTION4:
    case TIE_FUNCTION5:
    case TIE_FUNCTION6:
    case TIE_FUNCTION7:
    case TIE_CLOSURE0:
    case TIE_CLOSURE1:
    case TIE_CLOSURE2:
    case TIE_CLOSURE3:
    case TIE_CLOSURE4:
    case TIE_CLOSURE5:
    case TIE_CLOSURE6:
    case TIE_CLOSURE7:
      arity = ARITY(n->type);
      printf("f%d", arity);
      for (i = 0; i < arity; i++) {
        printf(" %p", n->parameters[i]);
      }
      printf("\n");
      for (i = 0; i < arity; i++) {
        pn(n->parameters[i], depth + 1);
      }
      break;
  }
}


void tie_print(const tie_expression *n) {
  pn(n, 0);
}

#pragma clang diagnostic pop