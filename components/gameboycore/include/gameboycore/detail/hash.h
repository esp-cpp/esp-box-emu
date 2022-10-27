#ifndef GAMEBOYCORE_DETAIL_HASH
#define GAMEBOYCORE_DETAIL_HASH

#include <vector>
#include <functional>

namespace gb
{
	namespace detail
	{
		// boost::hash_combine
		template<class T>
		inline void hash_combine(std::size_t& seed, T const& v)
		{
			seed ^= std::hash<T>{}(v) + 0x9E3779B9 + (seed << 6) + (seed >> 2);
		}

		/**
			Vector Hash
		*/
		template<class T>
		struct ContainerHash
		{
			using argument_type = T;
			using result_type = std::size_t;

			result_type operator()(argument_type const& in) const
			{
				std::size_t seed = 0;
				for (const auto& i : in)
					hash_combine(seed, i);

				return seed;
			}
		};
	}
}

#endif 
