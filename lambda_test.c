#include <string.h>
#include <stdlib.h>
#include "lambda.h"

#define ARRLEN(X) (sizeof(X) / sizeof((X)[0]))


struct test_make_print_desstoy_var_vec {
	char *name;
	char *expect;
};

void test_make_print_destroy_var(void)
{
	struct test_make_print_desstoy_var_vec tvecs[] = {
		{ .name = "x", .expect = "x" },
		{ .name = "xx", .expect = "xx" },
		{ .name = "xxxxxxxx", .expect = "xxxxxxxx" },
		{ .name = "xxxxxxxxX", .expect = "xxxxxxxx" },
	};

	for(unsigned i = 0; i < ARRLEN(tvecs); i++) {
		struct term *output = make_var(tvecs[i].name);
		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(outbuf, tvecs[i].expect) != 0)
			printf("%s FAIL %u: %s -> %s (expected %s)\n", __func__, i,
			       tvecs[i].name, outbuf, tvecs[i].expect);

		destroy_term(output);
		free(outbuf);
	}
}

struct test_make_print_desstoy_lambda_vec {
	char *name;
	char *expect;
};

void test_make_print_destroy_lambda(void)
{
	struct test_make_print_desstoy_lambda_vec tvecs[] = {
		{ .name = "x", .expect="\\x.x" },
		{ .name = "xxxxxxxx", .expect="\\xxxxxxxx.x" },
		{ .name = "xxxxxxxxX", .expect="\\xxxxxxxx.x" },
	};

	struct term *body = make_var("x");

	for(unsigned i = 0; i < ARRLEN(tvecs); i++) {

		struct term *output = make_lambda(tvecs[i].name, body);
		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(outbuf, tvecs[i].expect) != 0)
			printf("%s FAIL %u: %s -> %s (expected %s)\n", __func__, i,
			       tvecs[i].name, outbuf, tvecs[i].expect);

		destroy_term(output);
		free(outbuf);
	}
	destroy_term(body);
}

void test_make_print_destroy_appl(void)
{
	char *outbuf = NULL;
	size_t outlen = 0;
	struct term *fun = make_var("x");
	struct term *arg = make_var("y");

	struct term *output = make_appl(fun, arg);
	FILE *outstream = open_memstream(&outbuf, &outlen);
	term_print(outstream, output);
	fclose(outstream);

	if(strcmp(outbuf, "(x y)") != 0)
		printf("%s FAIL: %s (expected %s)\n", __func__,
		       outbuf, "(x x)");

	destroy_term(output);
	destroy_term(fun);
	destroy_term(arg);
	free(outbuf);
}

struct dup_test_vec {
	struct term *t;
	char *expect;
};

void test_dup(void)
{
	struct term *var = make_var("x");

	struct dup_test_vec tvecs[] = {
		{ .t = var, .expect = "x" },
		{ .t = make_lambda("x", var), .expect = "\\x.x" },
		{ .t = make_appl(var, var), .expect = "(x x)" },
	};
	
	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);
	for(unsigned i = start; i < limit; i++) {
		struct term *dupl = term_duplicate(tvecs[i].t);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, dupl);
		fclose(outstream);

		if(strcmp(outbuf, tvecs[i].expect) != 0)
			printf("%s FAIL %u: %s (expected %s)\n", __func__, i,
			       outbuf, "x");
		
		destroy_term(dupl);
		free(outbuf);
	}

	for(unsigned i = 0; i < ARRLEN(tvecs); i++) {
		destroy_term(tvecs[i].t);
	}

}

struct parse_test_vec {
	char *input;
	char *expect;
};

void test_parse(void)
{
	struct parse_test_vec tvecs[] = {
		{ .input = "x", .expect = "x" },
		{ .input = "xx", .expect = "xx" },
		{ .input = "hello", .expect = "hello" },
		{ .input = "(x)", .expect = "x" },

		{ .input = "\\", .expect = "" },
		{ .input = "\\.", .expect = "" },
		{ .input = "\\x", .expect = "" },
		{ .input = "\\x x", .expect = "" },
		{ .input = "\\x.", .expect = "" },
		{ .input = "\\x.x", .expect = "\\x.x" },

		{ .input = "(", .expect = "" },
		{ .input = ")", .expect = "" },
		{ .input = "()", .expect = "" },
		{ .input = "(x", .expect = "" },
		{ .input = "(x ", .expect = "" },
		{ .input = "(x y", .expect = "" },
		{ .input = "(.", .expect = "" },
		{ .input = "(x .", .expect = "" },
		{ .input = "(x y)", .expect = "(x y)" },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);
	for(unsigned i = start; i < limit; i++) {

		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *output = term_parse(instream, &res);
		fclose(instream);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(tvecs[i].expect, outbuf) != 0) {
			printf("%s FAIL %u: %s -> %s (expect %s) [%s]\n", __func__, i,
			       tvecs[i].input,
			       outbuf,
			       tvecs[i].expect,
			       term_parse_str(res));
		}
		destroy_term(output);
		free(outbuf);
	}
}

