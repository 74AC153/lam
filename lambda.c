#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "lambda.h"

static struct term *alloc_term(enum term_type type)
{
	struct term *ret = malloc(sizeof(struct term));
	ret->type = type;
	return ret;
}
static void dealloc_term(struct term *term)
{
	free(term);
}

struct term *term_duplicate(struct term *term)
{
	struct term *ret = alloc_term(term->type);
	switch(term->type) {
	case TYPE_VAR:
		strncpy(ret->var, term->var, VAR_LEN);
		break;
	case TYPE_LAMBDA:
		strncpy(ret->lambda.var, term->lambda.var, VAR_LEN);
		ret->lambda.body = term_duplicate(term->lambda.body);
		break;
	case TYPE_APPL:
		ret->appl.fun = term_duplicate(term->appl.fun);
		ret->appl.arg = term_duplicate(term->appl.arg);
		break;
	}
	return ret;
}

struct term *make_var(char *name)
{
	struct term *ret = alloc_term(TYPE_VAR);
	strncpy(ret->var, name, VAR_LEN);
	return ret;
}

struct term *make_lambda(char *var, struct term *body)
{
	struct term *ret = alloc_term(TYPE_LAMBDA);
	strncpy(ret->lambda.var, var, VAR_LEN);
	ret->lambda.body = term_duplicate(body);
	return ret;
}

struct term *make_appl(struct term *fun, struct term *arg)
{
	struct term *ret = alloc_term(TYPE_APPL);
	ret->appl.fun = term_duplicate(fun);
	ret->appl.arg = term_duplicate(arg);
	return ret;
}

void destroy_term(struct term *term)
{
	if(! term)
		return;
	switch(term->type) {
	case TYPE_VAR:
		break;
	case TYPE_LAMBDA:
		destroy_term(term->lambda.body);
		break;
	case TYPE_APPL:
		destroy_term(term->appl.fun);
		destroy_term(term->appl.arg);
		break;
	}
	dealloc_term(term);
}

struct lexer {
	FILE *stream;
	enum token_type {
		TOK_NONE,
		TOK_LPAREN,
		TOK_RPAREN,
		TOK_LAMBDA,
		TOK_DOT,
		TOK_SYM
	} type;
	char sym[VAR_LEN];
};

static int lexer_advance(struct lexer *lex)
{
	char ch;
	while(1) {
		if(0 == fread(&ch, 1, 1, lex->stream)) {
			lex->type = TOK_NONE;
			return 0;
		}
	
		if(!isspace(ch))
			break;
	}

	if(ch == '(')
		lex->type = TOK_LPAREN;
	else if(ch == ')')
		lex->type = TOK_RPAREN;
	else if(ch == '\\')
		lex->type = TOK_LAMBDA;
	else if(ch == '.')
		lex->type = TOK_DOT;
	else {
		lex->type = TOK_SYM;

		memset(lex->sym, 0, sizeof(lex->sym));
		lex->sym[0] = ch;
		unsigned i = 1;
		while(1) {
			if(fread(&ch, 1, 1, lex->stream) == 0)
				break;
			if(isspace(ch)) {
				ungetc(ch, lex->stream);
				break;
			}
			if(strchr("().\\", ch)) {
				ungetc(ch, lex->stream);
				break;
			}
			if(i < VAR_LEN)
				lex->sym[i] = ch;
			i++;
		}
	}

	return 1;
}

static struct term *_term_parse(struct lexer *lex, enum term_parse_res *res);

static struct term *_lparen_parse(struct lexer *lex, enum term_parse_res *res)
{
	struct term *term1 = NULL;
	struct term *term2 = NULL;
	struct term *ret = NULL;

	term1 = _term_parse(lex, res);
	if(! term1) {
		goto done;
	}

	if(lex->type != TOK_RPAREN) {
		term2 = _term_parse(lex, res);
		if(! term2) {
			goto done;
		}

		if(lex->type != TOK_RPAREN) {
			*res = PARSE_EXPECTED_RPAREN;
			goto done;
		}
		lexer_advance(lex);

		ret = make_appl(term1, term2);
	} else {
		lexer_advance(lex);
		ret = term1;
		term1 = NULL;
	}

done:
	destroy_term(term1);
	destroy_term(term2);
	return ret;
}

