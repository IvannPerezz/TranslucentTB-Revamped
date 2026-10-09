#include "mainappwindow.hpp"
#include <member_thunk/member_thunk.hpp>

#include "application.hpp"
#include "constants.hpp"
#include "localization.hpp"
#include "resources/ids.h"
#include "../ProgramLog/log.hpp"
#include "../ProgramLog/error/win32.hpp"

LRESULT MainAppWindow::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_HOTKEY:
		if (wParam == RESET_STATE_GLOBAL_HOTKEY_ID)
		{
			ResetDynamicStateRequested();
		}
		return 0;

	case WM_CLOSE:
		Exit();
		return 1;

	case WM_QUERYENDSESSION:
		if (lParam & ENDSESSION_CLOSEAPP)
		{
			// The app is being queried if it can close for an update.
			RegisterApplicationRestart(nullptr, 0);
		}
		return 1;

	case WM_ENDSESSION:
		if (wParam)
		{
			// The app can be killed after processing this message, but we'll try doing it gracefully
			Exit();
		}

		return 0;

	default:
		if (uMsg == m_NewInstanceMessage)
		{
			if (!m_App.BringWelcomeToFront())
			{
				SendNotification(IDS_ALREADY_RUNNING, NIF_REALTIME, NIIF_INFO);
				m_App.GetWorker().ResetState(true);
			}

			return 0;
		}
		else
		{
			return TrayContextMenu::MessageHandler(uMsg, wParam, lParam);
		}
	}
}

void MainAppWindow::RefreshMenu()
{
	page().SetStartupState(m_App.GetStartupManager().GetState());
}

void MainAppWindow::RegisterMenuHandlers()
{
	const auto &menu = page();
	m_SettingsRequestedRevoker = menu.SettingsRequested(winrt::auto_revoke, { this, &MainAppWindow::OpenSettings });
	m_StartupStateChangedRevoker = menu.StartupStateChanged(winrt::auto_revoke, { this, &MainAppWindow::StartupStateChanged });
	m_ExitRequestedRevoker = menu.ExitRequested(winrt::auto_revoke, { this, &MainAppWindow::Exit });
}

bool MainAppWindow::OnPrimaryAction()
{
	OpenSettings();
	return true;
}

void MainAppWindow::OpenSettings()
{
	std::unique_lock lock(m_SettingsMutex);
	if (m_SettingsHost)
	{
		SetForegroundWindow(m_SettingsHost->handle());
		return;
	}

	using winrt::TranslucentTB::Xaml::Pages::SettingsPage;
	m_App.CreateXamlWindow<SettingsPage>(xaml_startup_position::center,
		[this, snapshot = TakeSettingsSnapshot(), inner_lock = std::move(lock)](const SettingsPage &settings, BaseXamlPageHost *host) mutable
		{
			// we are on the window's XAML thread here
			m_SettingsHost = host;
			m_SettingsDispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
			inner_lock.unlock();

			ApplySettingsSnapshot(settings, snapshot);

			settings.Closed([this]
			{
				std::scoped_lock guard(m_SettingsMutex);
				m_SettingsHost = nullptr;
				m_SettingsDispatcher = nullptr;
			});

			// everything that touches the config happens on the main thread
			settings.TaskbarSettingsChanged([this](const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance)
			{
				m_App.DispatchToMainThread([this, state, appearance] { TaskbarSettingsChanged(state, appearance); });
			});
			settings.ColorRequested([this](const txmp::TaskbarState &state)
			{
				m_App.DispatchToMainThread([this, state] { ColorRequested(state); });
			});
			settings.OpenLogFileRequested([this]
			{
				m_App.DispatchToMainThread([this] { OpenLogFileRequested(); });
			});
			settings.LogLevelChanged([this](const txmp::LogLevel &level)
			{
				m_App.DispatchToMainThread([this, level] { LogLevelChanged(level); });
			});
			settings.DumpDynamicStateRequested([this]
			{
				m_App.DispatchToMainThread([this] { DumpDynamicStateRequested(); });
			});
			settings.ResetDynamicStateRequested([this]
			{
				m_App.DispatchToMainThread([this] { ResetDynamicStateRequested(); });
			});
			settings.CompactThunkHeapRequested([this]
			{
				m_App.DispatchToMainThread([] { CompactThunkHeapRequested(); });
			});
			settings.EditSettingsRequested([this]
			{
				m_App.DispatchToMainThread([this] { EditSettingsRequested(); });
			});
			settings.ResetSettingsRequested([this]
			{
				m_App.DispatchToMainThread([this] { ResetSettingsRequested(); });
			});
			settings.DisableSavingSettingsChanged([this](bool disabled)
			{
				m_App.DispatchToMainThread([this, disabled] { DisableSavingSettingsChanged(disabled); });
			});
			settings.StartupStateChanged([this]
			{
				m_App.DispatchToMainThread([this] { StartupStateChanged(); });
			});
			settings.ExitRequested([this]
			{
				m_App.DispatchToMainThread([this] { Exit(); });
			});

			// window controls drawn by the page; going through ShowWindow keeps the system animations
			settings.MinimizeRequested([host]
			{
				ShowWindow(host->handle(), SW_MINIMIZE);
			});
			settings.MaximizeRequested([host]
			{
				const HWND window = host->handle();
				ShowWindow(window, IsZoomed(window) ? SW_RESTORE : SW_MAXIMIZE);
			});
		});
}

