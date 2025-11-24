#include <memory>
#include <utility>
#include <Shlobj.h>
#pragma comment(lib, "Shell32.lib")

#include "implwebview2.h"

#include <spdlog/spdlog.h>

namespace WebView2 {
	Impl::Impl(
		const HWND&   hwnd,
		const int     width,
		const int     height,
		init_callback callback
	) :
		hwnd{hwnd},
		width{width},
		height{height},
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

		// Resize WebView to fit the bounds of the parent window
		controller->put_Bounds(RECT{0, 0, width, height});
		spdlog::info("put_Bounds called, HRESULT = 0x{:X}", err);

		spdlog::info("callback about to be called");
		callback(controller, webview);
		spdlog::info("callback called, HRESULT = 0x{:X}", err);
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
