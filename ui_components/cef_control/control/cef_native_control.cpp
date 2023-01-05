#include "stdafx.h"
#include "cef_native_control.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_runnable.h"
#include "cef_control/handler/browser_handler.h"
#include "cef_control/manager/cef_manager.h"

namespace nim_comp {

CefNativeControl::CefNativeControl(void)
{

}

CefNativeControl::~CefNativeControl(void)
{
	if (browser_handler_.get() && browser_handler_->GetBrowser().get())
	{
		//½â¾öÁËmulti_browserÄÚ´æÐ¹Â©µÄÎÊÌâ
		//https://github.com/xmcy0011/NIM_Duilib_Framework/commit/4d7efc1bcd13181c95d8b6082c1816bdee9c0509
		auto hwnd = GetCefHandle();
		DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
		// ÒòÎªReCreateBrowserÖÐÊ¹ÓÃÁËSetAsChild£¬¶øBrowserÓÖÊÇ¸ù¾Ý½ÓÊÕWM_CLOSEÏûÏ¢À´¹Ø±ÕÊÍ·ÅµÄ
		// ËùÒÔÕâÀïÍË³öÇ°£¬ÐèÒª¸ü¸ÄÒ»ÏÂ¸¸´°¿Ú£¬·ñÔò»áÔì³ÉÄÚ´æÐ¹Â©»òÕßÖ»ÓÐÕû¸öMultiBrowserFormÍË³öÊ±²ÅÊÍ·ÅÄÚ´æ¡£
		if (dwStyle & WS_CHILD)
			::SetParent(hwnd, GetDesktopWindow());

		// Request that the main browser close.
		browser_handler_->GetBrowserHost()->CloseBrowser(true);
		browser_handler_->SetHostWindow(nullptr);
		browser_handler_->SetHandlerDelegate(nullptr);
		browser_handler_ = nullptr;
	}	
}

void CefNativeControl::Init()
{
	if (browser_handler_.get() == nullptr)
	{
		LONG style = GetWindowLong(m_pWindow->GetHWND(), GWL_STYLE);
		SetWindowLong(m_pWindow->GetHWND(), GWL_STYLE, style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		ASSERT((GetWindowExStyle(m_pWindow->GetHWND()) & WS_EX_LAYERED) == 0 && L"æ— æ³•åœ¨åˆ†å±‚çª—å£å†…ä½¿ç”¨æœ¬æŽ§ä»¶");

		browser_handler_ = new nim_comp::BrowserHandler;
		browser_handler_->SetHostWindow(m_pWindow->GetHWND());
		browser_handler_->SetHandlerDelegate(this);
		ReCreateBrowser();
	}

	if (!js_bridge_.get())
	{
		js_bridge_.reset(new nim_comp::CefJSBridge);
	}

	__super::Init();
}

void CefNativeControl::ReCreateBrowser()
{
	if (browser_handler_->GetBrowser() == nullptr)
	{
		// ä½¿ç”¨æœ‰çª—æ¨¡å¼
		CefWindowInfo window_info;
		window_info.SetAsChild(this->m_pWindow->GetHWND(), m_rcItem);

		CefBrowserSettings browser_settings;
		CefBrowserHost::CreateBrowser(window_info, browser_handler_, L"", browser_settings, NULL);
	}
}

void CefNativeControl::SetPos(UiRect rc)
{
	__super::SetPos(rc);

	HWND hwnd = GetCefHandle();
	if (hwnd) 
	{
		SetWindowPos(hwnd, HWND_TOP, rc.left, rc.top, rc.GetWidth(), rc.GetHeight(), SWP_NOZORDER);
	}
}

void CefNativeControl::HandleMessage(EventArgs& event)
{
	if (browser_handler_.get() && browser_handler_->GetBrowser().get() == NULL)
		return __super::HandleMessage(event);

	else if (event.Type == kEventInternalSetFocus)
	{
		browser_handler_->GetBrowserHost()->SetFocus(true);
	}
	else if (event.Type == kEventInternalKillFocus)
	{
		browser_handler_->GetBrowserHost()->SetFocus(false);
	}

	__super::HandleMessage(event);
}

void CefNativeControl::SetVisible(bool bVisible /*= true*/)
{
	__super::SetVisible(bVisible);
	
	HWND hwnd = GetCefHandle();
	if (hwnd)
	{
		if (bVisible)
		{
			ShowWindow(hwnd, SW_SHOW);
		}
		else
		{
			SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		}
	}
}

void CefNativeControl::SetInternVisible(bool bVisible)
{
	__super::SetInternVisible(bVisible);

	HWND hwnd = GetCefHandle();
	if (hwnd)
	{
		if (bVisible)
		{
			ShowWindow(hwnd, SW_SHOW);
		}
		else
		{
			SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
		}
	}
}

void CefNativeControl::SetWindow(ui::Window* pManager, ui::Box* pParent, bool bInit)
{
	if (browser_handler_)
		browser_handler_->SetHostWindow(pManager->GetHWND());

	// è®¾ç½®Cefçª—å£å¥æŸ„ä¸ºæ–°çš„ä¸»çª—å£çš„å­çª—å£
	auto hwnd = GetCefHandle();
	if (hwnd)
		SetParent(hwnd, pManager->GetHWND());

	// ä¸ºæ–°çš„ä¸»çª—å£é‡æ–°è®¾ç½®WS_CLIPSIBLINGSã€WS_CLIPCHILDRENæ ·å¼ï¼Œå¦åˆ™Cefçª—å£åˆ·æ–°ä¼šå‡ºé—®é¢˜
	LONG style = GetWindowLong(pManager->GetHWND(), GWL_STYLE);
	SetWindowLong(pManager->GetHWND(), GWL_STYLE, style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	__super::SetWindow(pManager, pParent, bInit);
}

bool CefNativeControl::AttachDevTools(Control* /*view*/)
{
	if (devtool_attached_)
		return true;

	auto browser = browser_handler_->GetBrowser();
	if (browser == nullptr)
	{
		auto task = ToWeakCallback([this]()
		{
			nbase::ThreadManager::PostTask(kThreadUI, ToWeakCallback([this](){
				AttachDevTools(nullptr);
			}));
		});

		browser_handler_->AddAfterCreateTask(task);
	}
	else
	{
		CefWindowInfo windowInfo;
		windowInfo.SetAsPopup(NULL, L"cef_devtools");
		CefBrowserSettings settings;
		windowInfo.width = 900;
		windowInfo.height = 700;
		browser->GetHost()->ShowDevTools(windowInfo, new nim_comp::BrowserHandler, settings, CefPoint());
		devtool_attached_ = true;
		if (cb_devtool_visible_change_ != nullptr)
			cb_devtool_visible_change_(devtool_attached_);
	}
	return true;
}

}