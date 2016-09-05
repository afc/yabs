#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* case is sensitive in R6 and R7 */

#ifndef bool
    typedef enum {false, true} bool;
    #define bool bool
#endif

#define SCHEME_VER "v0.3"

typedef enum { BOOLEAN, FIXNUM, CHARACTER } object_t;

typedef struct {
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
    obj->type   = FIXNUM;
    obj->data.fixnum.value = num;
    return obj;
}

object *mkchar(char value)
{
    object *obj = mkobject();
    obj->type   = CHARACTER;
    obj->data.character.value = value;
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

void match_string(FILE *in, const char *str)
{
    int c;
    while (*str != '\0')
        if ((c = getc(in)) != *str++) break;
    if (*str == '\0') return ;
    fprintf(stderr, "ERROR: Unexcepted character code `%d'\n", c);
    exit(1);
}

void peek_delimiter(FILE *in)
{
    if (isdelimiter(peek(in))) return ;
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
            peek_delimiter(in);
            return mkchar(' ');

        case 'n':       /* linefeed */
            if (peek(in) != 'e') break;
            match_string(in, "ewline");
            peek_delimiter(in);
            return mkchar('\n');

        case 't':
            if (peek(in) != 'a') break;
            match_string(in, "ab");
            peek_delimiter(in);
            return mkchar('\t');
        
        case 'r':
            if (peek(in) != 'e') break;
            match_string(in, "eturn");
            peek_delimiter(in);
            return mkchar('\r');
    }
    peek_delimiter(in);
    return mkchar(c);
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
        switch (c) {
            case 't':
            case 'T':  return trueobj;

            case 'f':
            case 'F':  return falseobj;

            case '\\': return read_character(in);

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
}

object * eval(object *exp)
{ return exp; }

void iwrite(object *obj)
{
    int c;
    if (obj == NULL) return ;
    switch (obj->type) {
        case BOOLEAN:
            printf("#%c", obj == trueobj ? 't' : 'f');
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

