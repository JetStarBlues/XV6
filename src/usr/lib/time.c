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
	"December",
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
	return weekdays[ weekday - 1 ];
}

char* stringify_weekdayShort ( uint weekday )
{
	return weekdaysShort[ weekday - 1 ];
}

char* stringify_month ( uint month )
{
	return months[ month - 1 ];
}

char* stringify_monthShort ( uint month )
{
	return monthsShort[ month - 1 ];
}
