#include "pch.h"

#include "TrayFlyoutPage.h"
#if __has_include("Pages/TrayFlyoutPage.g.cpp")
#include "Pages/TrayFlyoutPage.g.cpp"
#endif

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	void TrayFlyoutPage::SetStartupState(const wf::IReference<Windows::ApplicationModel::StartupTaskState> &state)
	{
		const auto startup = StartupState();
		if (state)
		{
			const auto stateUnbox = state.Value();

			using enum Windows::ApplicationModel::StartupTaskState;
			startup.IsChecked(stateUnbox == Enabled || stateUnbox == EnabledByPolicy);
			startup.IsEnabled(stateUnbox == Disabled || stateUnbox == DisabledByUser || stateUnbox == Enabled);
		}
		else
		{
			startup.IsChecked(false);
			startup.IsEnabled(false);
		}
	}

	void TrayFlyoutPage::SettingsClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_SettingsRequestedDelegate();
	}

	void TrayFlyoutPage::StartupClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_StartupStateChangedDelegate();
	}

	void TrayFlyoutPage::ExitClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_ExitRequestedDelegate();
	}
}
