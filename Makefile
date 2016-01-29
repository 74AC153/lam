default: lambda_test lam
coverage: lambda.c.gcov

lambda_test: lambda_test.c lambda.c
	gcc -Wall -Wextra -pedantic --std=c11 -g3 -D_POSIX_C_SOURCE=200809L --coverage -o $@ $^
lambda.c.gcov: lambda_test
	./lambda_test
	gcov lambda.gcda
lam: lam.c lambda.c
	gcc -Wall -Wextra -pedantic --std=c11 -g3 -D_POSIX_C_SOURCE=200809L -o $@ $^
clean:
	-rm lambda_test lam *.gcov *.gcno *.gcda
