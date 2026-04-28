#include <memory>
#include <Shlobj.h>
#include <utility>
#pragma comment(lib, "Shell32.lib")

#include <spdlog/spdlog.h>
#include <wil/com.h>
#include <wrl/event.h>

#include "implwebview2.h"
#include "log.h"

namespace WebView2 {
	Impl::Impl(
		const HWND&   hwnd,
		const RECT&    extent,
		init_callback callback
	) :
		hwnd{hwnd},
		extent{extent},
		callback{std::move(callback)} {
	}


	void Impl::try_init_env() {
		while (attempts < MaxAttempts) {

			const HRESULT err = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, this);
			iwra::log(err, "CreateCoreWebView2EnvironmentWithOptions", iwra::ERR_LEVEL::WARN);
			if (SUCCEEDED(err) || err == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
				return;
			}

			Sleep(SleepMs);
			attempts++;
		}

		spdlog::error("Failed to initialise WebView2!");
		callback(nullptr, nullptr);
	}

	HRESULT STDMETHODCALLTYPE Impl::Invoke(HRESULT err, ICoreWebView2Environment* env) {
		iwra::log(err, "ICoreWebView2Environment::Invoke", iwra::ERR_LEVEL::WARN);
		if (SUCCEEDED(err)) {
			err = env->CreateCoreWebView2Controller(hwnd, this);
			iwra::log(err, "ICoreWebView2Environment::CreateCoreWebView2Controller", iwra::ERR_LEVEL::WARN);

			if (SUCCEEDED(err)) {
				return err;
			}
		}
		spdlog::info("HWND valid? {}, visible? {}", IsWindow(hwnd), IsWindowVisible(hwnd));
		try_init_env();
		return err;
	}

	HRESULT STDMETHODCALLTYPE Impl::Invoke(HRESULT err, ICoreWebView2Controller* controller) {
		iwra::log(err, "ICoreWebView2Controller::Invoke", iwra::ERR_LEVEL::WARN);
		if (FAILED(err)) {
			if (err == E_ABORT) {
				spdlog::info("Controller Invoke: E_ABORT");
				return S_OK;
			}
			if (err == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
				// pass
				spdlog::info("Controller Invoke: ERROR_INVALID_STATE");
			}
			try_init_env();
			return S_OK;
		}

		wil::com_ptr<ICoreWebView2> webview;
		err = controller->get_CoreWebView2(&webview);
		iwra::log(err, "ICoreWebView2Controller::get_CoreWebView2", iwra::ERR_LEVEL::FATAL);

		err = controller->put_Bounds(extent);
		iwra::log(err, "ICoreWebView2Controller::put_Bounds", iwra::ERR_LEVEL::WARN);

		wil::com_ptr<ICoreWebView2_13> webview13;
		err = webview->QueryInterface(IID_PPV_ARGS(&webview13));
		iwra::log(err, "ICoreWebView2::QueryInterface", iwra::ERR_LEVEL::WARN);

		if (SUCCEEDED(err)) {
			wil::com_ptr<ICoreWebView2Profile> profile;
			err = webview13->get_Profile(&profile);
			iwra::log(err, "ICoreWebView2_13::get_Profile", iwra::ERR_LEVEL::WARN);

			if (SUCCEEDED(err)) {
				err = profile->put_PreferredColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
				iwra::log(err, "ICoreWebView2Profile::put_PreferredColorScheme", iwra::ERR_LEVEL::WARN);
			}
		}

		wil::com_ptr<ICoreWebView2Settings> settings;
		err = webview->get_Settings(&settings);
		iwra::log(err, "ICoreWebView2::get_Settings", iwra::ERR_LEVEL::WARN);
		err = settings->put_AreDefaultContextMenusEnabled(false);
		iwra::log(err, "ICoreWebView2Settings::put_AreDefaultContextMenusEnabled", iwra::ERR_LEVEL::WARN);

		callback(controller, webview.get());
		return S_OK;
	}

	HRESULT Impl::QueryInterface(const IID& riid, LPVOID* ppv) noexcept {
		spdlog::info("QueryInterface");
		if (!ppv) {
			return E_POINTER;
		}
		*ppv = nullptr;

		if (IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
			*ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
			AddRef();
			return S_OK;
		}

		if (IsEqualIID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
			*ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
			AddRef();
			return S_OK;
		}


		return E_NOINTERFACE;
	}
}
