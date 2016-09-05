/* future reference: http://goo.gl/zrhKT1 
 *  a quick-dirty-panic duplicate :-(
 * */

#include <stdio.h>
#include <unistd.h>     /* getcwd() chdir() */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* case is sensitive in R6 and R7 */

#ifndef bool
    typedef enum {false, true} bool;
    #define bool bool
#endif

typedef unsigned int uint;

#define SCHEME_VERSION "v2.0"

/* THE_EMPTY_LIST for the empty list type 
 *  TODO: an exception type for diagnosis
 * */
typedef enum {
    THE_EMPTY_LIST, BOOLEAN, FIXNUM, CHARACTER, 
    STRING, PAIR, SYMBOL, PRIMITIVE_PROC, 
    COMPOUND_PROC, INPUT_PORT, OUTPUT_PORT, EOF_OBJECT
} object_t;

typedef struct object {
    object_t type;
    union {
        struct {
            bool value;
        } boolean;
        struct {
            long value;
        } fixnum;
        struct {
            char value;
        } character;
        struct {
            char *value;
        } string;
        struct {
            struct object *car;
            struct object *cdr;
        } pair;
        struct {
            char *value;
        } symbol;
        struct {
            struct object *(*fn)(struct object *arguments);
        } primitive_proc;
        struct {
            struct object *paras;
            struct object *body;
            struct object *env;
        } compound_proc;
        struct {
            FILE *stream;
        } input_port;
        struct {
            FILE *stream;
        } output_port;
    } data;
} object;

/* for boolean singleton */
static object *false_object;
static object *true_object;

static object *the_empty_list;

static object *symbol_table;

static object *quote_symbol;

/* using for define and set! also global environment */

static object *define_symbol;
static object *set_symbol;
static object *ok_symbol;

static object *the_empty_environment;
static object *the_global_environment;

static object *if_symbol;

static object *lambda_symbol;

static object *begin_symbol;

static object *cond_symbol;
static object *else_symbol;

static object *let_symbol;

static object *and_symbol;
static object *or_symbol;

/* TODO: #in, #out, #err as for stdin, stdout, stderr 
 *  non-standard but convient under some circumstances 
 *  also #eof as a shorthand of eof_object
 *  */
static object *eof_object;

/* map between the unescaped and escaped characters */
static const char *unescaped = "abfnrtv\\'\"?";
static const char   *escaped = "\a\b\f\n\r\t\v\\\'\"\?";

#define is_empty_list(obj) (obj == the_empty_list)

#define is_boolean(obj)    (obj->type == BOOLEAN)
/* a better(for a generous purpose) i12n of is_false() can be:
 *  (is_boolean(obj) && obj->data.boolean.value == false)
 * which the following code just judge if object equals to 
 *  the globally false object  e.g. #f
 * */
#define is_false(obj)      (obj == false_object)
/* according to standard  anything except for #f
 *  can be considered as #t  so you cannot define
 *  is_true(obj) as (obj == true_object)
 * */
#define is_true(obj)           (!is_false(obj))

#define is_fixnum(obj)         (obj->type == FIXNUM)
#define is_character(obj)      (obj->type == CHARACTER)
#define is_string(obj)         (obj->type == STRING)
#define is_pair(obj)           (obj->type == PAIR)
#define is_symbol(obj)         (obj->type == SYMBOL)
#define is_primitive_proc(obj) (obj->type == PRIMITIVE_PROC)
#define is_compound_proc(obj)  (obj->type == COMPOUND_PROC)
#define is_input_port(obj)     (obj->type == INPUT_PORT)
#define is_output_port(obj)    (obj->type == OUTPUT_PORT)
#define is_eof_object(obj)     (obj == eof_object)

/* code for object-generation  no memory reclaim! */

/* TODO: object *make_object(object_t type) */
object *make_object(void)
{
    object *obj = (object *) malloc(sizeof(object));
    if (obj != NULL) return obj;
    fprintf(stderr, "ERROR: Memory exhausted\n");
    exit(1);
}

object *make_fixnum(long num)
{
    object *obj = make_object();
    obj->type   = FIXNUM;
    obj->data.fixnum.value = num;
    return obj;
}

object *make_character(char value)
{
    object *obj = make_object();
    obj->type   = CHARACTER;
    obj->data.character.value = value;
    return obj;
}

object *make_string(const char *value)
{
    object *obj = make_object();
    obj->type   = STRING;
    char **p = &obj->data.string.value;
    *p = (char *) malloc(sizeof(char) + strlen(value));
    if (*p == NULL) {
        fprintf(stderr, "ERROR: Memory exhausted\n");
        exit(1);
    }
    strcpy(*p, value);
    return obj;
}

object *make_cons(object *car, object *cdr)
{
    object *obj = make_object();
    obj->type   = PAIR;
    obj->data.pair.car = car;
    obj->data.pair.cdr = cdr;
    return obj;
}

/* code for fetch and alter cons */

object *car(object *pair)
{ return pair->data.pair.car; }

object *cdr(object *pair)
{ return pair->data.pair.cdr; }

void set_car(object *obj, object *value)
{ obj->data.pair.car = value; }

void set_cdr(object *obj, object *value)
{ obj->data.pair.cdr = value; }

#define caar(obj)   car(car(obj))
#define cadr(obj)   car(cdr(obj))
#define cdar(obj)   cdr(car(obj))
#define cddr(obj)   cdr(cdr(obj))
#define caaar(obj)  car(car(car(obj)))
#define caadr(obj)  car(car(cdr(obj)))
#define cadar(obj)  car(cdr(car(obj)))
#define caddr(obj)  car(cdr(cdr(obj)))
#define cdaar(obj)  cdr(car(car(obj)))
#define cdadr(obj)  cdr(car(cdr(obj)))
#define cddar(obj)  cdr(cdr(car(obj)))
#define cdddr(obj)  cdr(cdr(cdr(obj)))
#define caaaar(obj) car(car(car(car(obj))))
#define caaadr(obj) car(car(car(cdr(obj))))
#define caadar(obj) car(car(cdr(car(obj))))
#define caaddr(obj) car(car(cdr(cdr(obj))))
#define cadaar(obj) car(cdr(car(car(obj))))
#define cadadr(obj) car(cdr(car(cdr(obj))))
#define caddar(obj) car(cdr(cdr(car(obj))))
#define cadddr(obj) car(cdr(cdr(cdr(obj))))
#define cdaaar(obj) cdr(car(car(car(obj))))
#define cdaadr(obj) cdr(car(car(cdr(obj))))
#define cdadar(obj) cdr(car(cdr(car(obj))))
#define cdaddr(obj) cdr(car(cdr(cdr(obj))))
#define cddaar(obj) cdr(cdr(car(car(obj))))
#define cddadr(obj) cdr(cdr(car(cdr(obj))))
#define cdddar(obj) cdr(cdr(cdr(car(obj))))
#define cddddr(obj) cdr(cdr(cdr(cdr(obj))))

