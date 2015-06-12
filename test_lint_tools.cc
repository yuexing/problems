#include <stdlib.h>

static int nondet;

void exp_func()
{
	throw 0;
}

void resource_leak_fn()
{
	int* p = (int*)malloc(sizeof(int)); 
	exp_func();							// exception path
	free(p);
}

void null_deref_fn_help(int *p)
{
	if(nondet) {
		*p = 0;							// p is not written on any path
	} else {

	}
}

void null_deref_fn()
{
	int *p = 0;
	null_deref_fn_help(p);
}

void null_deref_fp()
{
	int *p = 0;
	for(int i = 0; i < 10; i+=3) {
		if(i == 5) {
			*p = 0;					   // should be pruned
		}
	}
}

int main()
{
	null_deref_fn();
	
	return 0;
}