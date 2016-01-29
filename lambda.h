#if ! defined(LAMBDA_H_INCLUDED)
#define LAMBDA_H_INCLUDED

#include <stdio.h>

#define VAR_LEN 8
struct term {
	enum term_type {
		TYPE_VAR,
		TYPE_LAMBDA,
		TYPE_APPL
	} type;

	union {
		char var[VAR_LEN];
		struct { char var[VAR_LEN]; struct term *body; } lambda;
		struct { struct term *fun, *arg; } appl;
	};
};

struct term *make_var(char *name);
struct term *make_lambda(char *var_name, struct term *body);
struct term *make_appl(struct term *fun, struct term *arg);
void destroy_term(struct term *term);
struct term *term_duplicate(struct term *term);

#define PARSE_RES \
	X(OK) \
	X(EXPECTED_TERM) \
	X(UNEXPECTED_RPAREN) \
	X(UNEXPECTED_DOT) \
	X(MISSING_LPAREN_FIRST_TERM) \
	X(MISSING_LPAREN_SECOND_TERM) \
	X(EXPECTED_RPAREN) \
	X(EXPECTED_SYM) \
	X(EXPECTED_DOT) \
	X(MISSING_LAMBDA_BODY) \

enum term_parse_res {
	#define X(y) PARSE_ ## y , 
	PARSE_RES
	#undef X
};

struct term *term_parse(FILE *instream, enum term_parse_res *res);
const char *term_parse_str(enum term_parse_res res);
int term_print(FILE *outstream, struct term *t);

_Bool var_is_free(struct term *t, char *var);

struct term *substitute_free(struct term *t, char *var, struct term *subs);
struct term *alpha_rename(struct term *t, char *subs);
struct term *beta_reduce(struct term *t);
struct term *eta_convert(struct term *t);

_Bool alpha_eq(struct term *t1, struct term *t2);

#endif
