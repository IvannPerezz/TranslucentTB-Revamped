#pragma once
#include "factory.h"
#include "winrt.hpp"

#include "FunctionalConverters.g.h"

namespace winrt::TranslucentTB::Xaml::implementation
{
	struct FunctionalConverters
	{
		static bool InvertedBool(bool value) noexcept;
		static wux::Visibility InvertedBoolToVisibility(bool value) noexcept;
	};
}

FACTORY(winrt::TranslucentTB::Xaml, FunctionalConverters);
