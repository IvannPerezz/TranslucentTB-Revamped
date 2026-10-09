#include "startupmanager.hpp"
#include <format>
#include <winrt/Windows.Foundation.Collections.h>

#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/error/winrt.hpp"
#include "../uwp/uwp.hpp"
#include "../localization.hpp"
#include "../resources/ids.h"
#include "win32.hpp"

winrt::Windows::ApplicationModel::StartupTaskState StartupManager::GetRunKeyState()
{
	using enum winrt::Windows::ApplicationModel::StartupTaskState;

	DWORD size = 0;
	if (RegGetValue(HKEY_CURRENT_USER, RUN_KEY.data(), RUN_VALUE.data(), RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
	{
		return Disabled;
	}

	// turning the entry off in Task Manager or Settings keeps the Run value but flags it here (odd first byte)
	BYTE approved[12] = { };
	DWORD approvedSize = sizeof(approved);
	if (RegGetValue(HKEY_CURRENT_USER, APPROVED_KEY.data(), RUN_VALUE.data(), RRF_RT_REG_BINARY, nullptr, approved, &approvedSize) == ERROR_SUCCESS &&
		approvedSize > 0 && (approved[0] & 1))
	{
		return DisabledByUser;
	}

	return Enabled;
}

void StartupManager::EnableRunKey()
{
	const auto [exe, hr] = win32::GetExeLocation();
	if (FAILED(hr))
	{
		HresultHandle(hr, spdlog::level::err, L"Failed to determine executable location");
		return;
	}

	const auto command = std::format(L"\"{}\"", exe.native());
	const auto status = RegSetKeyValue(HKEY_CURRENT_USER, RUN_KEY.data(), RUN_VALUE.data(), REG_SZ, command.c_str(), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
	HresultVerify(HRESULT_FROM_WIN32(status), spdlog::level::err, L"Failed to add the startup entry");
}

void StartupManager::DisableRunKey()
{
	const auto status = RegDeleteKeyValue(HKEY_CURRENT_USER, RUN_KEY.data(), RUN_VALUE.data());
	if (status != ERROR_FILE_NOT_FOUND)
	{
		HresultVerify(HRESULT_FROM_WIN32(status), spdlog::level::err, L"Failed to remove the startup entry");
	}
}

winrt::fire_and_forget StartupManager::AcquireTask() try
{
	if (!m_StartupTask)
	{
		m_StartupTask = co_await winrt::Windows::ApplicationModel::StartupTask::GetAsync(L"TranslucentTB");
	}
}
HresultErrorCatch(spdlog::level::err, L"Failed to load startup task.");

std::optional<winrt::Windows::ApplicationModel::StartupTaskState> StartupManager::GetState() const try
{
	if (m_UseRunKey)
	{
		return GetRunKeyState();
	}

	return m_StartupTask ? std::optional(m_StartupTask.State()) : std::nullopt;
}
HresultErrorCatch(spdlog::level::warn, L"Failed to get startup task status.");

wf::IAsyncAction StartupManager::Enable() try
{
	if (m_UseRunKey)
	{
		EnableRunKey();
	}
	else if (m_StartupTask)
	{
		const auto result = co_await m_StartupTask.RequestEnableAsync();

		using enum winrt::Windows::ApplicationModel::StartupTaskState;
		if (result != Enabled && result != EnabledByPolicy)
		{
			Localization::ShowLocalizedMessageBox(IDS_STARTUPTASK_BROKEN, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND).detach();
		}
	}
}
HresultErrorCatch(spdlog::level::err, L"Failed to enable startup task.");

void StartupManager::Disable() try
{
	if (m_UseRunKey)
	{
		DisableRunKey();
	}
	else if (m_StartupTask)
	{
		m_StartupTask.Disable();
	}
}
HresultErrorCatch(spdlog::level::err, L"Failed to disable startup task.");

void StartupManager::OpenSettingsPage() try
{
	UWP::OpenUri(wf::Uri(L"ms-settings:startupapps"));
}
HresultErrorCatch(spdlog::level::err, L"Failed to open Settings app.");
