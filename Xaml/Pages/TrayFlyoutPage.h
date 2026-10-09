#pragma once
#include "../event.h"
#include "../factory.h"
#include "winrt.hpp"

#include "Pages/TrayFlyoutPage.g.h"

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	struct TrayFlyoutPage : TrayFlyoutPageT<TrayFlyoutPage>
	{
		DECL_EVENT(SettingsRequestedDelegate, SettingsRequested, m_SettingsRequestedDelegate);
		DECL_EVENT(StartupStateChangedDelegate, StartupStateChanged, m_StartupStateChangedDelegate);
		DECL_EVENT(ExitRequestedDelegate, ExitRequested, m_ExitRequestedDelegate);

		void SetStartupState(const wf::IReference<Windows::ApplicationModel::StartupTaskState> &state);

		void SettingsClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void StartupClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ExitClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
	};
}

FACTORY(winrt::TranslucentTB::Xaml::Pages, TrayFlyoutPage);
