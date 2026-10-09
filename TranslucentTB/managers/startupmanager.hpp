#pragma once
#include <optional>
#include <string_view>
#include "winrt.hpp"
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

class StartupManager {
private:
	// installed (unpackaged) copies start with Windows through the per-user Run key,
	// under the same value name the installer writes.
	static constexpr std::wstring_view RUN_KEY = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
	static constexpr std::wstring_view APPROVED_KEY = LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run)";
	static constexpr std::wstring_view RUN_VALUE = L"TranslucentTB";

	winrt::Windows::ApplicationModel::StartupTask m_StartupTask;
	bool m_UseRunKey;

	static winrt::Windows::ApplicationModel::StartupTaskState GetRunKeyState();
	static void EnableRunKey();
	static void DisableRunKey();

public:
	inline StartupManager() noexcept : m_StartupTask(nullptr), m_UseRunKey(false) { }

	winrt::fire_and_forget AcquireTask();
	inline void UseRunKey() noexcept { m_UseRunKey = true; }

	std::optional<winrt::Windows::ApplicationModel::StartupTaskState> GetState() const;
	wf::IAsyncAction Enable();
	void Disable();
	static void OpenSettingsPage();
};
