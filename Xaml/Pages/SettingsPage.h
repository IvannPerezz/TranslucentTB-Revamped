#pragma once
#include <array>
#include <cstddef>
#include "../event.h"
#include "../factory.h"
#include "winrt.hpp"

#include "FramelessPage.h"
#include "Models/Primitives/TaskbarAppearance.h"
#include "Pages/SettingsPage.g.h"

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	struct SettingsPage : wux::Markup::ComponentConnectorT<SettingsPageT<SettingsPage>>
	{
		SettingsPage();
		void InitializeComponent();

		wf::Rect ExpandedDragRegion() override;

		bool SystemHasBattery() noexcept
		{
			return m_SystemHasBattery;
		}

		DECL_EVENT(TaskbarSettingsChangedDelegate, TaskbarSettingsChanged, m_TaskbarSettingsChangedDelegate);
		DECL_EVENT(ColorRequestedDelegate, ColorRequested, m_ColorRequestedDelegate);

		DECL_EVENT(MinimizeRequestedDelegate, MinimizeRequested, m_MinimizeRequestedDelegate);
		DECL_EVENT(MaximizeRequestedDelegate, MaximizeRequested, m_MaximizeRequestedDelegate);

		DECL_EVENT(OpenLogFileRequestedDelegate, OpenLogFileRequested, m_OpenLogFileRequestedDelegate);
		DECL_EVENT(LogLevelChangedDelegate, LogLevelChanged, m_LogLevelChangedDelegate);
		DECL_EVENT(DumpDynamicStateRequestedDelegate, DumpDynamicStateRequested, m_DumpDynamicStateRequestedDelegate);
		DECL_EVENT(ResetDynamicStateRequestedDelegate, ResetDynamicStateRequested, m_ResetDynamicStateRequestedDelegate);
		DECL_EVENT(CompactThunkHeapRequestedDelegate, CompactThunkHeapRequested, m_CompactThunkHeapRequestedDelegate);
		DECL_EVENT(EditSettingsRequestedDelegate, EditSettingsRequested, m_EditSettingsRequestedDelegate);
		DECL_EVENT(ResetSettingsRequestedDelegate, ResetSettingsRequested, m_ResetSettingsRequestedDelegate);
		DECL_EVENT(DisableSavingSettingsChangedDelegate, DisableSavingSettingsChanged, m_DisableSavingSettingsChangedDelegate);
		DECL_EVENT(StartupStateChangedDelegate, StartupStateChanged, m_StartupStateChangedDelegate);
		DECL_EVENT(ExitRequestedDelegate, ExitRequested, m_ExitRequestedDelegate);

		void SetTaskbarSettings(const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance);
		void SetTaskbarType(const txmp::TaskbarType &type);
		void SetBlurSupported(bool supported);
		void SetDisableSavingSettings(bool disabled);
		void SetStartupState(const wf::IReference<Windows::ApplicationModel::StartupTaskState> &state);
		void SetLogState(const txmp::LogLevel &level, const txmp::LogSinkState &sinkState);
		void SetMaximized(bool maximized);

		void RootLoaded(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void NavClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AccentClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AppearanceToggled(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ChangeColorClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

		void StartupToggled(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void SaveSettingsToggled(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void EditSettingsClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void OpenLogFileClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ResetSettingsClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ExitClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

		void LogLevelSelectionChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void DumpDynamicStateClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ResetDynamicStateClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void CompactThunkHeapClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

		void TrafficLightsPointerEntered(const IInspectable &sender, const wux::Input::PointerRoutedEventArgs &args);
		void TrafficLightsPointerExited(const IInspectable &sender, const wux::Input::PointerRoutedEventArgs &args);
		void MinimizeClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void MaximizeClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void CloseLightClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

	private:
		static constexpr std::size_t STATE_COUNT = 7;
		static constexpr std::size_t GENERAL_SECTION = STATE_COUNT;
		static constexpr std::size_t ADVANCED_SECTION = STATE_COUNT + 1;
		static constexpr std::size_t SECTION_COUNT = STATE_COUNT + 2;
		static constexpr float TITLEBAR_HEIGHT = 44.0f;

		std::array<wuxc::Button, SECTION_COUNT> NavButtons();
		std::array<wuxc::Button, 5> SegmentButtons();

		void SelectSection(std::size_t section, bool animate);
		void LoadSelectedState();
		void UpdateDependentControls();
		void UpdateSegmentIndicator(bool animate);
		void UpdateSelectionPill(bool animate);
		void CommitSelectedState();

		void PlayEntranceAnimation();
		void PlaySectionChangeAnimation();

		void UpdateTrafficLights();

		bool m_TrafficLightsHovered = false;

		Windows::ApplicationModel::Resources::ResourceLoader m_ResourceLoader = nullptr;
		std::array<txmp::TaskbarAppearance, STATE_COUNT> m_Appearances = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		std::size_t m_Section = 0;
		txmp::TaskbarType m_TaskbarType = txmp::TaskbarType::XAML;
		bool m_BlurSupported = true;
		bool m_Updating = false;
		bool m_AnimationsEnabled = true;
		bool m_SystemHasBattery;
	};
}

FACTORY(winrt::TranslucentTB::Xaml::Pages, SettingsPage);
