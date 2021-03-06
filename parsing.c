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

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

#define LASSERT_ARGS(lval, expected, name) \
  LASSERT(lval, lval->count == expected, "Function '%s' passed too many " \
    "arguments. Got %i, Expected %i.", \
    name, lval->count, expected);

#define LASSERT_TYPE(lval, check, expected, name) \
  LASSERT(lval, check == expected, "Function '%s' passed wrong type " \
    "Got %s, Expected %s.", \
    name, ltype_name(check), ltype_name(expected));

#define LASSERT_ELIST(lval, name) \
  LASSERT(lval, lval->cell[0]->count != 0, "Function '%s' passed empty list.", \
    name);

struct lval;

typedef struct lval lval;

struct lenv {
  struct lenv* par;
  int count;
  char** syms;
  lval** vals;
};

typedef struct lenv lenv;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

char* ltype_name(int t) {
  switch (t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

// a function type called lbuiltin that takes an lenv and an lval 
// and returns a pointer to an lval
typedef lval* (*lbuiltin)(lenv*, lval*);

struct lval {
  int type;

  long num;
  char* err;
  char* sym;

  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  int count;
  lval** cell;
};

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);

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

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;

  return v;
}

lenv* lenv_new();
void lenv_put(lenv* e, lval* k, lval* v);

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));

  v->type = LVAL_FUN;
  v->builtin = NULL;
  v->env = lenv_new();
  v->formals = formals;
  v->body = body;

  return v;
}

void lval_print(lenv* e, lval* v);

void lval_expr_print(lenv* e, lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(e, v->cell[i]);

    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_fun_print(lenv* e, lval* v) {
  if (v->builtin) {
    for (int i = 0; i < e->count; i++) {
      if (e->vals[i]->builtin == v->builtin) {
        printf("<builtin> %s", e->syms[i]);
      }
    }
  } else {
    printf("(\\ "); lval_print(e, v->formals);
    putchar(' '); lval_print(e, v->body); putchar(')');
  }
}

void lval_print(lenv* e, lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(e, v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(e, v, '{', '}'); break;
    case LVAL_FUN: lval_fun_print(e, v); break;
  }
}

void lval_println(lenv* e, lval* v) { lval_print(e, v); putchar('\n'); }

void lval_print_env(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    printf("%s = ", e->syms[i]);
    lval_print(e, e->vals[i]);
    printf("\n");
  }
}

void lenv_del(lenv* e);

lenv* lenv_copy(lenv* e);

void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM: break;

    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
    break;
    case LVAL_FUN:
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
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

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    case LVAL_NUM: x->num = v->num; break;
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;
    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);

      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;
  }

  return x;
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
//      printf("%i\n", a->count);
    }
  }

  lval_del(a);

  return lval_num(count);
}

lval* builtin_len(lenv* e, lval* a) {
  LASSERT_ARGS(a, 1, "len");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "len");
  LASSERT_ELIST(a, "len");

  lval* len = lval_len(lval_pop(a, 0));

  lval_del(a);

  return len;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
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

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
  return builtin_op(e, a, "%");
}

lval* builtin_pow(lenv* e, lval* a) {
  return builtin_op(e, a, "^");
}

lval* builtin_head(lenv* e, lval* a) {
  LASSERT_ARGS(a, 1, "head");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "head");
  LASSERT_ELIST(a, "head");

  lval* v = lval_take(a, 0);

  // why do this? its like head of head?
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_ARGS(a, 1, "tail");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "tail");
  LASSERT_ELIST(a, "tail");

  lval* v = lval_take(a, 0);

  lval_del(lval_pop(v, 0));

  return v;
}

lval* builtin_list(lenv* e, lval* a) {
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

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(a, a->cell[i]->type, LVAL_QEXPR, "join");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);

  return x;
}

lval* builtin_cons(lenv* e, lval* a) {
  LASSERT_ARGS(a, 2, "cons");

  // TODO hmm only works with number? that can't be right
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_NUM, "cons");
  LASSERT_TYPE(a, a->cell[1]->type, LVAL_QEXPR, "cons");

  lval* x = lval_pop(a, 0);
  lval* y = lval_pop(a, 0);

  lval_del(a);

  lval* list = lval_qexpr();
  list = lval_add(list, x);
  list = lval_join(list, y);

  return list;
}

