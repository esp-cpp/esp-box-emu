#ifndef GAMEBOYCORE_TIME_H
#define GAMEBOYCORE_TIME_H

#include <cstdint>
#include <functional>

namespace gb
{
	struct Time
	{
		Time(uint8_t s, uint8_t m, uint8_t h, uint16_t d)
			: seconds{ s }
			, minutes{ m }
			, hours{ h }
			, days{ d }
		{
		}

		Time() : Time{0,0,0,0}
		{
		}

		uint8_t seconds;
		uint8_t minutes;
		uint8_t hours;
		uint16_t days;
	};

	using TimeProvider = std::function<Time()>;
}

#endif // GAMEBOYCORE_TIME_H
