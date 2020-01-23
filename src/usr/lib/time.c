#include "types.h"
#include "date.h"

/* TODO: When precision is implemented for printf,
         no need for short versions. Can just use "%.3s"
*/

static char* weekdays [ 7 ] = {

	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static char* weekdaysShort [ 7 ] = {

	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
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

static char* monthsShort [ 12 ] = {

	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

char* stringify_weekday ( uint weekday )
{
	if ( ( weekday >= 1 ) && ( weekday <= 7 ) )
	{
		return weekdays[ weekday - 1 ];
	}

	return 0;
}

char* stringify_weekdayShort ( uint weekday )
{
	if ( ( weekday >= 1 ) && ( weekday <= 7 ) )
	{
		return weekdaysShort[ weekday - 1 ];
	}

	return 0;
}

char* stringify_month ( uint month )
{
	if ( ( month >= 1 ) && ( month <= 12 ) )
	{
		return months[ month - 1 ];
	}

	return 0;
}

char* stringify_monthShort ( uint month )
{
	if ( ( month >= 1 ) && ( month <= 12 ) )
	{
		return monthsShort[ month - 1 ];
	}

	return 0;
}