struct var_is_free_test_vec {
	char *input;
	char *varname;
	_Bool expect;
};

void test_var_is_free(void)
{
	struct var_is_free_test_vec tvecs[] = {
		{ .input = "x", .varname = "x", .expect = 1 },
		{ .input = "\\x.y", .varname = "x", .expect = 0 },
		{ .input = "\\y.x", .varname = "x", .expect = 1 },
		{ .input = "\\y.\\x.x", .varname = "x", .expect = 0 },
		{ .input = "(\\x.y a)", .varname = "x", .expect = 0 },
		{ .input = "(\\x.y x)", .varname = "x", .expect = 1 },
		{ .input = "(x \\x.y)", .varname = "x", .expect = 1 },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);
	for(unsigned i = start; i < limit; i++) {
		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *output = term_parse(instream, &res);
		fclose(instream);

		_Bool result = var_is_free(output, tvecs[i].varname);

		if(result != tvecs[i].expect) {
			printf("%s FAIL %u: %s free in %s -> %s (expect %s)\n", __func__, i,
			       tvecs[i].varname,
			       tvecs[i].input,
			       result ? "yes" : "no",
			       tvecs[i].expect ? "yes" : "no");
		}
		destroy_term(output);
	}
}

struct subs_free_test_vec {
	char *input;
	char *varname;
	char *subs;
	char *expect;
};

void test_subs_free(void)
{
	struct subs_free_test_vec tvecs[] = {
		{ .input = "x", .varname="x", .subs="y", .expect="y" },
		{ .input = "z", .varname="x", .subs="y", .expect="z" },
		{ .input = "\\y.x", .varname="x", .subs="z", .expect="\\y.z" },
		{ .input = "\\y.\\z.(z x))", .varname="x", .subs="p", .expect="\\y.\\z.(z p)" },
		{ .input = "\\y.(x y)", .varname="x", .subs="z", .expect="\\y.(z y)" },
		{ .input = "\\x.x", .varname="x", .subs="z", .expect="\\x.x" },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);
	for(unsigned i = start; i < limit; i++) {
		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *input_term = term_parse(instream, &res);
		fclose(instream);

		instream = fmemopen(tvecs[i].subs, strlen(tvecs[i].subs), "r");
		struct term *subs_term = term_parse(instream, &res);
		fclose(instream);

		struct term *output = substitute_free(input_term, tvecs[i].varname, subs_term);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(tvecs[i].expect, outbuf) != 0) {
			printf("%s FAIL %u: %s [%s := %s] -> %s (expect %s)\n", __func__, i,
			       tvecs[i].input,
			       tvecs[i].varname,
			       tvecs[i].subs,
			       outbuf,
			       tvecs[i].expect);
		}

		destroy_term(input_term);
		destroy_term(subs_term);
		destroy_term(output);
		free(outbuf);
	}

}


struct alpha_rename_test_vec {
	char *input;
	char *subs;
	char *expect;
};

void test_alpha_rename(void)
{
	struct alpha_rename_test_vec tvecs[] = {
		{ "\\x.x", "y", "\\y.y" },
		{ "\\x.z", "y", "\\y.z" },
		{ "\\x.(x z)", "y", "\\y.(y z)" },
		{ "\\x.\\z.(x z)", "y", "\\y.\\z.(y z)" },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);

	for(unsigned i = start; i < limit; i++) {
		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *input_term = term_parse(instream, &res);
		fclose(instream);

		struct term *output = alpha_rename(input_term, tvecs[i].subs);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(tvecs[i].expect, outbuf) != 0) {
			printf("%s FAIL %u: %s [alpha %s] -> %s (expect %s)\n", __func__, i,
			       tvecs[i].input,
			       tvecs[i].subs,
			       outbuf,
			       tvecs[i].expect);
		}
		destroy_term(input_term);
		destroy_term(output);
		free(outbuf);
	}
}

