#include "pch.h"
#include <chrono>
#include <string_view>
#include <wil/resource.h>

#include "undefgetcurrenttime.h"
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include "redefgetcurrenttime.h"

#include "SettingsPage.h"
#if __has_include("Pages/SettingsPage.g.cpp")
#include "Pages/SettingsPage.g.cpp"
#endif

#include "arch.h"
#include <winbase.h>

namespace
{
	// Spring tuning. Composition springs take a damping ratio and a period, the same two
	// knobs Apple exposes (damping + response). 1.0 = no overshoot, lower = bouncier.
	constexpr float SELECTION_DAMPING = 0.82f; // sidebar pill and effect selector: a little life, no wobble
	constexpr float SELECTION_PERIOD = 0.07f;
	constexpr float SECTION_DAMPING = 1.0f;    // section content: critically damped
	constexpr float SECTION_PERIOD = 0.06f;

	constexpr std::array<std::wstring_view, 7> TITLE_KEYS = {
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_Desktop/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_VisibleWindow/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_MaximizedWindow/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_StartOpened/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_SearchOpened/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_TaskViewOpened/Text",
		L"/TranslucentTB.Xaml/Resources/TrayFlyoutPage_BatterySaver/Text"
	};

	constexpr std::array<std::wstring_view, 7> DESCRIPTION_KEYS = {
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_Desktop/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_VisibleWindow/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_MaximisedWindow/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_StartOpened/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_SearchOpened/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_TaskViewOpened/Text",
		L"/TranslucentTB.Xaml/Resources/SettingsPage_Description_BatterySaver/Text"
	};

	bool IsOn(const wuxc::Primitives::ToggleButton &button)
	{
		const auto checked = button.IsChecked();
		return checked && checked.Value();
	}

	wf::TimeSpan Seconds(float seconds)
	{
		return std::chrono::duration_cast<wf::TimeSpan>(std::chrono::duration<float>(seconds));
	}

	wuc::Visual VisualOf(const wux::UIElement &element)
	{
		return wuxh::ElementCompositionPreview::GetElementVisual(element);
	}

	void SetTranslation(const wux::UIElement &element, wfn::float3 value)
	{
		const auto visual = VisualOf(element);
		visual.StopAnimation(L"Translation");
		visual.Properties().InsertVector3(L"Translation", value);
	}

	// Springs start from the current on-screen value, so a new target mid-flight redirects
	// the motion instead of restarting it.
	void SpringVector(const wux::UIElement &element, std::wstring_view property, wfn::float3 to, float damping, float period, float delay = 0.0f)
	{
		const auto visual = VisualOf(element);
		const auto animation = visual.Compositor().CreateSpringVector3Animation();
		animation.FinalValue(to);
		animation.DampingRatio(damping);
		animation.Period(Seconds(period));
		if (delay > 0.0f)
		{
			animation.DelayTime(Seconds(delay));
			animation.DelayBehavior(wuc::AnimationDelayBehavior::SetInitialValueBeforeDelay);
		}

		visual.StartAnimation(property, animation);
	}

