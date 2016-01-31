#include <string.h>
#include <stdlib.h>

#include "lambda.h"

struct termtrace {
	struct termtrace *prev;
	struct term **tref;
};

struct term *termtrace_current(struct termtrace *cursor)
{
	return *(cursor->tref);
}

struct term *termtrace_replace(struct termtrace *cursor, struct term *newterm)
{
	struct term *oldterm = *(cursor->tref);
	*(cursor->tref) = newterm;
	return oldterm;
}

void termtrace_go_prev(struct termtrace **cursor)
{
	struct termtrace *prev = (*cursor)->prev;
	free(*cursor);
	*cursor = prev;
}

void termtrace_go_fun(struct termtrace **cursor)
{
	struct termtrace *next = malloc(sizeof(struct termtrace));
	next->prev = *cursor;
	next->tref = &((*((*cursor)->tref))->appl.fun);
	*cursor = next;
}

void termtrace_go_arg(struct termtrace **cursor)
{
	struct termtrace *next = malloc(sizeof(struct termtrace));
	next->prev = *cursor;
	next->tref = &((*((*cursor)->tref))->appl.arg);
	*cursor = next;
}

void termtrace_go_body(struct termtrace **cursor)
{
	struct termtrace *next = malloc(sizeof(struct termtrace));
	next->prev = *cursor;
	next->tref = &((*((*cursor)->tref))->lambda.body);
	*cursor = next;
}

struct termnode {
	struct termnode *next;
	struct term *t;
	struct termtrace trace_first, *trace;
};

void termstack_push(struct termnode **top, struct term *t)
{
	struct termnode *new_top = malloc(sizeof(struct termnode));
	new_top->next = *top;
	new_top->t = t;
	new_top->trace_first.prev = NULL;
	new_top->trace_first.tref = &new_top->t;
	new_top->trace = &new_top->trace_first;

	*top = new_top;
}

struct term *termstack_get(struct termnode *top, unsigned which)
{
	for( ; top && which; top = top->next)
		which--;
	if(top)
		return termtrace_current(top->trace);
	return NULL;
}

struct term *termstack_pop(struct termnode **top)
{
	struct termnode *next = (*top)->next;

	while((*top)->trace->prev)
		termtrace_go_prev(&(*top)->trace);
	struct term *ret = (*top)->t;
	free(*top);
	*top = next;
	return ret;
}


void termstack_swap(struct termnode **top);
void termstack_roll(struct termnode **top);
void termstack_dup(struct termnode **top);
void termstack_drop(struct termnode **top);