MainAppWindow::SettingsSnapshot MainAppWindow::TakeSettingsSnapshot()
{
	const auto &settings = m_App.GetConfigManager().GetConfig();
	const auto type = m_App.GetWorker().GetType();

	SettingsSnapshot snapshot;
	snapshot.Type = type == TaskbarType::Classic ? txmp::TaskbarType::Classic : txmp::TaskbarType::XAML;
	snapshot.BlurSupported = type == TaskbarType::XAML ? true : m_App.GetWorker().IsBlurAccentStateSupported();

	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::Desktop)] = static_cast<txmp::TaskbarAppearance>(settings.DesktopAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::VisibleWindow)] = txmp::OptionalTaskbarAppearance(settings.VisibleWindowAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::MaximisedWindow)] = txmp::OptionalTaskbarAppearance(settings.MaximisedWindowAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::StartOpened)] = txmp::OptionalTaskbarAppearance(settings.StartOpenedAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::SearchOpened)] = txmp::OptionalTaskbarAppearance(settings.SearchOpenedAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::TaskViewOpened)] = txmp::OptionalTaskbarAppearance(settings.TaskViewOpenedAppearance);
	snapshot.Appearances[static_cast<std::size_t>(txmp::TaskbarState::BatterySaver)] = txmp::OptionalTaskbarAppearance(settings.BatterySaverAppearance);

	if (const auto sink = Log::GetSink())
	{
		snapshot.LogLevel = static_cast<txmp::LogLevel>(sink->level());
		snapshot.SinkState = static_cast<txmp::LogSinkState>(sink->state());
	}

	snapshot.DisableSaving = settings.DisableSaving;
	snapshot.Startup = m_App.GetStartupManager().GetState();

	return snapshot;
}

void MainAppWindow::ApplySettingsSnapshot(const winrt::TranslucentTB::Xaml::Pages::SettingsPage &page, const SettingsSnapshot &snapshot)
{
	page.SetTaskbarType(snapshot.Type);
	page.SetBlurSupported(snapshot.BlurSupported);
	for (std::size_t i = 0; i < snapshot.Appearances.size(); ++i)
	{
		page.SetTaskbarSettings(static_cast<txmp::TaskbarState>(i), snapshot.Appearances[i]);
	}

	page.SetDisableSavingSettings(snapshot.DisableSaving);
	page.SetStartupState(snapshot.Startup);
	page.SetLogState(snapshot.LogLevel, snapshot.SinkState);
}

void MainAppWindow::RefreshSettingsWindow()
{
	winrt::Windows::System::DispatcherQueue dispatcher(nullptr);
	{
		std::scoped_lock guard(m_SettingsMutex);
		dispatcher = m_SettingsDispatcher;
	}

	if (!dispatcher)
	{
		return;
	}

	dispatcher.TryEnqueue([this, dispatcher, snapshot = TakeSettingsSnapshot()]
	{
		std::unique_lock guard(m_SettingsMutex);
		if (m_SettingsHost && m_SettingsDispatcher == dispatcher)
		{
			const auto settings = m_SettingsHost->page().try_as<winrt::TranslucentTB::Xaml::Pages::SettingsPage>();
			guard.unlock();

			if (settings)
			{
				ApplySettingsSnapshot(settings, snapshot);
			}
		}
	});
}

