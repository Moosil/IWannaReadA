#pragma once
#include <functional>
#include <WebView2.h>


namespace WebView2 {
	class Impl :
			public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
			public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
		using init_callback = std::function<void(ICoreWebView2Controller*, ICoreWebView2* webview)>;

	private:
		HWND                          hwnd;
		RECT                          extent;
		init_callback                 callback;
		std::atomic<ULONG>            ref_count{1};
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
			RECT          extent,
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

		HRESULT QueryInterface(const IID& riid, LPVOID* ppv) override;
	};

	template<typename T>
	struct CastInfo_t {
		using type = T;
		IID iid;
	};

	static const auto ControllerCompleted =
			CastInfo_t<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>{
				IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
			};

	static const auto EnvCompleted =
			CastInfo_t<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>{
				IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
			};

	template<typename From, typename To>
	To* cast_if_equal_iid(From* from, REFIID riid, const CastInfo_t<To>& info, LPVOID* ppv = nullptr) noexcept {
		To* ptr = nullptr;
		if (IsEqualIID(riid, info.iid)) {
			ptr = static_cast<To*>(from);
			ptr->AddRef();
		}
		if (ppv) {
			*ppv = ptr;
		}
		return ptr;
	}
}
