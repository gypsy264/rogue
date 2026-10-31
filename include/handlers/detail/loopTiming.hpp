#pragma once

#include <cstdint>

namespace handler_detail {
inline std::uint32_t remainingDelay(std::uint32_t start, std::uint32_t now, std::uint32_t period) {
	const auto elapsed = now - start;
	return elapsed < period ? period - elapsed : 1;
}

class FrameClock {
public:
	bool due(std::uint32_t now, std::uint32_t period, bool full = false) {
		if (full || !started) { last = now; started = true; return true; }
		const auto elapsed = now - last;
		if (elapsed < period) return false;
		last += (elapsed / period) * period;
		return true;
	}

private:
	std::uint32_t last = 0;
	bool started = false;
};

class RateCounter {
public:
	void reset() { started = false; count = 0; rate = 0; }
	float tick(std::uint32_t now) {
		if (!started) { since = now; started = true; return 0; }
		++count;
		const auto elapsed = now - since;
		if (elapsed >= 1000) {
			rate = count * 1000.0f / elapsed;
			count = 0;
			since = now;
		}
		return rate;
	}

private:
	std::uint32_t since = 0, count = 0;
	float rate = 0;
	bool started = false;
};
}
