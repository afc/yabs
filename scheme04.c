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

#define SCHEME_VER "v0.4"

typedef enum { BOOLEAN, FIXNUM, CHARACTER, STRING } object_t;

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
        struct {
            char *value;
        } string;
    } data;
} object;

/* for boolean singleton */
static object *falseobj, *trueobj;

/* map between the unescaped and escaped characters */
static const char* unescaped = "abfnrtv\\'\"?";
static const char*   escaped = "\a\b\f\n\r\t\v\\\'\"\?";

/* no memory reclaim! */
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

object *make_char(char value)
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

void initialize(void)
{
    falseobj = make_object();
    falseobj->type = BOOLEAN;
    falseobj->data.boolean.value = false;

    trueobj  = make_object();
    trueobj->type  = BOOLEAN;
    trueobj->data.boolean.value  = true;
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
    fprintf(stderr, "ERROR: Unexcepted character code `%d'\n", c);
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
            peek_delimiter(in);
            return make_char(' ');

        case 'n':       /* linefeed */
            if (peek(in) != 'e') break;
            match_string(in, "ewline");
            peek_delimiter(in);
            return make_char('\n');

        case 't':
            if (peek(in) != 'a') break;
            match_string(in, "ab");
            peek_delimiter(in);
            return make_char('\t');
        
        case 'r':
            if (peek(in) != 'e') break;
            match_string(in, "eturn");
            peek_delimiter(in);
            return make_char('\r');
    }
    peek_delimiter(in);
    return make_char(c);
}

object * input(FILE *in)
{
    skip_blank(in);
    int c = getc(in);
    /* allow empty string */
    if (c == EOF) return NULL;
    short sign = 1;
    long num = 0;
    #define BUF_SIZE 1024
    char buffer[BUF_SIZE];
    uint i = 0;
    char *p;

    if (c == '#') {
        c = getc(in);
        switch (c) {
            case 't':  return trueobj;

            case 'f':  return falseobj;

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
        
        if (!is_delimiter(c)) {
            fprintf(stderr, "ERROR: Fixnum not followed by delimiter\n");
            exit(1);
        }
        
        ungetc(c, in);
        if (c != '\n') skip_blank(in);
        return make_fixnum(num);
    }
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
        return make_string(buffer);
    }

    fprintf(stderr, "Unexcepted input `%d'\n", c);
    exit(1);
}

object * eval(object *exp)
{ return exp; }

void output(object *obj)
{
    int c;
    const char *str, *p;
    
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
        
        default:
            fprintf(stderr, "ERROR: Unknown type id %d\n", obj->type);
            exit(1);
    }
}

int main(void)
{
    printf("Bootstrap Scheme %s  CTRL+C to quit.\n", SCHEME_VER);
    
    initialize();

    while (true) {
        printf("> ");
        output(eval(input(stdin)));
        puts("");
    }
    return 0;
}

