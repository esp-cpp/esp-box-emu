#include "rclassicFixedSharedState.h"

namespace TFE_Jedi
{
#ifdef TFE_ESPBOX
	RClassicFixedState* s_rcfStatePtr = nullptr;
	void rcf_setStatePtr(RClassicFixedState* state)
	{
		s_rcfStatePtr = state;
	}
#else
	RClassicFixedState s_rcfState = { 0 };
#endif
}  // TFE_Jedi