object *make_symbol(const char *value)
{
    object *iter = symbol_table;
    /* using hash table is a good idea */
    while (!is_empty_list(iter)) {
        if (!strcmp(car(iter)->data.symbol.value, value))
            return car(iter);
        iter = cdr(iter);
    }

    object *obj = make_object();
    obj->type   = SYMBOL;
    obj->data.symbol.value = (char *) malloc(sizeof(char) + strlen(value));
    if (obj->data.symbol.value == NULL) {
        fprintf(stderr, "ERROR: Memory exhausted\n");
        exit(1);
    }
    strcpy(obj->data.symbol.value, value);
    symbol_table = make_cons(obj, symbol_table);
    return obj;
}

/* this func. take a func. as its argument
 *  which it accept and return a object *
 * */
object *make_primitive_proc(object *(*fn)(object *))
{
    object *obj = make_object();
    obj->type   = PRIMITIVE_PROC;
    obj->data.primitive_proc.fn = fn;
    return obj;
}

object *make_compound_proc(object *paras, object *body, object *env)
{
    object *obj = make_object();
    obj->type   = COMPOUND_PROC;
    obj->data.compound_proc.paras = paras;
    obj->data.compound_proc.body  = body;
    obj->data.compound_proc.env   = env;
    return obj;
}

object *make_input_port(FILE *stream)
{
    object *obj = make_object();
    obj->type   = INPUT_PORT;
    obj->data.input_port.stream = stream;
    return obj;
}

object *make_output_port(FILE *stream)
{
    object *obj = make_object();
    obj->type   = OUTPUT_PORT;
    obj->data.output_port.stream = stream;
    return obj;
}

/* code for primitive procedures */

/* SOME POSSIBLE PROCEDURES:
 * named-let letrec do sort map for-each
 * filtering folding apply member-if
 *   <= >= abs exp expt not read eval write
 *   exact inexact modulo sqrt sin cos tan
 *   asin acos atan log cd load eqv? equal?
 *   complex? real? rational? exact? inexact?
 *   odd? even? positive? negative? zero?
 *   char=? char<? char>? char<=? char>=?
 * */

/* type predicates */
object *is_null_proc(object *arg)
{ return is_empty_list(car(arg)) ? true_object : false_object; }

object *is_boolean_proc(object *arg)
{ return is_boolean(car(arg)) ? true_object : false_object; }

object *is_symbol_proc(object *arg)
{ return is_symbol(car(arg)) ? true_object : false_object; }

object *is_integer_proc(object *arg)
{ return is_fixnum(car(arg)) ? true_object : false_object; }

object *is_char_proc(object *arg)
{ return is_character(car(arg)) ? true_object : false_object; }

object *is_string_proc(object *arg)
{ return is_string(car(arg)) ? true_object : false_object; }

object *is_pair_proc(object *arg)
{ return is_pair(car(arg)) ? true_object : false_object; }

object *is_procedure_proc(object *arg)
{
    object *obj = car(arg);
    return is_primitive_proc(obj) || 
           is_compound_proc(obj)   ? 
         true_object : false_object;
}

/* type conversions */
object *char_to_integer_proc(object *arg)
{ return make_fixnum(car(arg)->data.character.value); }

object *integer_to_char_proc(object *arg)
{ return make_character(car(arg)->data.fixnum.value); }

object *number_to_string_proc(object *arg)
{
    char buffer[64];
    sprintf(buffer, "%ld", car(arg)->data.fixnum.value);
    return make_string(buffer);
}

object *string_to_number_proc(object *arg)  /* dirty i12n */
{ return make_fixnum(atoi(car(arg)->data.string.value)); }

object *symbol_to_string_proc(object *arg)
{ return make_string(car(arg)->data.symbol.value); }

object *string_to_symbol_proc(object *arg)
{ return make_symbol(car(arg)->data.string.value); }

/* #<procedure +> */
object *add_proc(object *args)
{   /* (+) got 0 */
    long result = 0;
    while (!is_empty_list(args)) {
        result += car(args)->data.fixnum.value;
        args = cdr(args);
    }
    return make_fixnum(result);
}

/* #<procedure -> */
object *sub_proc(object *args)
{
    long result = car(args)->data.fixnum.value;
    if (is_empty_list(cdr(args)))
        return make_fixnum(-result);
    while (!is_empty_list((args = cdr(args))))
        result -= car(args)->data.fixnum.value;
    return make_fixnum(result);
}

object *mul_proc(object *args)
{   /* (*) got 1 */
    long result = 1, factor;
    while (!is_empty_list(args)) {
        factor = car(args)->data.fixnum.value;
        /* even if one of its arguments is zero
         *  it still need a type checking for the rest of arguments
         *  which i simply omitted it for simplicity
         * */
        if (factor == 0) return make_fixnum(0);
        result *= factor;
        args = cdr(args);
    }
    return make_fixnum(result);
}

/* TODO: div_proc e.g. / which slightly differ from usual i12n */

/* no divided by zero checking */
object *quotient_proc(object *args)
{
    return make_fixnum(
         car(args)->data.fixnum.value / 
        cadr(args)->data.fixnum.value
    );
}

object *remainder_proc(object *args)
{
    return make_fixnum(
         car(args)->data.fixnum.value %
        cadr(args)->data.fixnum.value
    );
}

/* #<procedure => */
object *is_number_equal_proc(object *args)
{
    long value = car(args)->data.fixnum.value;
    while (!is_empty_list((args = cdr(args))))
        if (value != car(args)->data.fixnum.value)
            return false_object;
    return true_object;
}

