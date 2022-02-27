module;
#pragma warning (push)
#pragma warning (disable: 4819) // Non-ANSI characters
#include "Eigen/Core"
#pragma warning (pop)

#include <fmt/format.h>
#include <string>
export module Physics:Types;

void A() {
	//fmt::format("");
	std::string s;
}

namespace Physics {
	using FloatType = float;
	using Vector3 = Eigen::Vector3<FloatType>;
}