void MainAppWindow::TaskbarSettingsChanged(const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance)
{
	auto &config = GetConfigForState(state);

	// restore color because the context menu doesn't transmit that info
	appearance.Color(config.Color);

	if (const auto optAppearance = appearance.try_as<txmp::OptionalTaskbarAppearance>())
	{
		if (state == txmp::TaskbarState::Desktop) [[unlikely]]
		{
			throw std::invalid_argument("Desktop appearance is not optional");
		}

		static_cast<OptionalTaskbarAppearance &>(config) = optAppearance;
	}
	else
	{
		config = appearance;
	}

	m_App.GetWorker().ConfigurationChanged();
}

void MainAppWindow::ColorRequested(const txmp::TaskbarState &state)
{
	std::unique_lock lock(m_PickerMutex);
	auto &pickerHost = m_ColorPickers.at(static_cast<std::size_t>(state));
	if (!pickerHost)
	{
		auto &appearance = GetConfigForState(state);

		using winrt::TranslucentTB::Xaml::Pages::ColorPickerPage;
		m_App.CreateXamlWindow<ColorPickerPage>(xaml_startup_position::mouse,
			[this, &appearance, &pickerHost, state, inner_lock = std::move(lock)](const ColorPickerPage &picker, BaseXamlPageHost *host) mutable
			{
				pickerHost = host;
				inner_lock.unlock();

				auto closeRevoker = picker.Closed(winrt::auto_revoke, [this, state, &pickerHost]
				{
					m_App.DispatchToMainThread([this, state, &pickerHost]() mutable
					{
						m_App.GetWorker().RemoveColorPreview(state);

						std::scoped_lock guard(m_PickerMutex);
						pickerHost = nullptr;
					});
				});

				picker.ChangesCommitted([this, state, &appearance, &pickerHost, revoker = std::move(closeRevoker)](const winrt::Windows::UI::Color &color) mutable
				{
					revoker.revoke(); // we're already doing this.

					m_App.DispatchToMainThread([this, state, color, &appearance, &pickerHost]() mutable
					{
						appearance.Color = color;
						m_App.GetWorker().RemoveColorPreview(state); // remove color preview implicitly refreshes config
						RefreshSettingsWindow();

						std::scoped_lock guard(m_PickerMutex);
						pickerHost = nullptr;
					});
				});

				picker.ColorChanged([this, state](const winrt::Windows::UI::Color &color)
				{
					m_App.DispatchToMainThread([this, state, color]
					{
						m_App.GetWorker().ApplyColorPreview(state, color);
					});
				});
			},
			state,
			appearance.Color);
	}
	else
	{
		SetForegroundWindow(pickerHost->handle());
	}
}

void MainAppWindow::OpenLogFileRequested()
{
	if (const auto sink = Log::GetSink())
	{
		HresultVerify(win32::EditFile(sink->file()), spdlog::level::err, L"Failed to open log file.");
	}
}

void MainAppWindow::LogLevelChanged(const txmp::LogLevel &level)
{
	const auto spdlogLevel = static_cast<spdlog::level::level_enum>(level);

	auto &configManager = m_App.GetConfigManager();
	configManager.GetConfig().LogVerbosity = spdlogLevel;
	configManager.UpdateVerbosity();
}

void MainAppWindow::DumpDynamicStateRequested()
{
	m_App.GetWorker().DumpState();
}

void MainAppWindow::EditSettingsRequested()
{
	m_App.GetConfigManager().EditConfigFile();
}

void MainAppWindow::ResetSettingsRequested()
{
	auto &manager = m_App.GetConfigManager();
	manager.GetConfig() = { };

	manager.UpdateVerbosity();
	m_App.GetWorker().ConfigurationChanged();
	ConfigurationChanged();
}

void MainAppWindow::DisableSavingSettingsChanged(bool disabled) noexcept
{
	m_App.GetConfigManager().GetConfig().DisableSaving = disabled;
}

