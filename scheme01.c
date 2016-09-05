#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef bool
    typedef enum {false, true} bool;
    #define bool bool
#endif

#define SCHEME_VER "v0.1"

typedef enum { FIXNUM } object_t;

typedef struct {
    object_t type;
    union {
        struct {
            long value;
        } fixnum;
    } data;
} object;

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

bool is_delimiter(int c)
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
    if (isdigit(c) ||
            (c == '-' && isdigit(peek(in))) ||
            (c == '+' && isdigit(peek(in)))) {
        if (c == '-')       sign = -1;
        else if (c != '+')  ungetc(c, in);
        
        while (isdigit(c = getc(in)))
            num = (num * 10) + (c - '0');
        
        num *= sign;
        
        if (!is_delimiter(c)) {
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
    while (true) {
        printf("> ");
        iwrite(eval(iread(stdin)));
        puts("");
    }
    return 0;
}