static struct term *_lambda_parse(struct lexer *lex, enum term_parse_res *res)
{
	struct term *body = NULL;
	struct term *ret = NULL;

	if(lex->type != TOK_SYM) {
		*res = PARSE_EXPECTED_SYM;
		goto done;
	} else {
		char var[VAR_LEN];
		strncpy(var, lex->sym, VAR_LEN);
		lexer_advance(lex);

		if(lex->type != TOK_DOT) {
			*res = PARSE_EXPECTED_DOT;
			goto done;
		}
		lexer_advance(lex);
		
		body = _term_parse(lex, res);
		if(! body) {
			goto done;
		}

		ret = make_lambda(var, body);
	}
done:
	destroy_term(body);
	return ret;
}

static struct term *_term_parse(struct lexer *lex, enum term_parse_res *res)
{
	struct term *ret = NULL;
	switch(lex->type) {
		case TOK_NONE:
			*res = PARSE_EXPECTED_TERM;
			break;
		case TOK_LPAREN:
			lexer_advance(lex);
			ret = _lparen_parse(lex, res);
			break;
		case TOK_RPAREN:
			*res = PARSE_UNEXPECTED_RPAREN;
			break;
		case TOK_LAMBDA:
			lexer_advance(lex);
			ret = _lambda_parse(lex, res);
			break;
		case TOK_DOT:
			*res = PARSE_UNEXPECTED_DOT;
			break;
		case TOK_SYM:
			ret = make_var(lex->sym);
			lexer_advance(lex);
			break;
	}
	return ret;
}

struct term *term_parse(FILE *instream, enum term_parse_res *res)
{
	struct lexer lex = { .stream = instream, .type = TOK_NONE };
	*res = PARSE_OK;
	lexer_advance(&lex);
	return _term_parse(&lex, res);
}

const char *term_parse_str(enum term_parse_res res)
{
	const char *strings[] = {
		#define X(y) #y, 
		PARSE_RES
		#undef X
	};
	return strings[res];
}

int term_print(FILE *outstream, struct term *t)
{
	if(!t)
		return -1;
	switch(t->type) {
	case TYPE_VAR:
		if(0 <= fprintf(outstream, "%.*s", VAR_LEN, t->var))
			break;
		return -1;
	case TYPE_LAMBDA:
		if(0 <= fprintf(outstream, "\\%.*s", VAR_LEN, t->lambda.var))
			if(0 <= fprintf(outstream, "."))
				if(0 <= term_print(outstream, t->lambda.body))
					break;
		return -1;
	case TYPE_APPL:
		if(0 <= fprintf(outstream, "("))
			if(0 <= term_print(outstream, t->appl.fun))
				if(0 <= fprintf(outstream, " "))
					if(0 <= term_print(outstream, t->appl.arg))
						if(0 <= fprintf(outstream, ")"))
							break;
		return -1;
	}

	return 0;
}

_Bool var_is_free(struct term *t, char *var)
{
	_Bool is_free = 1;

	switch(t->type) {
	case TYPE_VAR:
		if(strncmp(t->var, var, VAR_LEN) != 0)
			is_free = 0;
		break;
	case TYPE_LAMBDA:
		if(strncmp(t->lambda.var, var, VAR_LEN) == 0)
			is_free = 0;
		else
			is_free = var_is_free(t->lambda.body, var);
		break;
	case TYPE_APPL:
		is_free = var_is_free(t->appl.fun, var);
		if(! is_free)
			is_free = var_is_free(t->appl.arg, var);
		break;
	}

	return is_free;
}

struct subs_bt {
	struct subs_bt *prev;
	char *binding;
};