/* #<procedure <> */
object *is_less_than_proc(object *args)
{
    long prev, next;
    prev = car(args)->data.fixnum.value;
    while (!is_empty_list((args = cdr(args)))) {
        next = car(args)->data.fixnum.value;
        if (prev >= next)
            return false_object;
        prev = next;
    }
    return true_object;
}

/* #<procedure >> */
object *is_greater_than_proc(object *args)
{
    long prev, next;
    prev = car(args)->data.fixnum.value;
    while (!is_empty_list((args = cdr(args)))) {
        next = car(args)->data.fixnum.value;
        if (prev <= next)
            return false_object;
        prev = next;
    }
    return true_object;
}

/* note here you cannot simply make `and' and `or' to a 
 *  primitive procedure  although it indeed a standard 
 *  one  however in our design using a primitive_proc
 *  type to i12n `and' and `or' means its arguments
 *  must be evaluated before calling its relevant func.
 *  however if its arguments have side-effects which
 *  will break the principal of `and' and `or'
 * though `not' is in a special manner  which can be
 *  i12n via primitive_proc structure
 * */
object *not_proc(object *arg)
{
    /* TODO: throw a exception if (is_empty_list(arg)) */
    object *result = car(arg);
    if (is_boolean(result) && result->data.boolean.value == false)
        return true_object;
    return false_object;
}

object *cons_proc(object *args)
{ return make_cons(car(args), cadr(args)); }

object *car_proc(object *arg)
{ return caar(arg); }

object *cdr_proc(object *arg)
{ return cdar(arg); }

object *set_car_proc(object *args)
{
    set_car(car(args), cadr(args));
    return ok_symbol;
}

object *set_cdr_proc(object *args)
{
    set_cdr(car(args), cadr(args));
    return ok_symbol;
}

object *list_proc(object *args)
{ return args; }

object *is_eq_proc(object *args)
{
    object *obj1 = car(args);
    object *obj2 = cadr(args);

    if (obj1->type != obj2->type) return false_object;
    
    switch (obj1->type) {
        case FIXNUM:
            return (obj1->data.fixnum.value ==
                    obj2->data.fixnum.value) ?
                true_object : false_object;

        case CHARACTER:
            return (obj1->data.character.value == 
                    obj2->data.character.value) ?
                true_object : false_object;

        case STRING:
            return (strcmp(obj1->data.string.value, 
                    obj2->data.string.value) == 0) ? 
                true_object : false_object;
        
        default: break;
    }
    
    /* generally comparision */
    return (obj1 == obj2) ? true_object : false_object;
}

object *apply_proc(object* args)
{
    /* actually code refers to eval(): is_application(exp) part */
    fprintf(stderr, "ERROR: apply is not callable\n");
    exit(1);
    return args;    /* comfort compiler */
}

/* code for (eval) syntax */

/* return the global environment */
object *interaction_environment_proc(object *unused_args)
{ return the_global_environment; }

object *setup_environment(void);

/* return a empty environment */
/*  equivalent form of (cons (cons '() '()) '())  e.g. ((())) */
object *null_environment_proc(object *unused_args)
{ return setup_environment(); }

object *make_environment(void);

/* return a standard-like environment with possible primitive procedures */
object *environment_proc(object *unused_args)
{ return make_environment(); }

object *eval_proc(object *args)
{
    fprintf(stderr, "ERROR: eval is not callable\n");
    exit(1);
    return args;
}

/* code for I/O utilities */

object *input(FILE *);
object *eval(object *, object *);

object *load_proc(object *arg)
{
    char *fname;
    FILE *fin;
    object *exp, *res = NULL;   /* res may used while uninitialized */
    fname = car(arg)->data.string.value;
    fin = fopen(fname, "r");
    if (fin == NULL) {
        fprintf(stderr, "ERROR: Cannot load file \"%s\"\n", fname);
        exit(1);
    }
    while ((exp = input(fin)) != NULL)
        res = eval(exp, the_global_environment);
    if (fclose(fin) == 0)   /* return zero if fin closed */
        return res;
    fprintf(stderr, "ERROR: Failed to close file \"%s\"\n", fname);
    exit(1);
}

object *open_input_port_proc(object *arg)
{
    char *fname;
    FILE *fin;
    fname = car(arg)->data.string.value;
    fin = fopen(fname, "r");
    if (fin != NULL)
        return make_input_port(fin);
    fprintf(stderr, "ERROR: Cannot open file \"%s\"\n", fname);
    exit(1);
}

object *close_input_port_proc(object *arg)
{
    /* which for the most of the time 0 is the excepted
     *  value  a jump-friendly design */
    if (fclose(car(arg)->data.input_port.stream) == 0)
        return ok_symbol;
    fprintf(stderr, "ERROR: Failed to close input port\n");
    exit(1);
}

object *is_input_port_proc(object *arg)
{ return is_input_port(car(arg)) ? true_object : false_object; }

object *read_proc(object *arg)
{
    FILE *fin = is_empty_list(arg) ? stdin :
                car(arg)->data.input_port.stream;
    object *res = input(fin);
    return (res != NULL) ? res : eof_object;
}

object *read_char_proc(object *arg)
{
    FILE *fin = is_empty_list(arg) ? stdin :
                car(arg)->data.input_port.stream;
    int ch = getc(fin);
    return (ch != EOF) ? make_character(ch) : eof_object;
}

int peek(FILE *);

object *peek_char_proc(object *arg)
{
    FILE *fin = is_empty_list(arg) ? stdin :
                car(arg)->data.input_port.stream;
    int ch = peek(fin);
    return (ch != EOF) ? make_character(ch) : eof_object;
}

object *is_eof_object_proc(object *arg)
{ return is_eof_object(car(arg)) ? true_object : false_object; }

object *open_output_port_proc(object *arg)
{
    char *fname = car(arg)->data.string.value;
    /* a defensive way is first check if fname exists */
    /*  if exists we won't open it and prompt repl-user */
    FILE *fout = fopen(fname, "w");     /* will empty content if file exists */
    if (fout != NULL)
        return make_output_port(fout);

    fprintf(stderr, "ERROR: Cannot open file \"%s\"\n", fname);
    exit(1);
}

object *close_output_port_proc(object *arg)
{
    if (fclose(car(arg)->data.output_port.stream) == 0)
        return ok_symbol;
    fprintf(stderr, "ERROR: Cannot close output port\n");
    exit(1);
}

object *is_output_port_proc(object *arg)
{ return is_output_port(car(arg)) ? true_object : false_object; }

