#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef bool
    typedef enum {false, true} bool;
    #define bool bool
#endif

#define SCHEME_VER "v0.2"

typedef enum { BOOLEAN, FIXNUM } object_t;

typedef struct {
    object_t type;
    union {
        struct {
            bool value;
        } boolean;
        struct {
            long value;
        } fixnum;
    } data;
} object;

/* for boolean singleton */
static object *falseobj, *trueobj;

/* no memory reclaim! */
object *mkobject(void)
{
    object *obj = (object *) malloc(sizeof(object));
    if (obj != NULL) return obj;
    fprintf(stderr, "ERROR: Memory exhausted\n");
    exit(-4);
}

object *mkfixnum(long num)
{
    object *obj = mkobject();
    obj->type = FIXNUM;
    obj->data.fixnum.value = num;
    return obj;
}

void initialize(void)
{
    falseobj = mkobject();
    falseobj->type = BOOLEAN;
    falseobj->data.boolean.value = false;

    trueobj  = mkobject();
    trueobj->type  = BOOLEAN;
    trueobj->data.boolean.value  = true;
}

bool isdelimiter(int c)
{
    return isspace(c) || c == EOF ||
           c == '('   || c == ')' ||
           c == '"'   || c == ';';
}

int peek(FILE *in)
{
    int c = getc(in);
    ungetc(c, in);
    return c;
}

/* clean up space */
void clspace(FILE *in)
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

object * iread(FILE *in)
{
    clspace(in);
    int c = getc(in);
    /* allow empty string */
    if (c == EOF) return NULL;
    short sign = 1;
    long num = 0;
    
    if (c == '#') {
        c = getc(in);
        /* note that Scheme is case-insensitive */
        switch (c | 1<<5) {
            case 't': return trueobj;
            case 'f': return falseobj;
            default:
                fprintf(stderr, "ERROR: Unknown boolean literal\n");
                exit(1);
        }
    }
    else if (isdigit(c) ||
            (c == '-' && isdigit(peek(in))) ||
            (c == '+' && isdigit(peek(in)))) {
        if (c == '-')       sign = -1;
        else if (c != '+')  ungetc(c, in);
        
        while (isdigit(c = getc(in)))
            num = (num * 10) + (c - '0');
        
        num *= sign;
        
        if (!isdelimiter(c)) {
            fprintf(stderr, "ERROR: Fixnum not followed by delimiter\n");
            exit(-2);
        }
        
        ungetc(c, in);
        if (c != '\n') clspace(in);
        return mkfixnum(num);
    }
    fprintf(stderr, "Unexcepted input `%d'\n", c);
    exit(-3);
    
    /* return NULL; */
}

object * eval(object *exp)
{ return exp; }

void iwrite(object *obj)
{
    if (obj == NULL) return ;
    switch (obj->type) {
        case BOOLEAN:
            printf("#%c", obj == trueobj ? 't' : 'f');
        break;

        case FIXNUM:
            printf("%ld", obj->data.fixnum.value);
        break;

        default:
            fprintf(stderr, "ERROR: Unknown type id %d\n", obj->type);
            exit(-1);
    }
}

int main(void)
{
    printf("Bootstrap Scheme %s  CTRL+C to quit.\n", SCHEME_VER);
    
    initialize();

    while (true) {
        printf("> ");
        iwrite(eval(iread(stdin)));
        puts("");
    }
    return 0;
}