lval* builtin_init(lenv* e, lval* a) {
  LASSERT_ARGS(a, 1, "init");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "init");
  LASSERT_ELIST(a, "init");

  lval* b = lval_pop(a, 0);
  lval* c = lval_qexpr();

  while (b->count > 1) {
    c = lval_add(c, lval_pop(b, 0));
  }

  lval_del(a);
  lval_del(b);

  return c;
}

lval* lval_eval(lenv* e, lval* v);

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_ARGS(a, 1, "eval");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "eval");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(e, x);
}

lval* builtin_def(lenv* e, lval* a);

lval* builtin_printEnv(lenv* e, lval* a) {
  lval_print_env(e);

  lval_del(a);

  return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_ARGS(a, 2, "\\");
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, "\\");
  LASSERT_TYPE(a, a->cell[1]->type, LVAL_QEXPR, "\\");

  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, a->cell[0]->cell[i]->type == LVAL_SYM,
      "Cannot define non-symbol. Got %s. Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
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

lval* lval_call(lenv* e, lval* func, lval* args) {
  if (func->builtin) {
    return func->builtin(e, args);
  }

  int given = args->count;
  int total = func->formals->count;

  while (args->count) {
    if (func->formals->count == 0) {
      lval_del(args);
      return lval_err("Function passed too many arguments. "
      "Got %i. Expected %i.", given, total);
    }

    lval* sym = lval_pop(func->formals, 0);
    lval* val = lval_pop(args, 0);

    lenv_put(func->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  lval_del(args);

  if (func->formals->count == 0) {
    func->env->par = e;

    return builtin_eval(func->env,
      lval_add(lval_sexpr(), lval_copy(func->body)));
  } else {
    return lval_copy(func);
  }
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count == 0) { return v; }
  if (v->count == 1) { return lval_take(v, 0); }

  lval* f = lval_pop(v, 0);

  if (f->type != LVAL_FUN) {
    lval* err = lval_err("S-Expression starts with incorrect type. "
      "got %s, expected %s.",
      ltype_name(f->type), ltype_name(LVAL_FUN));

    lval_del(f);
    lval_del(v);

    return err;
  }

  lval* result = lval_call(e, f, v);
  lval_del(f);

  return result;
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;

  return e;
};

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("Unbound Symbol '%s'", k->sym);
  }
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);

      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->par) {
    e = e->par;
  }

  lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));

  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }

  return n;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);

  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_TYPE(a, a->cell[0]->type, LVAL_QEXPR, func);

  lval* syms = a->cell[0];

  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
      "Funcion %s cannot define non-symbol",
      ltype_name(syms->cell[i]->type));
  }

  // value indicies are all symbol indicies + 1 b/c first index of values is 
  // symbols qexpr
  LASSERT(a, syms->count == a->count - 1,
    "Function def cannot define inequal number of values to symbols. "
    "Got %i symbols and %i values.",
    syms->count, a->count - 1);

  for (int i = 0; i < syms->count; i++) {

    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    }

    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }
  }

  lval_del(a);

  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

// ADD COMMENTS AND SHIT. THIS IS CONFUSING
void lenv_add_builtins(lenv* e) {
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "%", builtin_mod);
  lenv_add_builtin(e, "^", builtin_pow);

  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "init", builtin_init);
  lenv_add_builtin(e, "len", builtin_len);

  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "\\", builtin_lambda);
  // should probably do this a different way cause there is no need
  // for second variable... idk
  lenv_add_builtin(e, "printEnv", builtin_printEnv);
}

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);

    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }

  return v;
}

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
      symbol    : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&\\^%]+/ ;                      \
      sexpr     : '(' <expr>* ')' ;                                           \
      qexpr     : '{' <expr>* '}' ;                                           \
      expr      : <number> | <symbol> | <sexpr> | <qexpr> ;                   \
      lispy     : /^/ <expr>* /$/ ;                                           \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  while(1) {
    char* input = readline("lispy> ");

    add_history(input);
//    printf("line\n");
//    printf("input is %s \n", input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      // success?
      lval* result = lval_eval(e, lval_read(r.output));
      lval_println(e, result);
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

  lenv_del(e);
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
