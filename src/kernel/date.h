struct rtcdate
{
	uint second;    // 0..59
	uint minute;    // 0..59
	uint hour;      // 0..23
	uint weekday;   // 1..7, Sunday = 1
	uint monthday;  // 1..31
	uint month;     // 1..12
	uint year;      // 2000 + (0..99)
};