void MainAppWindow::ResetDynamicStateRequested()
{
	m_App.GetWorker().ResetState(true);
}

void MainAppWindow::CompactThunkHeapRequested()
{
	member_thunk::compact();
}

winrt::fire_and_forget MainAppWindow::StartupStateChanged()
{
	auto &manager = m_App.GetStartupManager();
	if (const auto state = manager.GetState())
	{
		switch (*state)
		{
			using enum winrt::Windows::ApplicationModel::StartupTaskState;

		case Disabled:
			co_await manager.Enable();
			break;

		case Enabled:
			manager.Disable();
			break;

		case DisabledByUser:
			StartupManager::OpenSettingsPage();
			break;

		default:
			MessagePrint(spdlog::level::err, L"Cannot change startup state because it is locked by external factors (for example Group Policy).");
			break;
		}
	}

	// the request might not have gone through (for example disabled by the user in Settings)
	RefreshSettingsWindow();
}

void MainAppWindow::Exit()
{
	m_App.GetConfigManager().SaveConfig();
	m_App.Shutdown();
}

TaskbarAppearance &MainAppWindow::GetConfigForState(const txmp::TaskbarState &state)
{
	auto &config = m_App.GetConfigManager().GetConfig();
	switch (state)
	{
		using enum txmp::TaskbarState;

	case Desktop: return config.DesktopAppearance;
	case VisibleWindow: return config.VisibleWindowAppearance;
	case MaximisedWindow: return config.MaximisedWindowAppearance;
	case StartOpened: return config.StartOpenedAppearance;
	case SearchOpened: return config.SearchOpenedAppearance;
	case TaskViewOpened: return config.TaskViewOpenedAppearance;
	case BatterySaver: return config.BatterySaverAppearance;
	default: throw std::invalid_argument("Unknown taskbar state");
	}
}

void MainAppWindow::UpdateTrayVisibility(bool visible)
{
	if (!m_HideIconOverride && visible)
	{
		Show();
	}
	else
	{
		Hide();
	}
}

MainAppWindow::MainAppWindow(Application &app, bool hideIconOverride, HINSTANCE hInstance, DynamicLoader &loader) :
	// make the window topmost so that the context menu shows correctly
	MessageWindow(TRAY_WINDOW, APP_NAME, hInstance, WS_POPUP, WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP),
	TrayContextMenu(TRAY_GUID, MAKEINTRESOURCE(IDI_TRAYWHITEICON), MAKEINTRESOURCE(IDI_TRAYBLACKICON), loader),
	m_App(app),
	m_HideIconOverride(hideIconOverride),
	m_NewInstanceMessage(Window::RegisterMessage(WM_TTBNEWINSTANCESTARTED))
{
	// Register global hotkey for resetting dynamic state
	const BOOL hotkeyRegistered = RegisterHotKey(
		handle(),
		RESET_STATE_GLOBAL_HOTKEY_ID,
		MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT,
		VK_F1
	);
	if (!hotkeyRegistered)
	{
		LastErrorHandle(spdlog::level::warn, L"Unable to register global hotkey for dynamic state reset");
	}

	RegisterMenuHandlers();

	ConfigurationChanged();
}

MainAppWindow::~MainAppWindow()
{
	// Unregister the global hotkey
	UnregisterHotKey(handle(), RESET_STATE_GLOBAL_HOTKEY_ID);
}

void MainAppWindow::ConfigurationChanged()
{
	const Config &config = m_App.GetConfigManager().GetConfig();

	UpdateTrayVisibility(!config.HideTray.value_or(false));
	SetXamlContextMenuOverride(config.UseXamlContextMenu);
	RefreshSettingsWindow();
}

void MainAppWindow::RemoveHideTrayIconOverride()
{
	m_HideIconOverride = false;
	UpdateTrayVisibility(!m_App.GetConfigManager().GetConfig().HideTray.value_or(false));
}

void MainAppWindow::PostNewInstanceNotification()
{
	if (const auto msg = Window::RegisterMessage(WM_TTBNEWINSTANCESTARTED))
	{
		if (const auto runningInstance = Window::Find(TRAY_WINDOW, APP_NAME))
		{
			AllowSetForegroundWindow(runningInstance.process_id());
			runningInstance.post_message(*msg);
		}
	}
}
