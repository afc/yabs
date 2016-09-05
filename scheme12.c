/* future reference: http://goo.gl/zrhKT1 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* case is sensitive in R6 and R7 */

#ifndef bool
    typedef enum {false, true} bool;
    #define bool bool
#endif

typedef unsigned int uint;

#define SCHEME_VERSION "v1.2"

/* THE_EMPTY_LIST for the empty list type  */
typedef enum {
    THE_EMPTY_LIST, BOOLEAN, FIXNUM, CHARACTER, 
    STRING, PAIR, SYMBOL, PRIMITIVE_PROC
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

/* map between the unescaped and escaped characters */
static const char *unescaped = "abfnrtv\\'\"?";
static const char   *escaped = "\a\b\f\n\r\t\v\\\'\"\?";

#define is_empty_list(obj) (obj == the_empty_list)

#define is_boolean(obj)    (obj->type == BOOLEAN)
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

/* code for object-generation  no memory reclaim! */

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

/* code for primitive procedures */

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
{ return is_primitive_proc(car(arg)) ? true_object : false_object; }

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

/* TODO: <= >= and or not */
object *and_proc(object *args)
{
    object *result = true_object;
    while (!is_empty_list(args)) {
        result = car(args);
        if (is_boolean(result) && result->data.boolean.value == false)
            return false_object;
        args = cdr(args);
    }
    return result;
}

object *or_proc(object *args)
{
    object *result = false_object;
    while (!is_empty_list(args)) {
        result = car(args);
        if (is_boolean(result) && result->data.boolean.value == false) {
            args = cdr(args);
            continue;
        }
        break;
    }
    return result;
}

object *not_proc(object *arg)
{
    object *result = car(arg);
    /* TODO: throw a exception if (!is_empty_list(cdr(arg))) */
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
    the_empty_environment  = the_empty_list;
    the_global_environment = setup_environment();

    if_symbol     = make_symbol("if");
    
#define add_procedure(proc_name, proc_ptr)  \
    define_variable(                        \
        make_symbol(proc_name),             \
        make_primitive_proc(proc_ptr),      \
        the_global_environment              \
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
    
    add_procedure("and", and_proc);
    add_procedure("or",  or_proc);
    add_procedure("not", not_proc);
    
    add_procedure("cons",     cons_proc);
    add_procedure("car",      car_proc);
    add_procedure("cdr",      cdr_proc);
    add_procedure("set-car!", set_car_proc);
    add_procedure("set-cdr!", set_cdr_proc);
    add_procedure("list",     list_proc);
    
    add_procedure("eq?", is_eq_proc);
    
#undef add_procedure
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
        is_boolean(exp)   ||        \
        is_fixnum(exp)    ||        \
        is_character(exp) ||        \
        is_string(exp)              \
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
#define assignment_variable(exp) cadr(exp)
#define assignment_value(exp) caddr(exp)

#define is_definition(exp) is_tagged_list(exp, define_symbol)
#define definition_variable(exp) cadr(exp)
#define definition_value(exp) caddr(exp)

/* code for basic if syntax */

#define is_if(exp) is_tagged_list(exp, if_symbol)

#define if_predicate(exp)   cadr(exp)
#define if_consequent(exp) caddr(exp)
#define if_alternative(exp) (       \
    is_empty_list(cdddr(exp)) ?     \
        false_object : cadddr(exp)  \
    )

/* code for primitive procedure */

#define is_application(exp) is_pair(exp)
#define operator(exp)       car(exp)
#define operands(exp)       cdr(exp)

#define is_no_operands(ops) is_empty_list(ops)
#define first_operand(ops)  car(ops)
#define rest_operands(ops)  cdr(ops)

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

/* code for evaluation */

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
    object *proc, *args;
    
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
    else if (is_application(exp)) {
        /* proc actually a symbol(variable)  got its value after eval. */
        proc = eval(operator(exp), env);
        /*  here need a type checking if proc a primitive procedure */
        /*  if (!is_primitive_proc(proc)) { ... } */
        args = list_of_values(operands(exp), env);
        /* note that after list_of_values() all args. were evaluated */
        return (proc->data.primitive_proc.fn)(args);
    }
    
    fprintf(stderr, "ERROR: Unknown S-expression type\n");
    exit(1);
}

/* output result -- know the value of everything but the cost of nothing */
/* TODO: introduce a macro to indicate the default action when a action done 
 *          e.g. print the result immediately or whatsoever else
 * */

void output(object *);

/* which argument should be const */
void output_pair(object *pair)
{
    object *car_obj = car(pair);
    object *cdr_obj = cdr(pair);
    output(car_obj);
    if (cdr_obj->type == PAIR) {
        putchar(' ');
        output_pair(cdr_obj);
    }
    else if (cdr_obj->type != THE_EMPTY_LIST) {
        printf(" . ");
        output(cdr_obj);
    }
}

/* which obj should be const */
void output(object *obj)
{
    if (obj == NULL) return ;

    int c;
    const char *str, *p;
    
    switch (obj->type) {
        case THE_EMPTY_LIST:
            printf("()");
            break;
        
        case BOOLEAN:
            printf("#%c", obj == true_object ? 't' : 'f');
            break;

        case FIXNUM:
            printf("%ld", obj->data.fixnum.value);
            break;

        case CHARACTER:
            c = obj->data.character.value;
            printf("#\\");
            switch (c) {
                case '\n':
                    printf("newline");
                break;

                case '\t':
                    printf("tab");
                break;
                
                case '\r':
                    printf("return");
                break;
                
                case ' ':
                    printf("space");
                break;
                
                default:
                    putchar(c);
            }
            break;
        
        case STRING:
            str = obj->data.string.value;
            putchar('"');
            while (*str != '\0') {
                p = strchr(escaped, *str);
                if (p != NULL)
                    printf("\\%c", unescaped[p - escaped]);
                else
                    putchar(*str);
                str++;
            }
            putchar('"');
            break;

        case PAIR:
            putchar('(');
            output_pair(obj);
            putchar(')');
            break;
        
        case SYMBOL:
            printf("%s", obj->data.symbol.value);
            break;
        
        case PRIMITIVE_PROC:
            /* TODO: add `struct object *def' to data.primitive_proc
             *  add a argument to make_primitive_proc() binding its def.
             *
             *  ;object *def_obj = obj->data.primitive_proc.def;
             *  printf("#<procedure %s>", def_obj->data.symbol.value);
             *
             *  which display in this way can be accurate probably
             * */
            printf("#<procedure>");
            break;
        
        default:
            fprintf(stderr, "ERROR: Unknown type id %d\n", obj->type);
            exit(1);
    }
}

int main(void)
{
    printf("Welcome to Bootstrap Scheme %s  CTRL+C to quit.\n", SCHEME_VERSION);
    
    initialize();
    
    while (true) {
        printf("> ");
        /* TODO: introduce a PS2  say, like "+ " */
        output(eval(input(stdin), the_global_environment));
        puts("");
    }
    return 0;
}