int main(int argc, char *argv[])
{
	if(argc == 1) {
		printf("usage: %s <cmds> ...\n", argv[0]);
		printf("<cmds> is one of:\n");
		printf("<expr> -- push <expr> onto stack\n");

		printf("<expr> <name> -alpha -- replace <expr> with alpha rename of <expr>\n");
		printf("<expr> -beta -- replace <expr> with beta reduce of <expr>\n");
		printf("<expr> -eta -- replace <expr> with eta convert of <expr>\n");

		printf("<body> <var> -lambda -- replace <body> with lambda binding free <var> in <body>\n");
		printf("<fun> <arg> -apply -- replace <fun> with (<fun> <arg>)\n");

		printf("<expr1> <expr2> -alpheq -- push \\x.\\y.x (true) or \\x.\\y.y (false) if <expr1> and <expr2> are alpha equivalent\n");

		printf("-body -- move implicit cursor into body of lambda\n");
		printf("-fun -- move implicit cursor into func of apply\n");
		printf("-arg -- move implicit cursor into arg of apply\n");
		printf("-up -- move implicit cursor up one level\n");
		printf("-top -- move implicit cursor to top level\n");

		printf("<expr1> <expr2> -swap -- swap <expr1> and <expr2>\n");
		printf("<expr> -dup -- duplicate <expr>\n");
		printf("<expr1> <expr2> -repl -- replace <expr1> with <expr2> and remove <expr2>\n");

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
		} else if(strcmp(argv[i], "-alpha") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *var = termstack_get(term_top, 0);
			if(var->type != TYPE_VAR) {
				fprintf(stderr, "alpha requires second arg VAR, got ");
				term_print(stderr, var);
				fputc('\n', stderr);
				break;
			}
			struct term *expr = termstack_get(term_top, 1);
			struct term *renamed = alpha_rename(expr, var->var);
			if(renamed) {
				termstack_push(&term_top, renamed);
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
			struct term *reduced = beta_reduce(term);
			if(reduced) {
				termstack_push(&term_top, reduced);
			} else {
				fprintf(stderr, "beta reduction failed\n");
				break;
			}
		} else if(strcmp(argv[i], "-eta") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *converted = eta_convert(term_top->t);
			if(converted) {
				termstack_push(&term_top, converted);
			} else {
				fprintf(stderr, "eta conversion failed\n");
				break;
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
			}
			struct term *lambda = make_lambda(var->var, body);
			if(lambda) {
				termstack_push(&term_top, lambda);
			} else {
				fprintf(stderr, "create lambda failed\n");
				break;
			}
		} else if(strcmp(argv[i], "-apply") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *arg = termstack_get(term_top, 0);
			struct term *fun = termstack_get(term_top, 1);

			struct term *appl = make_appl(fun, arg);
			if(appl) {
				termstack_push(&term_top, appl);
			} else {
				fprintf(stderr, "create apply failed\n");
				break;
			}
		} else if(strcmp(argv[i], "-alpheq") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *t1_term = termstack_get(term_top, 0);
			struct term *t2_term = termstack_get(term_top, 1);

			if(alpha_eq(t1_term, t2_term)) {
				termstack_push(&term_top, term_duplicate(church_true));
			} else {
				termstack_push(&term_top, term_duplicate(church_false));
			}
		} else if(strcmp(argv[i], "-print") == 0) {
			for(struct termnode *n = term_top; n; n = n->next) {
				term_print(stdout, n->t);
				fprintf(stdout, "\n");
			}
		} else if(strcmp(argv[i], "-body") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			if(termtrace_current(term_top->trace)->type != TYPE_LAMBDA) {
				fprintf(stderr, "not a lambda\n");
				break;
			}
			termtrace_go_body(&term_top->trace);
		} else if(strcmp(argv[i], "-fun") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			if(termtrace_current(term_top->trace)->type != TYPE_APPL) {
				fprintf(stderr, "not an apply\n");
				break;
			}
			termtrace_go_fun(&term_top->trace);
		} else if(strcmp(argv[i], "-arg") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			if(termtrace_current(term_top->trace)->type != TYPE_APPL) {
				fprintf(stderr, "not an apply\n");
				break;
			}
			termtrace_go_arg(&term_top->trace);
		} else if(strcmp(argv[i], "-up") == 0) {
			if(term_top->trace->prev)
				termtrace_go_prev(&term_top->trace);
		} else if(strcmp(argv[i], "-top") == 0) {
			while(term_top->trace->prev)
				termtrace_go_prev(&term_top->trace);
		} else if(strcmp(argv[i], "-swap") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct termnode *next = term_top->next;
			term_top->next = next->next;
			next->next = term_top;
			term_top = next;
		} else if(strcmp(argv[i], "-dup") == 0) {
			if(! termstack_get(term_top, 0)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			termstack_push(&term_top,
			               term_duplicate(termtrace_current(term_top->trace)));
		} else if(strcmp(argv[i], "-repl") == 0) {
			if(! termstack_get(term_top, 1)) {
				fprintf(stderr, "stack underflow\n");
				break;
			}
			struct term *newterm = term_duplicate(termtrace_current(term_top->trace));
			destroy_term(termstack_pop(&term_top));
			destroy_term(termtrace_replace(term_top->trace, newterm));
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
		term_print(stdout, t);
		fprintf(stdout, "\n");
		destroy_term(termstack_pop(&term_top));
	}

	destroy_term(church_true);
	destroy_term(church_false);
	return 0;
}
