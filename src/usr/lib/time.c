#include "types.h"
#include "date.h"

static char* weekdays [ 7 ] = {

	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static char* months [ 12 ] = {

	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

char* stringify_weekday ( uint weekday )
{
	return weekdays[ weekday - 1 ];
}

char* stringify_month ( uint month )
{
	return months[ month - 1 ];
}