struct beta_reduce_test_vec {
	char *input;
	char *expect;
};

void test_beta_reduce(void)
{
	struct beta_reduce_test_vec tvecs[] = {
		{ .input = "(\\x.x x)", .expect = "x" },
		{ .input = "(\\x.x \\x.x)", .expect = "\\x.x" },
		{ .input = "x", .expect = "" },
		{ .input = "\\x.x", .expect = "" },
		{ .input = "(x x)", .expect = "" },
		{ .input = "((x x) x)", .expect = "" },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);

	for(unsigned i = start; i < limit; i++) {
		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *input_term = term_parse(instream, &res);
		fclose(instream);

		struct term *output = beta_reduce(input_term);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(tvecs[i].expect, outbuf) != 0) {
			printf("%s FAIL %u: %s [beta] -> %s (expect %s)\n", __func__, i,
			       tvecs[i].input,
			       outbuf,
			       tvecs[i].expect);
		}
		destroy_term(input_term);
		destroy_term(output);
		free(outbuf);
	}
}

struct eta_convert_test_vec {
	char *input;
	char *expect;
};

void test_eta_convert(void)
{
	struct eta_convert_test_vec tvecs[] = {
		{ .input = "(\\x.(y x))", .expect = "y" },
		{ .input = "(\\x.(x x))", .expect = "" },
		{ .input = "x", .expect = "" },
		{ .input = "\\x.x", .expect = "" },
	};


	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);

	for(unsigned i = start; i < limit; i++) {
		FILE *instream = fmemopen(tvecs[i].input, strlen(tvecs[i].input), "r");
		enum term_parse_res res;
		struct term *input_term = term_parse(instream, &res);
		fclose(instream);

		struct term *output = eta_convert(input_term);

		char *outbuf = NULL;
		size_t outlen = 0;
		FILE *outstream = open_memstream(&outbuf, &outlen);
		term_print(outstream, output);
		fclose(outstream);

		if(strcmp(tvecs[i].expect, outbuf) != 0) {
			printf("%s FAIL %u: %s [eta] -> %s (expect %s)\n", __func__, i,
			       tvecs[i].input,
			       outbuf,
			       tvecs[i].expect);
		}
		destroy_term(input_term);
		destroy_term(output);
		free(outbuf);
	}
}

struct alpha_eq_test_vec {
	char *t1;
	char *t2;
	_Bool expect;
};

void test_alpha_eq(void)
{
	struct alpha_eq_test_vec tvecs[] = {
		{ .t1 = "x", .t2 = "x", .expect = 1 },
		{ .t1 = "x", .t2 = "y", .expect = 0 },
		{ .t1 = "x", .t2 = "(x y)", .expect = 0 },
		{ .t1 = "\\x.x", .t2 = "\\x.x", .expect = 1 },
		{ .t1 = "\\x.x", .t2 = "\\y.y", .expect = 1 },
		{ .t1 = "\\x.x", .t2 = "\\x.y", .expect = 0 },
		{ .t1 = "(y \\x.x)", .t2 = "(y \\y.y)", .expect = 1 },
		{ .t1 = "(\\x.x y)", .t2 = "(\\y.y y)", .expect = 1 },
	};

	unsigned start = 0;
	unsigned limit = ARRLEN(tvecs);

	for(unsigned i = start; i < limit; i++) {
		enum term_parse_res res;
		FILE *instream = fmemopen(tvecs[i].t1, strlen(tvecs[i].t1), "r");
		struct term *t1_term = term_parse(instream, &res);
		fclose(instream);

		instream = fmemopen(tvecs[i].t2, strlen(tvecs[i].t2), "r");
		struct term *t2_term = term_parse(instream, &res);
		fclose(instream);

		_Bool output = alpha_eq(t1_term, t2_term);

		if(output != tvecs[i].expect) {
			printf("%s FAIL %u: %s alpheq %s -> %s (expect %s)\n", __func__, i,
			       tvecs[i].t1,
			       tvecs[i].t2,
			       output ? "yes" : "no",
			       tvecs[i].expect ? "yes" : "no");
		}
		destroy_term(t1_term);
		destroy_term(t2_term);
	}
}

int main(void)
{
	test_make_print_destroy_var();
	test_make_print_destroy_lambda();
	test_make_print_destroy_appl();
	test_dup();
	test_parse();
	test_var_is_free();
	test_subs_free();
	test_alpha_rename();
	test_beta_reduce();
	test_eta_convert();
	test_alpha_eq();

	return 0;
}