static struct term *_rec_subs_free(
	struct subs_bt *prev,
	struct term *t, char *var, struct term *subs)
{
	struct term *ret = NULL;
	if(t->type == TYPE_LAMBDA) {
		if(var_is_free(subs, t->var)) {
			// stop substitution if anything in 'subs' is captured by lambda
			// need to do alpha rename on this lambda before we can substitute
			// i.e. need to do a capture-avoiding substitution
			ret = NULL;
		} else if(strncmp(t->lambda.var, var, VAR_LEN) == 0) {
			// stop substitution if 'var' becomes bound
			ret = term_duplicate(t);
		} else {
			// introduce new binding level
			struct subs_bt bt = { .prev = prev, .binding = t->var };
			struct term *subs_body = _rec_subs_free(&bt, t->lambda.body, var, subs);
			ret = make_lambda(t->var, subs_body);
			destroy_term(subs_body);
		}
	} else if(t->type == TYPE_APPL) {
		struct term *subs_fun = _rec_subs_free(prev, t->appl.fun, var, subs);
		struct term *subs_arg = _rec_subs_free(prev, t->appl.arg, var, subs);
		ret = make_appl(subs_fun, subs_arg);
		destroy_term(subs_fun);
		destroy_term(subs_arg);
	} else /* if(t->type == TYPE_VAR) */ {
		// check to see if var is already bound
		int bound = 0;
		for(struct subs_bt *cursor = prev; cursor; cursor = cursor->prev) {
			if(strncmp(cursor->binding, t->var, VAR_LEN) == 0) {
				bound = 1;
				break;
			}
		}
		if(bound) {
			// bound: do nothing
			ret = make_var(t->var);
		} else if(strncmp(t->var, var, VAR_LEN) != 0) {
			// not maching var: do nothing
			ret = make_var(t->var);
		} else /* strncmp(t->var, var, VAR_LEN) == 0) */ {
			// match: do the substitution
			ret = term_duplicate(subs);
		}
	}
	return ret;
}

struct term *substitute_free(struct term *t, char *var, struct term *subs)
{
	return _rec_subs_free(NULL, t, var, subs);
}

// \x.Y [x := subs] --> \z.Y/[x:=subs]
struct term *alpha_rename(struct term *t, char *subs)
{
	struct term *ret = NULL;
	if(t->type == TYPE_LAMBDA) {
		struct term *subs_term = make_var(subs);
		struct term *body_rename =
			substitute_free(t->lambda.body, t->lambda.var, subs_term);
		ret = make_lambda(subs, body_rename);
		destroy_term(subs_term);
		destroy_term(body_rename);
	}
	return ret;
}

// (\x.Y) z --> Y/[x := z]
struct term *beta_reduce(struct term *t)
{
	struct term *ret = NULL;
	if(t->type == TYPE_APPL)
		if(t->appl.fun->type == TYPE_LAMBDA)
			ret = substitute_free(
				t->appl.fun->lambda.body,
				t->appl.fun->lambda.var,
				t->appl.arg);
	return ret;
}

// \x.(Y x) --> Y
// x must be bound in Y
struct term *eta_convert(struct term *t)
{
	struct term *ret = NULL;
	if(t->type == TYPE_LAMBDA) {
		struct term *body = t->lambda.body;
		if(body->type == TYPE_APPL) {
			struct term *fun = body->appl.fun;
			struct term *arg = body->appl.arg;
			if(! var_is_free(fun, t->lambda.var)) {
				if(arg->type == TYPE_VAR)
					if(strncmp(arg->var, t->lambda.var, VAR_LEN) == 0)
						ret = term_duplicate(t->lambda.body->appl.fun);
			}
		}
	}
	return ret;
}

_Bool alpha_eq(struct term *t1, struct term *t2)
{
	if(t1->type != t2->type)
		return 0;

	_Bool ret = 0;
	switch(t1->type) {
	case TYPE_VAR:
		if(strncmp(t1->var, t2->var, VAR_LEN) == 0)
			ret = 1;
		break;
	case TYPE_LAMBDA:
		if(strncmp(t1->lambda.var, t2->lambda.var, VAR_LEN) == 0)
			ret = alpha_eq(t1->lambda.body, t2->lambda.body);
		else {
			struct term *renamed = alpha_rename(t2, t1->lambda.var);
			ret = alpha_eq(t1->lambda.body, renamed->lambda.body);
			destroy_term(renamed);
		}
		break;
	case TYPE_APPL:
		ret = alpha_eq(t1->appl.fun, t2->appl.fun);
		if(ret)
			ret = alpha_eq(t2->appl.arg, t2->appl.arg);
		break;
	}

	return ret;
}
