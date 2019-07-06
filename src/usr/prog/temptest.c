// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

struct test
{
	short sh1;
	int   in1;
	uint  in2;
	short sh2;
	char  ch1;
};

int main ( int argc, char* argv [] )
{
	struct test s;

	s.sh1 = 100;
	s.in1 = 432;
	s.in2 = 765;
	s.sh2 = 234;
	s.ch1 = 77;

	printf(

		1,
		"s.sh1 %d\n"
		"s.in1 %d\n"
		"s.in2 %d\n"
		"s.sh2 %d\n"
		"s.ch1 %d\n"
		"\n",

		s.sh1,
		s.in1,
		s.in2,
		s.sh2,
		s.ch1
	);

	exit();
}
