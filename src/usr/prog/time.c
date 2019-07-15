// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"
#include "date.h"

int main ( int argc, char* argv [] )
{
	struct rtcdate d;

	gettime( &d );

	printf( 1,

		"The time is:\n"
		"  second - %d\n"
		"  minute - %d\n"
		"  hour   - %d\n"
		"  day    - %d\n"
		"  month  - %d\n"
		"  year   - %d\n",

		d.second,
		d.minute,
		d.hour,
		d.day,
		d.month,
		d.year
	);

	exit();
}