object *write_char_proc(object *arg)
{
    object *chr = car(arg);
    arg = cdr(arg);     /* fetch its write port */
    FILE *fout = is_empty_list(arg) ? stdout :
                 car(arg)->data.output_port.stream;
    putc(chr->data.character.value, fout);
    fflush(fout);
    return ok_symbol;
}

void output(FILE *, object *);

object *write_proc(object *arg)
{
    object *exp = car(arg);
    arg = cdr(arg);
    FILE *fout = is_empty_list(arg) ? stdout : 
                 car(arg)->data.output_port.stream;
    output(fout, exp);
    fflush(fout);
    return ok_symbol;
}

object *error_proc(object *args)
{
    while (!is_empty_list(args)) {
        output(stderr, car(args));
        fprintf(stderr, " ");
        args = cdr(args);
    }
    fprintf(stderr, "\nTerminated\n");
    exit(1);
}

/* TODO: i12n (cd) (pwd) */

object *cd_proc(object *arg)
{
    /* another scheme is (cd) works as (pwd)
     *  when (cd) have no argument
     *  if (is_empty_list(arg)) return pwd_proc(NULL);
     * */
    char *pdir = car(arg)->data.string.value;
    if (chdir(pdir) == 0)   /* return zero if success */
        return ok_symbol;
    fprintf(stderr, "ERROR: Cannot change directory to \"%s\"\n", pdir);
    exit(1);
}

object *pwd_proc(object *unused_arg)
{
    /* throw a exception: if (!is_empty_list(unused_arg)) */
    static char cwd[512];
    char *pdir = getcwd(cwd, sizeof(cwd));
    if (pdir) return make_string(pdir);
    fprintf(stderr, "ERROR: Cannot get current directory\n");
    exit(1);
}

/* a ex-dirty i12n of (exit) */
object *exit_proc(object *unused_arg)
{ exit(1); }

/* code for environment */

#define enclosing_environment(env) cdr(env)     /* scoped? */
#define first_frame(env) car(env)  /* get first env. frame */

#define make_frame(variables, values) make_cons(variables, values)
#define frame_variables(frame) car(frame)
#define frame_values(frame) cdr(frame)

void add_binding_to_frame(object *var, object *val, object *frame)
{
    /* add variable with value to head of frame */
    set_car(frame, make_cons(var, car(frame)));
    set_cdr(frame, make_cons(val, cdr(frame)));
}

/* this macro only use once */
#define extend_environment(vars, vals, base_env) \
    make_cons(make_frame(vars, vals), base_env)

object *lookup_variable_value(object *var, object *env)
{
    object *frame, *vars, *vals;
    while (!is_empty_list(env)) {
        frame = first_frame(env);
        vars = frame_variables(frame);
        vals = frame_values(frame);
        while (!is_empty_list(vars)) {
            if (var == car(vars)) return car(vals);
            vars = cdr(vars);
            vals = cdr(vals);
        }
        env = enclosing_environment(env);
    }
    
    /* using var->data.string.value directly may insecure */
    fprintf(stderr, "ERROR: Unbound variable `%s'\n", var->data.string.value);
    exit(1);
}

/* almost the same structure of lookup_variable_value() */
void set_variable_value(object *var, object *val, object *env)
{
    object *frame, *vars, *vals;
    while (!is_empty_list(env)) {
        frame = first_frame(env);
        vars = frame_variables(frame);
        vals = frame_values(frame);
        while (!is_empty_list(vars)) {
            if (var == car(vars)) {
                set_car(vals, val);
                return ;
            }
            vars = cdr(vars);
            vals = cdr(vals);
        }
        env = enclosing_environment(env);
    }
    
    /* same comment refers to lookup_variable_value() */
    fprintf(stderr, "ERROR: Unbound variable `%s'\n", var->data.string.value);
    exit(1);
}

void define_variable(object *var, object *val, object *env)
{
    object *frame, *vars, *vals;
    frame = first_frame(env);       /* only use first frame */
    vars  = frame_variables(frame);
    vals  = frame_values(frame);
    
    /* only apply to global environment  e.g. global scope */
    while (!is_empty_list(vars)) {
        if (var == car(vars)) {
            set_car(vals, val);
            return ;
        }
        vars = cdr(vars);
        vals = cdr(vals);
    }

    add_binding_to_frame(var, val, frame);
}

object *setup_environment(void)
{
    /* make a (no vars no vals) empty env. */
    object *initial_env = extend_environment(
            the_empty_list, 
            the_empty_list, 
            the_empty_environment
    );
    return initial_env;
}

/* alternative of setup_environment() */
#ifdef DO_NOT_GENERATE_CODE
#define setup_environment()     \
    extend_environment(         \
        the_empty_list,         \
        the_empty_list,         \
        the_empty_environment   \
    )
#endif

void populate_environment(object *env)
{
#define add_procedure(proc_name, proc_ptr)  \
    define_variable(                        \
        make_symbol(proc_name),             \
        make_primitive_proc(proc_ptr),      \
        env                                 \
    )
    
    add_procedure("null?",      is_null_proc);
    add_procedure("boolean?",   is_boolean_proc);
    add_procedure("symbol?",    is_symbol_proc);
    add_procedure("integer?",   is_integer_proc);
    add_procedure("char?",      is_char_proc);
    add_procedure("string?",    is_string_proc);
    add_procedure("pair?",      is_pair_proc);
    add_procedure("procedure?", is_procedure_proc);

    add_procedure("char->integer",  char_to_integer_proc);
    add_procedure("integer->char",  integer_to_char_proc);
    add_procedure("number->string", number_to_string_proc);
    add_procedure("string->number", string_to_number_proc);
    add_procedure("symbol->string", symbol_to_string_proc);
    add_procedure("string->symbol", string_to_symbol_proc);

    add_procedure("+",         add_proc);
    add_procedure("-",         sub_proc);
    add_procedure("*",         mul_proc);
    add_procedure("quotient",  quotient_proc);
    add_procedure("remainder", remainder_proc);
    add_procedure("=",         is_number_equal_proc);
    add_procedure("<",         is_less_than_proc);
    add_procedure(">",         is_greater_than_proc);
    
    add_procedure("not", not_proc);
    
    add_procedure("cons",     cons_proc);
    add_procedure("car",      car_proc);
    add_procedure("cdr",      cdr_proc);
    add_procedure("set-car!", set_car_proc);
    add_procedure("set-cdr!", set_cdr_proc);
    add_procedure("list",     list_proc);
    
    add_procedure("eq?", is_eq_proc);

    add_procedure("apply", apply_proc);
    
    add_procedure("interaction-environment", interaction_environment_proc);
    add_procedure("null-environment", null_environment_proc);
    add_procedure("environment",      environment_proc);
    add_procedure("eval",             eval_proc);
    
    /* i'd say this kind of alignment is ugly */
    add_procedure("load",              load_proc);
    add_procedure("open-input-port",   open_input_port_proc);
    add_procedure("close-input-port",  close_output_port_proc);
    add_procedure("input-port?",       is_input_port_proc);
    add_procedure("read",              read_proc);
    add_procedure("read-char",         read_char_proc);
    add_procedure("peek-char",         peek_char_proc);
    add_procedure("eof-object?",       is_eof_object_proc);
    add_procedure("open-output-port",  open_output_port_proc);
    add_procedure("close-output-port", close_output_port_proc);
    add_procedure("output-port?",      is_output_port_proc);
    add_procedure("write",             write_proc);
    add_procedure("write-char",        write_char_proc);
    
    add_procedure("error", error_proc);
    
    add_procedure("cd",  cd_proc);
    add_procedure("pwd", pwd_proc);

    add_procedure("exit", exit_proc);
    
#undef add_procedure        /* not necessary */
}

