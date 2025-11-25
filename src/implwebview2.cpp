#include <memory>
#include <utility>
#include <Shlobj.h>
#pragma comment(lib, "Shell32.lib")

#include "implwebview2.h"

#include <spdlog/spdlog.h>
#include <wil/com.h>

namespace WebView2 {
	Impl::Impl(
		const HWND&   hwnd,
		const RECT    extent,
		init_callback callback
	) :
		hwnd{hwnd},
		extent{extent},
		callback{std::move(callback)} {
	}


	void Impl::try_init_env() {
		if (attempts < MaxAttempts) {
			attempts++;

			const HRESULT err = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, this);
			spdlog::info("CreateCoreWebView2EnvironmentWithOptions called, HRESULT = 0x{:X}", err);
			if (SUCCEEDED(err)) {
				return;
			}

			if (err == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
				return;
			}

			Sleep(SleepMs);
			return try_init_env();
		}

		callback(nullptr, nullptr);
	}

	HRESULT STDMETHODCALLTYPE Impl::Invoke(HRESULT err, ICoreWebView2Environment* env) {
		spdlog::info("Environment Invoke called, HRESULT = 0x{:X}", err);
		if (SUCCEEDED(err)) {
			spdlog::info("HWND valid? {}, visible? {}", IsWindow(hwnd), IsWindowVisible(hwnd));
			err = env->CreateCoreWebView2Controller(hwnd, this);
			spdlog::info("CreateCoreWebView2Controller returned 0x{:X}", err);

			if (SUCCEEDED(err)) {
				return err;
			}
		}
		try_init_env();
		return err;
	}

	HRESULT STDMETHODCALLTYPE Impl::Invoke(HRESULT err, ICoreWebView2Controller* controller) {
		spdlog::info("Controller Invoke called, HRESULT = 0x{:X}", err);
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

		ICoreWebView2* webview;
		err = controller->get_CoreWebView2(&webview);
		spdlog::info("get_CoreWebView2 called, HRESULT = 0x{:X}", err);

		err = controller->put_Bounds(extent);
		spdlog::info("put_Bounds called, HRESULT = 0x{:X}", err);

		wil::com_ptr<ICoreWebView2Controller2> controller2;
		err = controller->QueryInterface(IID_PPV_ARGS(&controller2));
		spdlog::info("QueryInterface (ICoreWebView2Controller2) called, HRESULT = 0x{:X}", err);

		constexpr COREWEBVIEW2_COLOR color = {255,255,255,255};
		err = controller2->put_DefaultBackgroundColor(color);
		spdlog::info("put_DefaultBackgroundColor called, HRESULT = 0x{:X}", err);

		wil::com_ptr<ICoreWebView2_13> webview13;
		err = webview->QueryInterface(IID_PPV_ARGS(&webview13));
		spdlog::info("QueryInterface (ICoreWebView2_13) called, HRESULT = 0x{:X}", err);

		wil::com_ptr<ICoreWebView2Profile> profile;
		err = webview13->get_Profile(&profile);
		spdlog::info("get_Profile called, HRESULT = 0x{:X}", err);

		err = profile->put_PreferredColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
		spdlog::info("put_PreferredColorScheme called, HRESULT = 0x{:X}", err);

		callback(controller, webview);
		return S_OK;
	}

	HRESULT Impl::QueryInterface(const IID& riid, LPVOID* ppv) {
		spdlog::info("QueryInterface");
		if (!ppv) {
			return E_POINTER;
		}

		if (cast_if_equal_iid(this, riid, ControllerCompleted, ppv)
		    || cast_if_equal_iid(this, riid, EnvCompleted, ppv)) {
			return S_OK;
		}

		return E_NOINTERFACE;
	}
}
