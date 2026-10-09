#include "basexamlpagehost.hpp"
#include <cmath>
#include <ShellScalingApi.h>
#include <wil/resource.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

#include "win32.hpp"
#include "../ProgramLog/error/win32.hpp"
#include "../uwp/uwp.hpp"

void BaseXamlPageHost::UpdateFrame()
{
	if (m_CornerRadius > 0.0f)
	{
		// the window region gives the shape, the system corners and border would only fight it
		const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DONOTROUND;
		DwmSetWindowAttribute(m_WindowHandle, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

		const COLORREF border = DWMWA_COLOR_NONE;
		DwmSetWindowAttribute(m_WindowHandle, DWMWA_BORDER_COLOR, &border, sizeof(border));
		return;
	}

	// Magic that gives us shadows
	// we use the top side because any other side would cause a single line of white pixels to
	// suddenly flash when resizing the color picker.
	// can't use 0, that does nothing
	// or -1: turns it full white.
	const MARGINS margins = { 0, 0, 1, 0 };
	HresultVerify(DwmExtendFrameIntoClientArea(m_WindowHandle, &margins), spdlog::level::info, L"Failed to extend frame into client area");
}

void BaseXamlPageHost::UpdateRoundedRegion()
{
	if (IsZoomed(m_WindowHandle))
	{
		// maximized windows fill the work area with square corners
		if (!SetWindowRgn(m_WindowHandle, nullptr, true))
		{
			LastErrorHandle(spdlog::level::info, L"Failed to clear window region");
		}
	}
	else if (const auto client = client_rect())
	{
		// a window region is not anti-aliased; the page draws a hairline edge over it to soften the steps
		const int diameter = static_cast<int>(std::round(m_CornerRadius * 2.0f * GetDpiScale(monitor())));
		if (wil::unique_hrgn region { CreateRoundRectRgn(0, 0, client->right + 1, client->bottom + 1, diameter, diameter) })
		{
			if (SetWindowRgn(m_WindowHandle, region.get(), true))
			{
				region.release(); // the system owns the region now
			}
			else
			{
				LastErrorHandle(spdlog::level::info, L"Failed to set window region");
			}
		}
	}
}

wf::Rect BaseXamlPageHost::ScaleRect(wf::Rect rect, float scale)
{
	return {
		rect.X * scale,
		rect.Y * scale,
		rect.Width * scale,
		rect.Height * scale
	};
}

HMONITOR BaseXamlPageHost::GetInitialMonitor(POINT &cursor, xaml_startup_position position)
{
	if (position == xaml_startup_position::mouse)
	{
		if (!GetCursorPos(&cursor))
		{
			LastErrorHandle(spdlog::level::info, L"Failed to get cursor position");
		}
	}

	return MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
}

float BaseXamlPageHost::GetDpiScale(HMONITOR mon)
{
	UINT dpiX, dpiY;
	if (const HRESULT hr = GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY); SUCCEEDED(hr))
	{
		return static_cast<float>(dpiX) / USER_DEFAULT_SCREEN_DPI;
	}
	else
	{
		HresultHandle(hr, spdlog::level::info, L"Failed to get monitor DPI");
		return 1.0f;
	}
}

void BaseXamlPageHost::CalculateInitialPosition(int &x, int &y, int width, int height, POINT cursor, const RECT &workArea, xaml_startup_position position) noexcept
{
	if (position == xaml_startup_position::mouse)
	{
		// Center on the mouse
		x = cursor.x - (width / 2);
		y = cursor.y - (height / 2);

		AdjustWindowPosition(x, y, width, height, workArea);
	}
	else
	{
		x = ((workArea.right - workArea.left - width) / 2) + workArea.left;
		y = ((workArea.bottom - workArea.top - height) / 2) + workArea.top;
	}
}

bool BaseXamlPageHost::AdjustWindowPosition(int &x, int &y, int width, int height, const RECT &workArea) noexcept
{
	RECT coords = { x, y, x + width, y + height };
	if (win32::RectFitsInRect(workArea, coords))
	{
		// It fits, nothing to do.
		return false;
	}

	const bool rightDoesntFits = coords.right > workArea.right;
	const bool leftDoesntFits = coords.left < workArea.left;
	const bool bottomDoesntFits = coords.bottom > workArea.bottom;
	const bool topDoesntFits = coords.top < workArea.top;

	if ((rightDoesntFits && leftDoesntFits) || (bottomDoesntFits && topDoesntFits))
	{
		// Doesn't fits in the monitor work area :(
		return true;
	}

	// Offset the rect so that it is completely in the work area
	int x_offset = 0;
	if (rightDoesntFits)
	{
		x_offset = workArea.right - coords.right; // Negative offset
	}
	else if (leftDoesntFits)
	{
		x_offset = workArea.left - coords.left;
	}

	int y_offset = 0;
	if (bottomDoesntFits)
	{
		y_offset = workArea.bottom - coords.bottom; // Negative offset
	}
	else if (topDoesntFits)
	{
		y_offset = workArea.top - coords.top;
	}

	win32::OffsetRect(coords, x_offset, y_offset);
	x = coords.left;
	y = coords.top;

	return true;
}