object *make_environment(void)
{
    object *env = setup_environment();
    populate_environment(env);
    return env;
}

void initialize(void)
{
    false_object = make_object();
    false_object->type = BOOLEAN;
    false_object->data.boolean.value = false;

    true_object  = make_object();
    true_object->type  = BOOLEAN;
    true_object->data.boolean.value  = true;

    the_empty_list  = make_object();
    the_empty_list->type  = THE_EMPTY_LIST;

    symbol_table = the_empty_list;
    quote_symbol = make_symbol("quote");
    
    define_symbol = make_symbol("define");
    set_symbol    = make_symbol("set!");
    ok_symbol     = make_symbol("OK");

    if_symbol     = make_symbol("if");

    lambda_symbol = make_symbol("lambda");
    
    begin_symbol  = make_symbol("begin");

    cond_symbol   = make_symbol("cond");
    else_symbol   = make_symbol("else");

    let_symbol    = make_symbol("let");

    and_symbol    = make_symbol("and");
    or_symbol     = make_symbol("or");

    eof_object    = make_object();
    eof_object->type = EOF_OBJECT;

    the_empty_environment  = the_empty_list;
    the_global_environment = make_environment();
}

bool is_delimiter(int c)
{
    return isspace(c) || c == EOF ||
           c == '('   || c == ')' ||
           c == '"'   || c == ';';
}

bool is_initial(int c)
{
    return isalpha(c) || c == '*' || c == '/' || c == '<' ||
             c == '>' || c == '=' || c == '?' || c == '!';
}

int peek(FILE *in)
{
    int c = getc(in);
    ungetc(c, in);
    return c;
}

void skip_blank(FILE *in)
{
    int c;
    while ((c = getc(in)) != EOF) {
        if (isspace(c))
            continue;
        else if (c == ';')
            while (((c = getc(in)) != EOF) && (c != '\n'))
                continue;
        
        ungetc(c, in);
        break;
    }
}

void match_string(FILE *in, const char *str)
{
    int c;
    while (*str != '\0')
        if ((c = getc(in)) != *str++) break;
    if (*str == '\0') return ;
    fprintf(stderr, "ERROR: Unexcepted character code `%d'!!!\n", c);
    exit(1);
}

void peek_delimiter(FILE *in)
{
    if (is_delimiter(peek(in))) return ;
    fprintf(stderr, "ERROR: Character now followed by delimiter\n");
    exit(1);
}

object *read_character(FILE *in)
{
    int c = getc(in);
    switch (c) {
        case EOF:
            fprintf(stderr, "ERROR: Incomplete character literal\n");
            exit(1);

        case 's':
            if (peek(in) != 'p') break;
            match_string(in, "pace");
            c = ' ';
            break;

        case 'n':       /* linefeed */
            if (peek(in) != 'e') break;
            match_string(in, "ewline");
            c = '\n';
            break;

        case 't':
            if (peek(in) != 'a') break;
            match_string(in, "ab");
            c = '\t';
            break;
        
        case 'r':
            if (peek(in) != 'e') break;
            match_string(in, "eturn");
            c = '\r';
            break;
    }
    peek_delimiter(in);
    if (peek(in) != '\n') skip_blank(in);
    return make_character(c);
}

object *input(FILE *);

object *read_pair(FILE *in)
{
    int c;
    object *car_obj, *cdr_obj;
    skip_blank(in);
    
    if ((c = getc(in)) == ')')
        return the_empty_list;
    
    ungetc(c, in);
    car_obj = input(in);
    skip_blank(in);
    if ((c = getc(in)) == '.') {
        if (!is_delimiter(peek(in))) {
            fprintf(stderr, "ERROR: Dot do not followed by delimiter\n");
            exit(1);
        }
        cdr_obj = input(in);
        skip_blank(in);
        if (getc(in) == ')')
            return make_cons(car_obj, cdr_obj);
        fprintf(stderr, "ERROR: Missing right parenthesis\n");
        exit(1);
    }
    
    ungetc(c, in);
    cdr_obj = read_pair(in);
    return make_cons(car_obj, cdr_obj);
}

object *input(FILE *in)
{
    skip_blank(in);
    int c = getc(in);
    /* allow empty string */
    if (c == '\n' || c == EOF) return NULL;
    short sign = 1;
    long num = 0;
    #define BUF_SIZE 1024
    char buffer[BUF_SIZE];
    uint i = 0;
    char *p;
    
