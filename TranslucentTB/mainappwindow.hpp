#pragma once
#include "arch.h"
#include "tray/traycontextmenu.hpp"
#include <cstddef>
#include <spdlog/common.h>
#include <tuple>
#include <windef.h>
#include "winrt.hpp"
#include "undefgetcurrenttime.h"
#include <winrt/TranslucentTB.Xaml.Models.Primitives.h>
#include <winrt/TranslucentTB.Xaml.Pages.h>
#include <winrt/Windows.System.h>
#include "redefgetcurrenttime.h"

#include "config/config.hpp"
#include "dynamicloader.hpp"
#include "managers/startupmanager.hpp"
#include "util/thread_independent_mutex.hpp"
#include "uwp/basexamlpagehost.hpp"

class Application;

class MainAppWindow final : public TrayContextMenu<winrt::TranslucentTB::Xaml::Pages::TrayFlyoutPage> {
private:
	using page_t = winrt::TranslucentTB::Xaml::Pages::TrayFlyoutPage;

	Application &m_App;
	bool m_HideIconOverride;

	Util::thread_independent_mutex m_PickerMutex;
	std::array<BaseXamlPageHost*, 7> m_ColorPickers{};

	// Settings window. It lives on its own XAML thread, so it is only ever touched through its dispatcher.
	struct SettingsSnapshot {
		std::array<txmp::TaskbarAppearance, 7> Appearances = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		txmp::TaskbarType Type = txmp::TaskbarType::XAML;
		bool BlurSupported = true;
		bool DisableSaving = false;
		std::optional<winrt::Windows::ApplicationModel::StartupTaskState> Startup;
		txmp::LogLevel LogLevel = txmp::LogLevel::Off;
		txmp::LogSinkState SinkState = txmp::LogSinkState::Failed;
	};

	Util::thread_independent_mutex m_SettingsMutex;
	BaseXamlPageHost *m_SettingsHost = nullptr;
	winrt::Windows::System::DispatcherQueue m_SettingsDispatcher = nullptr;

	page_t::StartupStateChanged_revoker m_StartupStateChangedRevoker;
	page_t::ExitRequested_revoker m_ExitRequestedRevoker;
	page_t::SettingsRequested_revoker m_SettingsRequestedRevoker;

	std::optional<UINT> m_NewInstanceMessage;

	LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void RefreshMenu() override;
	void RegisterMenuHandlers();
	bool OnPrimaryAction() override;

	void OpenSettings();
	SettingsSnapshot TakeSettingsSnapshot();
	static void ApplySettingsSnapshot(const winrt::TranslucentTB::Xaml::Pages::SettingsPage &page, const SettingsSnapshot &snapshot);
	void RefreshSettingsWindow();

	void TaskbarSettingsChanged(const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance);
	void ColorRequested(const txmp::TaskbarState &state);

	void OpenLogFileRequested();
	void LogLevelChanged(const txmp::LogLevel &level);
	void DumpDynamicStateRequested();
	void EditSettingsRequested();
	void ResetSettingsRequested();
	void DisableSavingSettingsChanged(bool disabled) noexcept;
	void ResetDynamicStateRequested();
	static void CompactThunkHeapRequested();

	winrt::fire_and_forget StartupStateChanged();
	void Exit();

	TaskbarAppearance &GetConfigForState(const txmp::TaskbarState &state);
	void UpdateTrayVisibility(bool visible);

public:
	MainAppWindow(Application &app, bool hideIconOverride, HINSTANCE hInstance, DynamicLoader &loader);
	~MainAppWindow();

	void ConfigurationChanged();
	void RemoveHideTrayIconOverride();

	static void PostNewInstanceNotification();
};
