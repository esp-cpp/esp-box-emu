#pragma once
#include <cstdint>
#include <exception>

#ifndef LIKELY
#define LIKELY(x) __builtin_expect((x), 1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect((x), 0)
#endif

namespace gbc
{
class CPU;
class Machine;
class Memory;
class IO;
constexpr bool ENABLE_GBC = true;

inline void setflag(bool expr, uint8_t& flg, uint8_t mask)
{
    if (expr)
        flg |= mask;
    else
        flg &= ~mask;
}
extern void assert_failed(const int expr, const char* strexpr,
						  const char* filename, const int line);

class MachineException : public std::exception
{
public:
	MachineException(const char* m)
		: message{m} {}

	const char* what() const noexcept override
	{
		return message;
	}
private:
	const char* const message;
};

} // namespace gbc


#define GBC_ASSERT(expr) \
	{ if (!(expr)) { \
		gbc::assert_failed(expr, #expr, __FILE__, __LINE__); \
		__builtin_unreachable(); \
	} }