	void FadeTo(const wux::UIElement &element, float from, float to, float duration, float delay = 0.0f)
	{
		const auto visual = VisualOf(element);
		const auto compositor = visual.Compositor();
		const auto animation = compositor.CreateScalarKeyFrameAnimation();
		animation.InsertKeyFrame(0.0f, from);
		animation.InsertKeyFrame(1.0f, to, compositor.CreateCubicBezierEasingFunction({ 0.2f, 0.0f }, { 0.0f, 1.0f }));
		animation.Duration(Seconds(duration));
		if (delay > 0.0f)
		{
			animation.DelayTime(Seconds(delay));
			animation.DelayBehavior(wuc::AnimationDelayBehavior::SetInitialValueBeforeDelay);
		}

		visual.StartAnimation(L"Opacity", animation);
	}
}

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	SettingsPage::SettingsPage()
	{
		SYSTEM_POWER_STATUS powerStatus;
		if (GetSystemPowerStatus(&powerStatus))
		{
			// 128 means no system battery. assume everything else
			// means the system has one.
			m_SystemHasBattery = powerStatus.BatteryFlag != 128;
		}
		else
		{
			m_SystemHasBattery = true; // assume the system has a battery
		}

		m_Appearances[0] = txmp::TaskbarAppearance();
		for (std::size_t i = 1; i < STATE_COUNT; ++i)
		{
			m_Appearances[i] = txmp::OptionalTaskbarAppearance();
		}
	}

	void SettingsPage::InitializeComponent()
	{
		ComponentConnectorT::InitializeComponent();

		m_ResourceLoader = Windows::ApplicationModel::Resources::ResourceLoader::GetForUIContext(UIContext());
		m_AnimationsEnabled = Windows::UI::ViewManagement::UISettings().AnimationsEnabled();

		using wuxh::ElementCompositionPreview;
		ElementCompositionPreview::SetIsTranslationEnabled(SelectionPill(), true);
		ElementCompositionPreview::SetIsTranslationEnabled(SegmentIndicator(), true);
		ElementCompositionPreview::SetIsTranslationEnabled(DetailHost(), true);
		for (const auto &button : NavButtons())
		{
			ElementCompositionPreview::SetIsTranslationEnabled(button, true);
		}

		// keep the indicators glued to their targets when layout changes (resizes, DPI, hidden blur option)
		const auto weak = get_weak();
		SegNormal().SizeChanged([weak](const IInspectable &, const wux::SizeChangedEventArgs &)
		{
			if (const auto self = weak.get())
			{
				self->UpdateSegmentIndicator(false);
			}
		});
		NavList().SizeChanged([weak](const IInspectable &, const wux::SizeChangedEventArgs &)
		{
			if (const auto self = weak.get())
			{
				self->UpdateSelectionPill(false);
			}
		});
		RegisterPropertyChangedCallback(winrt::TranslucentTB::Xaml::Pages::FramelessPage::IsActiveProperty(), [weak](const wux::DependencyObject &, const wux::DependencyProperty &)
		{
			if (const auto self = weak.get())
			{
				self->UpdateTrafficLights();
			}
		});

		UpdateTrafficLights();
		SelectSection(0, false);
	}

	wf::Rect SettingsPage::ExpandedDragRegion()
	{
		// the top strip of the visible window, up to the window controls
		const auto card = WindowCard();
		const auto origin = card.TransformToVisual(*this).TransformPoint({ 0.0f, 0.0f });
		const auto lights = TrafficLights().TransformToVisual(*this).TransformPoint({ 0.0f, 0.0f });

		float width = lights.X - origin.X;
		if (width <= 0.0f)
		{
			width = static_cast<float>(card.ActualWidth());
		}

		return { origin.X, origin.Y, width, TITLEBAR_HEIGHT };
	}

	void SettingsPage::SetTaskbarSettings(const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance)
	{
		const auto index = static_cast<std::size_t>(state);
		if (index >= STATE_COUNT || !appearance)
		{
			return;
		}

		m_Appearances[index] = appearance;
		if (index == m_Section)
		{
			LoadSelectedState();
		}
	}

	void SettingsPage::SetTaskbarType(const txmp::TaskbarType &type)
	{
		m_TaskbarType = type;
		UpdateDependentControls();
	}

	void SettingsPage::SetBlurSupported(bool supported)
	{
		m_BlurSupported = supported;
		UpdateDependentControls();
	}

	void SettingsPage::SetDisableSavingSettings(bool disabled)
	{
		m_Updating = true;
		const auto guard = wil::scope_exit([this]() noexcept { m_Updating = false; });

		SaveSettingsSwitch().IsChecked(!disabled);
	}

	void SettingsPage::SetStartupState(const wf::IReference<Windows::ApplicationModel::StartupTaskState> &state)
	{
		m_Updating = true;
		const auto guard = wil::scope_exit([this]() noexcept { m_Updating = false; });

		const auto startup = StartupSwitch();
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

	void SettingsPage::SetLogState(const txmp::LogLevel &level, const txmp::LogSinkState &sinkState)
	{
		m_Updating = true;
		const auto guard = wil::scope_exit([this]() noexcept { m_Updating = false; });

		const bool logAvailable = sinkState != txmp::LogSinkState::Failed;
		const auto levelBox = LogLevelBox();
		levelBox.SelectedIndex(static_cast<int32_t>(level));
		levelBox.IsEnabled(logAvailable);
		DumpDynamicStateButton().IsEnabled(logAvailable);
		OpenLogFileButton().IsEnabled(sinkState == txmp::LogSinkState::Opened);
	}

	void SettingsPage::SetMaximized(bool maximized)
	{
		// arrowheads point outward to maximize and inward to restore
		const auto data = maximized ?
			L"M 5.9,5.9 L 1.7,5.9 L 5.9,1.7 Z M 7.1,7.1 L 11.3,7.1 L 7.1,11.3 Z" :
			L"M 3.1,3.1 L 7.3,3.1 L 3.1,7.3 Z M 9.9,9.9 L 5.7,9.9 L 9.9,5.7 Z";
		MaximizeGlyph().Data(wux::Markup::XamlBindingHelper::ConvertValue(xaml_typename<wux::Media::Geometry>(), box_value(data)).as<wux::Media::Geometry>());

		// a maximized window fills the work area edge to edge, so it loses its rounded corners and hairline
		const double radius = maximized ? 0.0 : 26.0;
		const auto card = WindowCard();
		card.CornerRadius({ radius, radius, radius, radius });
		const double stroke = maximized ? 0.0 : 1.0;
		card.BorderThickness({ stroke, stroke, stroke, stroke });
		const double inner = maximized ? 0.0 : radius - 1.0;
		Sidebar().CornerRadius({ inner, 0.0, 0.0, inner });
	}

	void SettingsPage::RootLoaded(const IInspectable &, const wux::RoutedEventArgs &)
	{
		UpdateSelectionPill(false);
		UpdateSegmentIndicator(false);

		if (m_AnimationsEnabled)
		{
			PlayEntranceAnimation();
		}
	}

	void SettingsPage::NavClicked(const IInspectable &sender, const wux::RoutedEventArgs &)
	{
		const auto buttons = NavButtons();
		for (std::size_t i = 0; i < buttons.size(); ++i)
		{
			if (buttons[i] == sender && i != m_Section)
			{
				SelectSection(i, true);
				break;
			}
		}
	}

	void SettingsPage::AccentClicked(const IInspectable &sender, const wux::RoutedEventArgs &)
	{
		if (m_Updating || m_Section >= STATE_COUNT)
		{
			return;
		}

		const auto buttons = SegmentButtons();
		for (std::size_t i = 0; i < buttons.size(); ++i)
		{
			if (buttons[i] == sender)
			{
				const auto accent = static_cast<txmp::AccentState>(i);
				if (m_Appearances[m_Section].Accent() != accent)
				{
					m_Appearances[m_Section].Accent(accent);
					UpdateSegmentIndicator(true);
					UpdateDependentControls();
					CommitSelectedState();
				}

				break;
			}
		}
	}

	void SettingsPage::AppearanceToggled(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (m_Updating || m_Section >= STATE_COUNT)
		{
			return;
		}

		const auto &appearance = m_Appearances[m_Section];
		if (const auto optAppearance = appearance.try_as<txmp::OptionalTaskbarAppearance>())
		{
			optAppearance.Enabled(IsOn(EnabledSwitch()));
		}

		appearance.ShowPeek(IsOn(PeekSwitch()));
		appearance.ShowLine(IsOn(LineSwitch()));

		UpdateDependentControls();
		CommitSelectedState();
	}

	void SettingsPage::ChangeColorClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (m_Section < STATE_COUNT)
		{
			m_ColorRequestedDelegate(static_cast<txmp::TaskbarState>(m_Section));
		}
	}

	void SettingsPage::StartupToggled(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (!m_Updating)
		{
			m_StartupStateChangedDelegate();
		}
	}

	void SettingsPage::SaveSettingsToggled(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (!m_Updating)
		{
			m_DisableSavingSettingsChangedDelegate(!IsOn(SaveSettingsSwitch()));
		}
	}

	void SettingsPage::EditSettingsClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_EditSettingsRequestedDelegate();
	}

	void SettingsPage::OpenLogFileClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_OpenLogFileRequestedDelegate();
	}


	void SettingsPage::ResetSettingsClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_ResetSettingsRequestedDelegate();
	}

	void SettingsPage::ExitClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_ExitRequestedDelegate();
	}

	void SettingsPage::LogLevelSelectionChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		const auto index = LogLevelBox().SelectedIndex();
		if (!m_Updating && index >= 0)
		{
			m_LogLevelChangedDelegate(static_cast<txmp::LogLevel>(index));
		}
	}

	void SettingsPage::DumpDynamicStateClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_DumpDynamicStateRequestedDelegate();
	}

	void SettingsPage::ResetDynamicStateClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_ResetDynamicStateRequestedDelegate();
	}

	void SettingsPage::CompactThunkHeapClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_CompactThunkHeapRequestedDelegate();
	}


	void SettingsPage::TrafficLightsPointerEntered(const IInspectable &, const wux::Input::PointerRoutedEventArgs &)
	{
		// like macOS, hovering the group reveals every glyph at once
		m_TrafficLightsHovered = true;
		UpdateTrafficLights();
	}

	void SettingsPage::TrafficLightsPointerExited(const IInspectable &, const wux::Input::PointerRoutedEventArgs &)
	{
		m_TrafficLightsHovered = false;
		UpdateTrafficLights();
	}

	void SettingsPage::MinimizeClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		m_MinimizeRequestedDelegate();
	}

	void SettingsPage::MaximizeClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		// the glyph follows once the host reports the new state through SetMaximized
		m_MaximizeRequestedDelegate();
	}

	void SettingsPage::CloseLightClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		Close();
	}


	void SettingsPage::UpdateTrafficLights()
	{
		// grey while the window is in the background, unless the pointer is over them
		const bool lit = IsActive() || m_TrafficLightsHovered;
		const auto paint = [lit](const wuxc::Button &button, Windows::UI::Color color)
		{
			button.Background(wux::Media::SolidColorBrush(lit ? color : Windows::UI::Color { 255, 86, 86, 90 }));
		};

		paint(MinimizeLight(), { 255, 254, 188, 46 });
		paint(MaximizeLight(), { 255, 40, 200, 64 });
		paint(CloseLight(), { 255, 255, 95, 87 });

		const double glyphOpacity = m_TrafficLightsHovered ? 1.0 : 0.0;
		MinimizeGlyph().Opacity(glyphOpacity);
		MaximizeGlyph().Opacity(glyphOpacity);
		CloseGlyph().Opacity(glyphOpacity);
	}

	std::array<wuxc::Button, SettingsPage::SECTION_COUNT> SettingsPage::NavButtons()
	{
		// same order as TaskbarState, then General and Advanced
		return {
			NavDesktop(),
			NavVisibleWindow(),
			NavMaximisedWindow(),
			NavStartOpened(),
			NavSearchOpened(),
			NavTaskViewOpened(),
			NavBatterySaver(),
			NavGeneral(),
			NavAdvanced()
		};
	}

	std::array<wuxc::Button, 5> SettingsPage::SegmentButtons()
	{
		// same order as AccentState
		return { SegNormal(), SegOpaque(), SegClear(), SegBlur(), SegAcrylic() };
	}

	void SettingsPage::SelectSection(std::size_t section, bool animate)
	{
		m_Section = section;

		const auto visibleIf = [](bool condition)
		{
			return condition ? wux::Visibility::Visible : wux::Visibility::Collapsed;
		};

		StatePanel().Visibility(visibleIf(section < STATE_COUNT));
		GeneralPanel().Visibility(visibleIf(section == GENERAL_SECTION));
		AdvancedPanel().Visibility(visibleIf(section == ADVANCED_SECTION));
		if (section < STATE_COUNT)
		{
			LoadSelectedState();
		}

		UpdateSelectionPill(animate);
		DetailScroller().ChangeView(nullptr, 0.0, nullptr, true);

		if (animate && m_AnimationsEnabled)
		{
			PlaySectionChangeAnimation();
		}
	}

	void SettingsPage::LoadSelectedState()
	{
		if (m_Section >= STATE_COUNT)
		{
			return;
		}

		m_Updating = true;
		const auto guard = wil::scope_exit([this]() noexcept { m_Updating = false; });

		StateTitle().Text(m_ResourceLoader.GetString(TITLE_KEYS[m_Section]));
		StateDescription().Text(m_ResourceLoader.GetString(DESCRIPTION_KEYS[m_Section]));

		const auto &appearance = m_Appearances[m_Section];
		if (const auto optAppearance = appearance.try_as<txmp::OptionalTaskbarAppearance>())
		{
			EnabledGroup().Visibility(wux::Visibility::Visible);
			EnabledSwitch().IsChecked(optAppearance.Enabled());
		}
		else
		{
			EnabledGroup().Visibility(wux::Visibility::Collapsed);
		}

		PeekSwitch().IsChecked(appearance.ShowPeek());
		LineSwitch().IsChecked(appearance.ShowLine());

		UpdateDependentControls();
		UpdateSegmentIndicator(false);
	}

	void SettingsPage::UpdateDependentControls()
	{
		const auto showPeek = m_TaskbarType == txmp::TaskbarType::Classic;
		PeekRow().Visibility(showPeek ? wux::Visibility::Visible : wux::Visibility::Collapsed);
		LineRow().Visibility(showPeek ? wux::Visibility::Collapsed : wux::Visibility::Visible);

		SegBlur().Visibility(m_BlurSupported ? wux::Visibility::Visible : wux::Visibility::Collapsed);
		BlurColumn().Width(wux::GridLength { m_BlurSupported ? 1.0 : 0.0, m_BlurSupported ? wux::GridUnitType::Star : wux::GridUnitType::Pixel });

		if (m_Section >= STATE_COUNT)
		{
			return;
		}

		const auto &appearance = m_Appearances[m_Section];
		bool enabled = true;
		if (const auto optAppearance = appearance.try_as<txmp::OptionalTaskbarAppearance>())
		{
			enabled = optAppearance.Enabled();
		}

		// fades through the panel's OpacityTransition
		const auto options = AppearanceOptions();
		options.Opacity(enabled ? 1.0 : 0.4);
		options.IsHitTestVisible(enabled);
		for (const auto &button : SegmentButtons())
		{
			button.IsTabStop(enabled);
		}
		PeekSwitch().IsEnabled(enabled);
		LineSwitch().IsEnabled(enabled);

		const bool isNormal = appearance.Accent() == txmp::AccentState::Normal;
		ChangeColorButton().IsEnabled(enabled && !isNormal);
		ColorUnavailableText().Visibility(isNormal ? wux::Visibility::Visible : wux::Visibility::Collapsed);

		const auto swatch = ColorSwatch();
		swatch.Background(wux::Media::SolidColorBrush(appearance.Color()));
		swatch.Opacity(isNormal ? 0.4 : 1.0);
	}

	void SettingsPage::UpdateSegmentIndicator(bool animate)
	{
		const auto indicator = SegmentIndicator();
		if (m_Section >= STATE_COUNT)
		{
			return;
		}

		const auto index = static_cast<std::size_t>(m_Appearances[m_Section].Accent());
		const auto buttons = SegmentButtons();
		if (index >= buttons.size())
		{
			indicator.Opacity(0.0);
			return;
		}

		const auto &button = buttons[index];
		if (button.Visibility() == wux::Visibility::Collapsed || button.ActualWidth() <= 0.0)
		{
			indicator.Opacity(0.0);
			return;
		}

		indicator.Opacity(1.0);
		indicator.Width(button.ActualWidth());
		indicator.Height(button.ActualHeight());

		const float x = button.TransformToVisual(SegmentGrid()).TransformPoint({ 0.0f, 0.0f }).X;
		if (animate && m_AnimationsEnabled)
		{
			SpringVector(indicator, L"Translation", { x, 0.0f, 0.0f }, SELECTION_DAMPING, SELECTION_PERIOD);
		}
		else
		{
			SetTranslation(indicator, { x, 0.0f, 0.0f });
		}
	}

	void SettingsPage::UpdateSelectionPill(bool animate)
	{
		const auto buttons = NavButtons();
		const auto &selected = buttons[m_Section];
		if (selected.ActualHeight() <= 0.0)
		{
			return;
		}

		const float y = selected.TransformToVisual(NavList()).TransformPoint({ 0.0f, 0.0f }).Y;
		if (animate && m_AnimationsEnabled)
		{
			SpringVector(SelectionPill(), L"Translation", { 0.0f, y, 0.0f }, SELECTION_DAMPING, SELECTION_PERIOD);
		}
		else
		{
			SetTranslation(SelectionPill(), { 0.0f, y, 0.0f });
		}
	}

	void SettingsPage::CommitSelectedState()
	{
		if (m_Section >= STATE_COUNT)
		{
			return;
		}

		// send a copy, the receiver is free to modify it
		const auto &appearance = m_Appearances[m_Section];
		txmp::TaskbarAppearance copy(nullptr);
		if (const auto optAppearance = appearance.try_as<txmp::OptionalTaskbarAppearance>())
		{
			copy = txmp::OptionalTaskbarAppearance(optAppearance.Enabled(), appearance.Accent(), appearance.Color(), appearance.ShowPeek(), appearance.ShowLine(), appearance.BlurRadius());
		}
		else
		{
			copy = txmp::TaskbarAppearance(appearance.Accent(), appearance.Color(), appearance.ShowPeek(), appearance.ShowLine(), appearance.BlurRadius());
		}

		m_TaskbarSettingsChangedDelegate(static_cast<txmp::TaskbarState>(m_Section), copy);
	}

	void SettingsPage::PlayEntranceAnimation()
	{
		// the system animates the window itself; inside it, the sidebar items settle in one after the other
		const auto buttons = NavButtons();
		for (std::size_t i = 0; i < buttons.size(); ++i)
		{
			const float delay = 0.08f + 0.022f * static_cast<float>(i);
			SetTranslation(buttons[i], { -12.0f, 0.0f, 0.0f });
			SpringVector(buttons[i], L"Translation", { 0.0f, 0.0f, 0.0f }, SECTION_DAMPING, SECTION_PERIOD, delay);
			FadeTo(buttons[i], 0.0f, 1.0f, 0.2f, delay);
		}

		FadeTo(SelectionPill(), 0.0f, 1.0f, 0.2f, 0.08f + 0.022f * static_cast<float>(m_Section));
	}

	void SettingsPage::PlaySectionChangeAnimation()
	{
		// new content slides in a short distance from the side it is coming from
		const auto host = DetailHost();
		SetTranslation(host, { 16.0f, 0.0f, 0.0f });
		SpringVector(host, L"Translation", { 0.0f, 0.0f, 0.0f }, SECTION_DAMPING, SECTION_PERIOD);
		FadeTo(host, 0.25f, 1.0f, 0.2f);
	}
}