LRESULT BaseXamlPageHost::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_QUERYENDSESSION:
		if (!TryClose())
		{
			return 0;
		}
		else
		{
			return 1;
		}

	case WM_SIZE:
		if (m_CornerRadius > 0.0f && wParam != SIZE_MINIMIZED)
		{
			UpdateRoundedRegion();

			// the system can resize this window by itself (maximize, restore, snap), so the island has to follow
			if (!SetWindowPos(m_interopWnd, nullptr, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
			{
				LastErrorHandle(spdlog::level::info, L"Failed to set interop window size");
			}
		}
		[[fallthrough]];

	case WM_SETTINGCHANGE:
	case WM_THEMECHANGED:
		if (const auto coreWin = UWP::GetCoreWindow())
		{
			// forward theme changes to the fake core window
			// so that they propagate to our islands
			// do the same for size: https://github.com/microsoft/microsoft-ui-xaml/issues/3577#issuecomment-1399250405
			coreWin.send_message(uMsg, wParam, lParam);
		}

		break;

	case WM_NCCALCSIZE:
		return 0;

	case WM_NCACTIVATE:
		if (m_CornerRadius > 0.0f)
		{
			// there is no visible caption to repaint, only keep the activation change
			return DefWindowProc(m_WindowHandle, uMsg, wParam, -1);
		}
		break;

	case WM_GETMINMAXINFO:
		if (m_CornerRadius > 0.0f)
		{
			// without this, a captioned window maximizes past the work area by its (invisible) border
			MONITORINFO info = { sizeof(info) };
			if (GetMonitorInfo(monitor(), &info))
			{
				const auto minMax = reinterpret_cast<MINMAXINFO *>(lParam);
				minMax->ptMaxPosition = { info.rcWork.left - info.rcMonitor.left, info.rcWork.top - info.rcMonitor.top };
				minMax->ptMaxSize = { info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top };
				return 0;
			}
		}
		break;

	case WM_DWMCOMPOSITIONCHANGED:
		UpdateFrame();
		return 0;

	case WM_SETFOCUS:
		if (!SetFocus(m_interopWnd))
		{
			LastErrorHandle(spdlog::level::info, L"Failed to set focus to Island host");
		}
		return 0;

	case WM_DPICHANGED:
	{
		const auto newRect = reinterpret_cast<RECT *>(lParam);

		ResizeWindow(newRect->left, newRect->top, newRect->right - newRect->left, newRect->bottom - newRect->top, true);
		return 0;
	}
	}

	return MessageWindow::MessageHandler(uMsg, wParam, lParam);
}

void BaseXamlPageHost::ResizeWindow(int x, int y, int width, int height, bool move, UINT flags)
{
	flags |= SWP_NOACTIVATE | SWP_NOZORDER;
	if (!SetWindowPos(m_interopWnd, nullptr, 0, 0, width, height, flags)) [[unlikely]]
	{
		LastErrorHandle(spdlog::level::info, L"Failed to set interop window position");
	}

	if (!SetWindowPos(m_WindowHandle, nullptr, x, y, width, height, flags | (move ? 0 : SWP_NOMOVE))) [[unlikely]]
	{
		LastErrorHandle(spdlog::level::info, L"Failed to set host window position");
	}
}

void BaseXamlPageHost::PositionDragRegion(wf::Rect position, wf::Rect buttonsRegion, UINT flags)
{
	if (const auto wndRect = rect())
	{
		m_DragRegion.Position(*wndRect, position, buttonsRegion, flags);
	}
}

bool BaseXamlPageHost::PaintBackground(HDC dc, const RECT &target, winrt::Windows::UI::Color col)
{
	if (!m_BackgroundBrush || m_BackgroundColor != col) [[unlikely]]
	{
		m_BackgroundBrush.reset(CreateSolidBrush(RGB(col.R, col.G, col.B)));
		if (m_BackgroundBrush)
		{
			m_BackgroundColor = col;
		}
		else
		{
			MessagePrint(spdlog::level::info, L"Failed to create background brush");
			return false;
		}
	}

	if (FillRect(dc, &target, m_BackgroundBrush.get()))
	{
		return true;
	}
	else
	{
		LastErrorHandle(spdlog::level::info, L"Failed to fill rectangle.");
		return false;
	}
}

BaseXamlPageHost::BaseXamlPageHost(WindowClass &classRef, WindowClass &dragRegionClass, float cornerRadius) :
	// a caption and window boxes (even though they are drawn by the page) give the system minimize, maximize, open and close animations
	MessageWindow(classRef, { }, cornerRadius > 0.0f ? WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX : 0),
	m_CornerRadius(cornerRadius),
	m_DragRegion(dragRegionClass, m_WindowHandle)
{
	UpdateFrame();

	auto nativeSource = m_source.as<IDesktopWindowXamlSourceNative2>();
	HresultVerify(nativeSource->AttachToWindow(m_WindowHandle), spdlog::level::critical, L"Failed to attach DesktopWindowXamlSource");
	HresultVerify(nativeSource->get_WindowHandle(m_interopWnd.put()), spdlog::level::critical, L"Failed to get interop window handle");

	m_focusToken = m_source.TakeFocusRequested([](const wuxh::DesktopWindowXamlSource &sender, const wuxh::DesktopWindowXamlSourceTakeFocusRequestedEventArgs &args)
	{
		const auto request = args.Request();
		const auto reason = request.Reason();
		if (reason == wuxh::XamlSourceFocusNavigationReason::First || reason == wuxh::XamlSourceFocusNavigationReason::Last)
		{
			// just cycle back to beginning
			sender.NavigateFocus(request);
		}
	});
}