    /* read for boolean or character */
    if (c == '#') {
        c = getc(in);
        
        if ((c == 't' || c == 'f') && peek(in) != '\n')
            skip_blank(in);
        
        switch (c) {
            case 't':  return true_object;

            case 'f':  return false_object;

            case '\\': return read_character(in);

            default:
                fprintf(stderr, "ERROR: Unknown boolean literal\n");
                exit(1);
        }
    }
    /* read for fixnum */
    else if (isdigit(c) ||
            (c == '-' && isdigit(peek(in))) ||
            (c == '+' && isdigit(peek(in)))) {
        if (c == '-')       sign = -1;
        else if (c != '+')  ungetc(c, in);
        
        while (isdigit(c = getc(in)))
            num = (num * 10) + (c - '0');
        
        num *= sign;
        
        if (!is_delimiter(c)) {
            fprintf(stderr, "ERROR: Fixnum not followed by delimiter\n");
            exit(1);
        }
        
        ungetc(c, in);
        if (c != '\n') skip_blank(in);
        return make_fixnum(num);
    }
    /* read for string */
    else if (c == '"') {
        while ((c = getc(in)) != '"') {
            if (c == '\\') {
                p = strchr(unescaped, (c = getc(in)));
                if (p != NULL) c = escaped[p - unescaped];
            }
            
            if (c == EOF) {
                fprintf(stderr, "ERROR: Nonterminated string literal\n");
                exit(1);
            }
            
            if (i < BUF_SIZE-1) buffer[i++] = c;
            else {
                fprintf(stderr, "ERROR: Reached the end of string\n");
                exit(1);
            }
        }
        buffer[i] = '\0';
        if (peek(in) != '\n') skip_blank(in);
        return make_string(buffer);
    }
    /* read for empty list or pair */
    else if (c == '(') {
        object *obj = read_pair(in);
        if (peek(in) != '\n') skip_blank(in);
        return obj;
    }
    /* read for symbol */
    else if (is_initial(c) || ((c == '+' || c == '-') && is_delimiter(peek(in)))) {
        while (is_initial(c) || isdigit(c) || c == '+' || c == '-') {
            if (i >= BUF_SIZE-1) {
                fprintf(stderr, "ERROR: Reached maximum symbol length %u\n", BUF_SIZE);
                exit(1);
            }
            buffer[i++] = c;
            c = getc(in);
        }

        if (!is_delimiter(c)) {
            fprintf(stderr, "ERROR: Symbol not followed by delimiter\n");
            exit(1);
        }

        buffer[i] = '\0';
        ungetc(c, in);
        if (c != '\n') skip_blank(in);      /* c already lookaheaded */
        return make_symbol(buffer);
    }
    /* read for quoted expression */
    else if (c == '\'')
        /* format of (cons quote (cons xxx the_empty_list)) */
        return make_cons(quote_symbol, make_cons(input(in), the_empty_list));
    
    fprintf(stderr, "Unexcepted input `%d'\n", c);
    exit(1);
}

/* code for quotation */

#define is_self_evaluating(exp) (   \
    is_boolean(exp)   ||            \
    is_fixnum(exp)    ||            \
    is_character(exp) ||            \
    is_string(exp)                  \
)

#define is_variable(exp) is_symbol(exp)

/* check list as a form of (tag exp ...) */
bool is_tagged_list(object *exp, object *tag)
{
    const object *car_obj;
    if (is_pair(exp)) {
        car_obj = car(exp);
        return is_symbol(car_obj) && car_obj == tag;
    }
    return false;
}

#define is_quoted(exp) is_tagged_list(exp, quote_symbol)

#define text_of_quotation(exp) cadr(exp)

/* code for environment */

#define is_assignment(exp) is_tagged_list(exp, set_symbol)
#define assignment_variable(exp)  cadr(exp)
#define assignment_value(exp)    caddr(exp)

#define is_definition(exp) is_tagged_list(exp, define_symbol)

#define definition_variable(exp) ({ \
    is_symbol(cadr(exp)) ?          \
         cadr(exp)       :          \
        caadr(exp);   /* proc. */   \
})

/* note that make_lambda above this line is
 *  not defined  however since it's a macro
 * */
#define definition_value(exp) ({            \
    is_symbol(cadr(exp)) ?                  \
        caddr(exp)       :                  \
        /* fetch paras. and its body */     \
        make_lambda(cdadr(exp), cddr(exp)); \
})

/* code for basic if syntax */

/* make_if() mainly used in cond syntax */
object *make_if(object *pred, object *conseq, object *alter)
{
    return make_cons(if_symbol, 
        make_cons(pred, 
            make_cons(conseq, 
                make_cons(alter, the_empty_list))));
}

#define is_if(exp) is_tagged_list(exp, if_symbol)

#define if_predicate(exp)   cadr(exp)
#define if_consequent(exp) caddr(exp)
#define if_alternative(exp) (       \
    is_empty_list(cdddr(exp)) ?     \
        false_object : cadddr(exp)  \
    )

/* code for primitive procedure */

/* mainly used in let syntax */
#define make_application(operator, operands) \
    make_cons(operator, operands)

#define is_application(exp) is_pair(exp)
#define operator(exp)       car(exp)
#define operands(exp)       cdr(exp)

#define is_no_operands(ops) is_empty_list(ops)
#define first_operand(ops)  car(ops)
#define rest_operands(ops)  cdr(ops)

/* code for compound procedure */

#define make_lambda(paras, body) \
    make_cons(lambda_symbol, make_cons(paras, body))

#define is_lambda(exp) is_tagged_list(exp, lambda_symbol)

#define lambda_paras(exp) cadr(exp)
#define lambda_body(exp)  cddr(exp)

/* code for begin syntax */

#define make_begin(exp)    make_cons(begin_symbol, exp)
#define is_begin(exp)      is_tagged_list(exp, begin_symbol)
#define begin_actions(exp) cdr(exp)

/* utils used for compound procedure & begin syntax */

#define is_last_exp(seq) is_empty_list(cdr(seq))
#define first_exp(seq)   car(seq)
#define rest_exps(seq)   cdr(seq)

/* code for cond syntax */

#define is_cond(exp)      is_tagged_list(exp, cond_symbol)
#define cond_clauses(exp) cdr(exp)

#define cond_predicate(cla) car(cla)
#define cond_actions(cla)   cdr(cla)

#define is_cond_else_clause(cla) (cond_predicate(cla) == else_symbol)

object *sequence_to_exp(object *seq)
{
    if (is_empty_list(seq))
        return seq;
    else if (is_last_exp(seq))
        return first_exp(seq);
    /* using begin to wrap sequences up */
    return make_begin(seq);
}

