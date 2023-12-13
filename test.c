#include <stdio.h>
#include "strvec.h"



int main(void)
{
	strvec *s = strvec_init_str("123");
	printf("strvec_toi(strvec(\"123\")) == 123 : %d\n", strvec_toi(s) == 123);
	strvec_destroy(s);

	// just seeing what happens !
	s = strvec_init_str("9223372036854775807");
	printf("strvec_toi(LONG_MAX): %d\n", strvec_toi(s) == 123);
	strvec_destroy(s);

	s = strvec_init_str("-123");
	printf("strvec_toi(strvec(\"-123\")) == -123 : %d\n", strvec_toi(s) == -123);
	strvec_destroy(s);

	return 0;
}
