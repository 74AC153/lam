#include <string.h>
#include <stdlib.h>

#include "lambda.h"

struct termnode {
	struct termnode *next;
	struct term *t;
};

void termstack_push(struct termnode **top, struct term *t)
{
	struct termnode *new_top = malloc(sizeof(struct termnode));
	new_top->next = *top;
	new_top->t = t;
	*top = new_top;
}

struct term *termstack_get(struct termnode *top, unsigned which)
{
	for( ; top && which; top = top->next)
		which--;
	if(top)
		return top->t;
	return NULL;
}

void termstack_pop(struct termnode **top)
{
	struct termnode *next = (*top)->next;
	free(*top);
	*top = next;
}

int main(int argc, char *argv[])
{
	if(argc == 1) {
		printf("usage: %s <cmds> ...\n", argv[0]);
		printf("<cmds> is one of:\n");
		printf("<expr> -- push <expr> onto stack\n");
		printf("<expr> <name> <repl> -subs -- replace free occurrences of <name> with <repl> in <expr>\n");
		printf("<expr> <name> -alpha -- alpha rename <expr>\n");
		printf("<expr> -beta -- beta reduce <expr>\n");
		printf("<expr> -eta -- eta convert <expr>\n");
		printf("<body> <var> -lambda -- make a lambda binding free <var> in <body>\n");
		printf("<fun> <arg> -apply -- make an apply term with terms at top of stack\n");
		printf("<expr1> <expr2> -alpheq -- push \\x.\\y.x (true) or \\x.\\y.y (false) if <expr1> and <expr2> are alpha equivalent\n");
		printf("-print -- print expression stack\n");
		return 0;
	}
	struct termnode *term_top = NULL;

	struct term *church_true, *church_false;
	{
		enum term_parse_res res;
		char *expr = "\\a.\\b.a";
		FILE *stream = fmemopen(expr, strlen(expr), "r");
		church_true = term_parse(stream, &res);
		fclose(stream);
		expr = "\\a.\\b.b";
		stream = fmemopen(expr, strlen(expr), "r");
		church_false = term_parse(stream, &res);
		fclose(stream);
	}
	
	for(int i = 1; i < argc; i++) {
		if(0) {
		} else if(strcmp(argv[i], "-subs") == 0) {
			if(! termstack_get(term_top, 2)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *repl = termstack_get(term_top, 0);
			struct term *var = termstack_get(term_top, 1);
			struct term *expr = termstack_get(term_top, 2);

			struct term *subs = substitute_free(expr, var->var, repl);
			if(subs) {
				destroy_term(repl);
				destroy_term(var);
				destroy_term(expr);
				termstack_pop(&term_top);
				termstack_pop(&term_top);
				term_top->t = subs;
			} else {
				fprintf(stderr, "substitute free failed\n");
				break;
			}
		} else if(strcmp(argv[i], "-alpha") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *var = termstack_get(term_top, 0);
			struct term *expr = termstack_get(term_top, 1);

			struct term *rename = alpha_rename(expr, var->var);
			if(rename) {
				destroy_term(var);
				destroy_term(expr);
				termstack_pop(&term_top);
				term_top->t = rename;
			} else {
				fprintf(stderr, "alpha rename failed\n");
				break;
			}
		} else if(strcmp(argv[i], "-beta") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *term = termstack_get(term_top, 0);
			struct term *newterm = beta_reduce(term);
			if(newterm) {
				destroy_term(term_top->t);
				term_top->t = newterm;
			} else {
				fprintf(stderr, "beta reduction not allowed\n");
				break;
			}
		} else if(strcmp(argv[i], "-eta") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *newterm = eta_convert(term_top->t);
			if(! newterm) {
				fprintf(stderr, "eta conversion not allowed\n");
			} else {
				destroy_term(term_top->t);
				term_top->t = newterm;
			}
		} else if(strcmp(argv[i], "-lambda") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *var = termstack_get(term_top, 0);
			struct term *body = termstack_get(term_top, 1);

			if(var->type != TYPE_VAR) {
				fprintf(stderr, "lambda requires VAR, got ");
				term_print(stderr, var);
				fputc('\n', stderr);
				break;
			} else {
				struct term *lambda = make_lambda(var->var, body);
				termstack_pop(&term_top);
				term_top->t = lambda;
				destroy_term(var);
				destroy_term(body);
			}
		} else if(strcmp(argv[i], "-apply") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *arg = termstack_get(term_top, 0);
			struct term *fun = termstack_get(term_top, 1);

			struct term *appl = make_appl(fun, arg);
			termstack_pop(&term_top);
			term_top->t = appl;
			destroy_term(arg);
			destroy_term(fun);
		} else if(strcmp(argv[i], "-alpheq") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *t1_term = termstack_get(term_top, 0);
			struct term *t2_term = termstack_get(term_top, 1);

			termstack_pop(&term_top);
			if(alpha_eq(t1_term, t2_term)) {
				term_top->t = church_true;
			} else {
				term_top->t = church_false;
			}
			destroy_term(t1_term);
			destroy_term(t2_term);
		} else if(strcmp(argv[i], "-print") == 0) {
			for(struct termnode *n = term_top; n; n = n->next) {
				term_print(stdout, n->t);
				fprintf(stdout, "\n");
			}
		} else {
			FILE *stream = fmemopen(argv[i], strlen(argv[i]), "r");
			if(! stream)
				continue;

			enum term_parse_res res;
			struct term *term = term_parse(stream, &res);
			fclose(stream);

			if(term == NULL) {
				long loc = ftell(stream);
				if(! feof(stream))
					loc--;
				printf("parse error: %s at char %lu\n", term_parse_str(res), loc+1);
				printf("%s\n", argv[i]);
				for(long i = 0; i < loc; i++)
					printf(" ");
				printf("^\n");
				break;
			}
			termstack_push(&term_top, term);
		}
	}

	struct term *t;
	while(NULL != (t = termstack_get(term_top, 0))) {
		termstack_pop(&term_top);
		destroy_term(t);
	}
	return 0;
}