object *expand_clauses(object *clauses)
{
    object *first, *rest;
    if (is_empty_list(clauses))
        return false_object;
    first = car(clauses);
    rest  = cdr(clauses);
    if (is_cond_else_clause(first)) {
        if (is_empty_list(rest))
            return sequence_to_exp(cond_actions(first));

        fprintf(stderr, "ERROR: else clause isn't last predicate of cond\n");
        exit(1);
    }
    return make_if(
        cond_predicate(first), 
        sequence_to_exp(cond_actions(first)), 
        /* using recursive call to extend rest of clauses 
         *  it's inefficient when cond have plenty of predicates
         *  its else-clause increasing in a linearly-recursive way
         * it's a syntax-like way to transform cond into equivalent if
         * TODO: using a another way to de-deepen if-structure
         * */
        expand_clauses(rest)
    );
}

#define cond_to_if(exp) expand_clauses(cond_clauses(exp))

/* code for let syntax */

#define is_let(exp) is_tagged_list(exp, let_symbol)

#define let_bindings(exp) cadr(exp)
#define let_body(exp)     cddr(exp)

#define binding_parameter(binding)  car(binding)
#define binding_argument(binding)  cadr(binding)

/* fetch let's initial parameters
 * TODO: de-recursive-lize  using a loop to accelerate
 * */
object *bindings_parameters(object *bindings)
{
    return is_empty_list(bindings) ?
        the_empty_list             :
        make_cons(
            binding_parameter(car(bindings)), 
            bindings_parameters(cdr(bindings))
        );

#ifdef DO_NOT_GENERATE_CODE
    /* THE FOLLOWING CODE WILL NOT BE EXECUTED!
     * the following code is a altertive way to
     *  improve functionality of bindings_parameters()
     * which using iterative way to accelerate
     * note that it operates like a stack
     *  say  make parameters (x y z '()) into (z y x '())
     * so if you using these code  you must modify
     *  code of bindings_arguments() the same way
     *  to reverse let's arguments in a same manner
     * TODO: using a singled-linked list to accelerate
     * */
    object *parameter_list = the_empty_list;
    while (!is_empty_list(bindings)) {
        parameter_list = make_cons(
            binding_parameter(car(bindings)), 
            parameter_list
        );
        bindings = cdr(bindings);
    }
    return parameter_list;
#endif
}

/* fetch let's initial values */
object *bindings_arguments(object *bindings)
{
    return is_empty_list(bindings) ? 
        the_empty_list             :
        make_cons(
            binding_argument(car(bindings)), 
            bindings_arguments(cdr(bindings))
        );
}

#define let_parameters(exp) bindings_parameters(let_bindings(exp))
#define let_arguments(exp)  bindings_arguments(let_bindings(exp))

/* the principal behind let is convert let
 *  into a lambda function and passing let's
 *  initial value to call the lambda function
 * in a way like:
 *  ((lambda (parameters ..) actions) arguments)
 * */
#define let_to_application(exp)  \
    make_application(            \
        make_lambda(             \
            let_parameters(exp), \
            let_body(exp)        \
        ),                       \
        let_arguments(exp)       \
    )

/* code for `and' and `or' syntax */

#define is_and(exp)    is_tagged_list(exp, and_symbol)
#define and_tests(exp) cdr(exp)
#define is_or(exp)     is_tagged_list(exp, or_symbol)
#define or_tests(exp)  cdr(exp)

/* code for apply procedure */

#define apply_operator(args) car(args)

object *prepare_apply_operands(object *args)
{
    if (is_empty_list(cdr(args)))
        return car(args);
    return make_cons(
        car(args), 
        prepare_apply_operands(cdr(args))
    );
    
#ifdef DO_NOT_GENERATE_CODE
    /* THE FOLLOWING CODE WILL NOT BE EXECUTED!
     * a linearly-loop version of prepare_apply_operands()
     * TODO: throw an exception when its operand is null
     * */
    object *args_cons = car(args);
    while (!is_empty_list((args = cdr(args))))
        args_cons = make_cons(args_cons, car(args));
    return args_cons;
#endif
}

#define apply_operands(args) prepare_apply_operands(cdr(args))

/* code for (eval) procedure */

#define eval_expression(args)   car(args)
#define eval_environment(args) cadr(args)

/* code for evaluation */

object *eval(object *, object *);

/* TODO: de-recursive-lize */
object *list_of_values(object *exps, object *env)
{
    if (is_no_operands(exps)) return the_empty_list;
    /* using loop to accelerate recursive call */
    return make_cons(
        eval(first_operand(exps), env), 
        list_of_values(rest_operands(exps), env)
    );
}

object *eval_assignment(object *exp, object *env)
{
    set_variable_value(
        assignment_variable(exp), 
        eval(assignment_value(exp), env),
        env
    );
    return ok_symbol;
}

#ifdef DO_NOT_GENERATE_CODE
#define eval_assignment(exp, env) ({        \
    set_variable_value(                     \
        assignment_variable(exp),           \
        eval(assignment_value(exp), env),   \
        env                                 \
    );                                      \
    ok_symbol;      /* as result */         \
})
#endif

object *eval_definition(object *exp, object *env)
{
    define_variable(
        definition_variable(exp), 
        eval(definition_value(exp), env), 
        env
    );
    return ok_symbol;
}

/* code for evaluation */

object *eval(object *exp, object *env)
{
    object *proc, *args, *res;
    
tailcall:
    
