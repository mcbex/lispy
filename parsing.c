#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mpc.h"


// windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cp = malloc(strlen(buffer) + 1);

  strcpy(cp, buffer);
  cp[strlen(cp) - 1] = '\0';

  return cp;
}

void add_history(char* unused) {}

#else

#include <editline/readline.h>

#endif

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

// enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  
  strcpy(v->err, m);

  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);

  strcpy(v->sym, s);

  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM: break;

    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i =0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
    break;
  }

  free(v);
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;

  return v;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  // move the contents of location of arg2 to location of arg1... move arg3 amount of contents
  // so if there are 5 lvals and i == 1, take the lval at 1 and save it in x
  // then starting at the address for the lval at 2, 
  // move all remainng lvals (length is total count - index - 1 so 5 - 1 - 1 == 3 after the pop) 
  // to the address for the lval at 1
  memmove(&(v->cell[i]), &(v->cell[i + 1]), sizeof(lval*) * (v->count - i - 1));

  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);

  return x;
}

lval* lval_take(lval* v, int i) {
  // hmm why not just take the item at i then delete v?
  lval* x = lval_pop(v, i);
  lval_del(v);

  return x;
}

lval* lval_len(lval* a) {
  int count = 0;

  if (a->type != LVAL_SEXPR && a->type != LVAL_QEXPR) {
    // only s and q expr have children
    count += 1;
  } else {
    int c = a->count;
    for (int i = 0; i < c; i++) {
      lval* b = lval_len(lval_pop(a, 0));
      count += b->num;
      lval_del(b);
      printf("%i\n", a->count);
    }
  }

  lval_del(a);

  return lval_num(count);
}

lval* builtin_len(lval* a) {
  LASSERT(a, a->count == 1, "Function 'len' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'len' passed incorrect types");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");

  lval* len = lval_len(lval_pop(a, 0));

  lval_del(a);

  return len;
}

lval* builtin_op(lval* a, char* op) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);

      return lval_err("Cannot operate on non-number");
    }
  }

  lval* x = lval_pop(a, 0);

  if (strcmp(op, "-") == 0 && a->count == 0) {
    x->num = -(x->num);
  }

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) { 

      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero");

        break;
      }

      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0) { x->num %= y->num; }
    if (strcmp(op, "^") == 0) { x->num = pow(x->num, y->num); }

    lval_del(y);
  }

  lval_del(a);

  return x;
}

lval* builtin_head(lval* a) {
  LASSERT(a, a->count == 1, "Function 'head' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect types");
  LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");

  lval* v = lval_take(a, 0);

  // why do this? its like head of head?
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}

lval* builtin_tail(lval* a) {
  LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect types");
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");

  lval* v = lval_take(a, 0);

  lval_del(lval_pop(v, 0));

  return v;
}

lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;

  return a;
}

lval* lval_join(lval* x, lval* y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);

  return x;
}

lval* builtin_join(lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type.");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);

  return x;
}

lval* builtin_cons(lval* a) {
  LASSERT(a, a->count == 2, "Function 'cons' passed incorrect number of arguments");

  lval* x = lval_take(a, 0);
  lval* y = lval_take(a, 1);

  LASSERT(x, x->type == LVAL_NUM, "Function 'cons' passed incorrect type");
  LASSERT(y, y->type == LVAL_QEXPR, "Function 'cons' passed incorrect type");

  lval* list = lval_qexpr();
  list = lval_add(list, x);
  list = lval_join(list, y);

  lval_del(x);
  lval_del(y);

  return list;
}

lval* lval_eval(lval* v);

lval* builtin_eval(lval* a) {
  LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect types");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(x);
}

lval* builtin(lval* a, char* func) {
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("cons", func) == 0) { return builtin_cons(a); }
  if (strcmp("len", func) == 0) { return builtin_len(a); }
  if (strstr("+-/*%^", func)) { return builtin_op(a, func); }

  lval_del(a);

  return lval_err("Unknown function");
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
//  printf("Tag: %s\n", t->tag);
//  printf("Contents: %s\n", t->contents);
//  printf("Number of children: %d\n", t->children_num);

  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* v = NULL;

  if (strcmp(t->tag, ">") == 0) { 
    v = lval_sexpr();
  } else if (strstr(t->tag, "sexpr")) {
    v = lval_sexpr();
  } else if (strstr(t->tag, "qexpr")) {
    v = lval_qexpr(); 
  } else {
    v = lval_err("idfk");
  }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }

    v = lval_add(v, lval_read(t->children[i]));
  }

  return v;
}

lval* lval_eval_sexpr(lval* v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count == 0) { return v; }
  if (v->count == 1) { return lval_take(v, 0); }

  lval* f = lval_pop(v, 0);

  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);

    return lval_err("S-expression does not start with symbol");
  }

  lval* result = builtin(v, f->sym);
  lval_del(f);

  return result;
}

lval* lval_eval(lval* v) {
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(v);
  }

  return v;
}
/*
lval* eval_op(lval* x, char* op, lval* y) {
  if (x->type == LVAL_ERR) { return x; }
  if (y->type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) { return lval_num(x->num + y->num); }
  if (strcmp(op, "-") == 0) { return lval_num(x->num - y->num); }
  if (strcmp(op, "*") == 0) { return lval_num(x->num * y->num); }
  if (strcmp(op, "/") == 0) { 

    return y->num == 0 
      ? lval_err("Division by zero")
      : lval_num(x->num / y->num);
  }
  if (strcmp(op, "%") == 0) { return lval_num(x->num % y->num); }
  if (strcmp(op, "^") == 0) { return lval_num(pow(x->num, y->num)); }
  if (strcmp(op, "min") == 0) { return lval_num(x->num < y->num ? x->num : y->num); }
  if (strcmp(op, "max") == 0) { return lval_num(x->num > y->num ? x->num : y->num); }

  return lval_err("Invalid operation");
}
*/

int main(int argc, char** argv) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                         \
      number    : /-?[0-9]+/ ;                                                \
      symbol    : \"list\" | \"head\" | \"tail\" | \"join\"                   \
                | \"eval\" | \"cons\" | \"len\"                               \
                | '+' | '-' | '*' | '/' | '%' | '^' ;                         \
      sexpr     : '(' <expr>* ')' ;                                           \
      qexpr     : '{' <expr>* '}' ;                                           \
      expr      : <number> | <symbol> | <sexpr> | <qexpr> ;                   \
      lispy     : /^/ <expr>* /$/ ;                                           \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while(1) {
    char* input = readline("lispy> ");

    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      // success?

      lval* result = lval_eval(lval_read(r.output));
      lval_println(result);
      lval_del(result);

//      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      // fail
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
