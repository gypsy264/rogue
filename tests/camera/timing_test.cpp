#include "handlers/detail/loopTiming.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
	using namespace handler_detail;
	RateCounter rate;
	assert(rate.tick(UINT32_MAX - 500) == 0);
	float measured = 0;
	for (std::uint32_t elapsed = 10; elapsed <= 1000; elapsed += 10)
		measured = rate.tick(UINT32_MAX - 500 + elapsed);
	assert(std::fabs(measured - 100) < 0.1f);
	rate.reset();
	assert(rate.tick(10000) == 0);
	assert(std::fabs(rate.tick(12000) - 0.5f) < 0.01f);
	FrameClock frames;
	assert(frames.due(UINT32_MAX - 10, 33));
	assert(!frames.due(20, 33));
	assert(frames.due(22, 33));
	assert(frames.due(1000, 33));
	assert(!frames.due(1000, 33));
	assert(frames.due(1000, 33, true));
	assert(remainingDelay(100, 104, 10) == 6);
	assert(remainingDelay(100, 120, 10) == 1);
	assert(remainingDelay(UINT32_MAX - 2, 1, 10) == 6);
	puts("camera timing tests passed");
}