    if (is_self_evaluating(exp))
        return exp;
    else if (is_quoted(exp))
        return text_of_quotation(exp);
    else if (is_variable(exp))
        return lookup_variable_value(exp, env);
    else if (is_assignment(exp))
        return eval_assignment(exp, env);
    else if (is_definition(exp))
        return eval_definition(exp, env);
    else if (is_if(exp)) {
        exp = is_true(eval(if_predicate(exp), env)) ? 
                if_consequent(exp)  :
                if_alternative(exp) ;
        goto tailcall;
    }
    /* is_lambda() must come before is_application()
     *  say  if exp satisfied is_applicantion()  e.g. is_pair()
     *  then it must satified is_lambda() however  the second
     *  is which we needed to satisfies first
     * */
    else if (is_lambda(exp))
        return make_compound_proc(lambda_paras(exp), lambda_body(exp), env);
    else if (is_begin(exp)) {
        /* TODO: throw a exception if is_empty_list(exp) */
        exp = begin_actions(exp);
        while (!is_last_exp(exp)) {
            eval(first_exp(exp), env);
            exp = rest_exps(exp);
        }
        exp = first_exp(exp);
        goto tailcall;
    }
    else if (is_cond(exp)) {
        exp = cond_to_if(exp);
        goto tailcall;
    }
    else if (is_let(exp)) {
        exp = let_to_application(exp);
        goto tailcall;
    }
    else if (is_and(exp)) {
        exp = and_tests(exp);
        if (is_empty_list(exp)) return true_object;
        while (!is_last_exp(exp)) {
            res = eval(first_exp(exp), env);
            if (is_false(res)) return res;
            exp = rest_exps(exp);
        }
        exp = first_exp(exp);
        goto tailcall;
    }
    else if (is_or(exp)) {
        exp = or_tests(exp);
        if (is_empty_list(exp)) return false_object;
        while (!is_last_exp(exp)) {
            res = eval(first_exp(exp), env);
            if (is_true(res)) return res;
            exp = rest_exps(exp);
        }
        exp = first_exp(exp);
        goto tailcall;
    }
    /* since is_application() is actually is_pair() so it should
     *  at near end of else-if clauses  in a lowest priority
     * */
    else if (is_application(exp)) {
        /* proc actually a symbol(variable)  got its value after eval. */
        proc = eval(operator(exp), env);
        args = list_of_values(operands(exp), env);
        
        /* handle (apply) specially for tailcall requirement
         *  map it to a primitive procedure or a compound procedure
         * */
        if (is_primitive_proc(proc) && proc->data.primitive_proc.fn == apply_proc) {
            proc = apply_operator(args);
            args = apply_operands(args);
        }
        
        /* this kind of (eval) only accept two arguments */
        if (is_primitive_proc(proc) && proc->data.primitive_proc.fn == eval_proc) {
            exp = eval_expression(args);    /* must be quoted for next eval() */
            env = eval_environment(args);
            goto tailcall;
        }

        /* note that after list_of_values() all args. were evaluated */
        if (is_primitive_proc(proc))
            return (proc->data.primitive_proc.fn)(args);

        if (is_compound_proc(proc)) {
            /* switch to lexically-scoped environment */
            env = extend_environment(
                proc->data.compound_proc.paras, 
                args, 
                proc->data.compound_proc.env
            );
            exp = proc->data.compound_proc.body;
            /* evaluate its body until reached last exp. */
            while (!is_last_exp(exp)) {
                eval(first_exp(exp), env);
                exp = rest_exps(exp);
            }
            /* fetch the last exp. and evaluate it */
            exp = first_exp(exp);
            /* using a directly jump instead of func. call */
            goto tailcall;
        }
        
        fprintf(stderr, "ERROR: Unknown procedure type\n");
        exit(1);
    }
    
    fprintf(stderr, "ERROR: Unknown S-expression type\n");
    exit(1);
}

/* output result -- know the value of everything but the cost of nothing */
/* TODO: introduce a macro to indicate the default action when a action done 
 *          e.g. print the result immediately or whatsoever else
 * */

/* which argument should be const */
void output_pair(FILE *fout, object *pair)
{
    object *car_obj = car(pair);
    object *cdr_obj = cdr(pair);
    output(fout, car_obj);
    if (cdr_obj->type == PAIR) {
        putchar(' ');
        output_pair(fout, cdr_obj);
    }
    else if (cdr_obj->type != THE_EMPTY_LIST) {
        printf(" . ");
        output(fout, cdr_obj);
    }
}

/* which obj should be const */
void output(FILE *fout, object *obj)
{
    int c;
    const char *str, *p;
    
    switch (obj->type) {
        case THE_EMPTY_LIST:
            fprintf(fout, "()");
            break;
        
        case BOOLEAN:
            fprintf(fout, "#%c", obj == true_object ? 't' : 'f');
            break;

        case FIXNUM:
            fprintf(fout, "%ld", obj->data.fixnum.value);
            break;

        case CHARACTER:
            c = obj->data.character.value;
            fprintf(fout, "#\\");
            switch (c) {
                case '\n':
                    fprintf(fout, "newline");
                break;

                case '\t':
                    fprintf(fout, "tab");
                break;
                
                case '\r':
                    fprintf(fout, "return");
                break;
                
                case ' ':
                    fprintf(fout, "space");
                break;
                
                default:
                    putc(c, fout);
            }
            break;
        
        case STRING:
            str = obj->data.string.value;
            putchar('"');       /* print string delimiter to stdout */
            while (*str != '\0') {
                p = strchr(escaped, *str);
                if (p != NULL)
                    fprintf(fout, "\\%c", unescaped[p - escaped]);
                else
                    putc(*str, fout);
                str++;
            }
            putchar('"');
            break;

        case PAIR:
            putc('(', fout);
            output_pair(fout, obj);
            putc(')', fout);
            break;
        
        case SYMBOL:
            fprintf(fout, "%s", obj->data.symbol.value);
            break;
        
        case PRIMITIVE_PROC:
            /* TODO: add `struct object *sym' to primitive_proc
             *  and compound_proc for binding its variable(symbol) name
             * which display procedure in a way of:
             *  fprintf(fout, "#<xx-procedure %s>", sym->data.symbol.value);
             * */
            fprintf(fout, "#<primitive-procedure>");
            break;

        case COMPOUND_PROC:
            fprintf(fout, "#<compound-procedure>");
            break;
        
        case INPUT_PORT:
            fprintf(fout, "#<input-port>");
            break;

        case OUTPUT_PORT:
            fprintf(fout, "#<output-port>");
            break;
        
        case EOF_OBJECT:
            fprintf(fout, "#<eof>");
            break;

        default:
            fprintf(stderr, "ERROR: Unknown type id %d\n", obj->type);
            exit(1);
    }
}

int main(void)
{
    object *exp;
    printf("Welcome to Bootstrap Scheme %s  CTRL+C to quit.\n", SCHEME_VERSION);
    
    initialize();
    
    while (true) {
        printf("> ");
        /* TODO: introduce a PS2  say, like "+ "
         *  and support control key-hit  like Home to head of current line
         *  End to tail of current line Up-Down-arrow for expression history
         * */
        exp = input(stdin);
        if (exp == NULL) break;  /* or continue; */
        output(stdout, eval(exp, the_global_environment));
        puts("");
    }
    putchar('\n');
    return 0;
}

