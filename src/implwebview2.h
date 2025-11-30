#pragma once

#include <WebView2.h>

#include <coroutine>
#include <functional>


namespace WebView2 {
	class Impl :
			public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
			public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
		using init_callback = std::function<void(ICoreWebView2Controller*, ICoreWebView2* webview)>;

	private:
		HWND                          hwnd;
		RECT                          extent;
		init_callback                 callback;
		ULONG                         ref_count{1};
		unsigned int                  attempts{0};
		static constexpr unsigned int MaxAttempts{50};
		static constexpr int          SleepMs{100};

	public:
		virtual ~Impl() = default;

		Impl(const Impl& other) = delete;

		Impl& operator=(const Impl& other) = delete;

		Impl(Impl&& other) = delete;

		Impl& operator=(Impl&& other) = delete;

		Impl(
			const HWND&   hwnd,
			const RECT&   extent,
			init_callback callback
		);

		ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_count; }

		ULONG STDMETHODCALLTYPE Release() override {
			if (ref_count > 1) {
				return --ref_count;
			}
			delete this;
			return 0;
		}

		void try_init_env();

		HRESULT STDMETHODCALLTYPE Invoke(HRESULT err, ICoreWebView2Environment* env) override;

		HRESULT STDMETHODCALLTYPE Invoke(HRESULT err, ICoreWebView2Controller* controller) override;

		HRESULT QueryInterface(const IID& riid, LPVOID* ppv) noexcept override;
	};

	struct Navigate {
		wil::com_ptr<ICoreWebView2> webview;
		const wchar_t* html;
		EventRegistrationToken token;
		HRESULT result = E_FAIL;

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> h);

		HRESULT await_resume() const noexcept { return result; }
	};

	struct ExecuteScript {
		wil::com_ptr<ICoreWebView2> webview;
		std::wstring script;
		std::wstring resultJson;
		HRESULT result = E_FAIL;

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> h);

		auto await_resume() const {
			return std::tuple{ result, resultJson };
		}
	};

}
