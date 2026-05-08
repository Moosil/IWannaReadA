#include "tooltip.h"


#include <clip.h>
#include <filesystem>
#include <ranges>
#include <WebView2.h>
#include <Windowsx.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <wrl/event.h>

#include "dict_parser.h"
#include "log.h"
#include "util.h"
#include "util_ocr.h"
#include "util_text.h"
#include "util_utf8.h"


namespace iwra {
	TooltipWnd::TooltipWnd(
		const std::vector<OCRResult>& res,
		const cv::Rect& rect,
		const std::filesystem::path& webpage_path,
		const std::shared_ptr<DictParser>& parser,
		const std::shared_ptr<Anki::Interface>& anki
	): rect{rect}, parser{parser}, anki{anki} {
		processOCRResults(res, cv::Point{rect.x, rect.y}, results);

		constexpr int style           = WS_POPUP;
		constexpr int extended_styles = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT;

		hwnd = CreateWindowEx(
			extended_styles,
			className.c_str(),
			"tt",
			style,
			0,
			0,
			width,
			height,
			nullptr,
			nullptr,
			GetModuleHandle(nullptr),
			this
		);

		webpage_html = readFile(webpage_path);

		log(SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE), "SetWindowDisplayAffinity");
	}

	void TooltipWnd::initWebView2() {
		HRESULT err = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		log(err, "CoInitializeEx", ERR_LEVEL::FATAL);
		wchar_t* version;
		err = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
		log(err, "GetAvailableCoreWebView2BrowserVersionString", ERR_LEVEL::WARN);
		if (SUCCEEDED(err)) {
			CoTaskMemFree(version);
		}
		wv_init = std::make_unique<WebView2::Impl>(
			hwnd,
			RECT{0, 0, width, height},
			[this](ICoreWebView2Controller* controller, ICoreWebView2* wv) {
				if (!controller || !wv) {
					return;
				}
				wv_controller = controller;
				webview       = wv;

				const HRESULT nav_subscribe_err = webview->add_NavigationCompleted(
					Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
						[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
							inited_web_view2 = true;
							onNavigationComplete();
							return S_OK;
						}
					).Get(),
					nullptr
				);
				log(nav_subscribe_err, "ICoreWebView2::add_NavigationCompleted", ERR_LEVEL::WARN);

				const HRESULT nav_err = webview->NavigateToString(
					utf8ToWide(webpage_html).c_str()
				);
				log(nav_err, "ICoreWebView2::NavigateToString", ERR_LEVEL::FATAL);

				if (!is_hovering) {
					ShowWindow(hwnd, SW_HIDE);
				}
			}
		);
		wv_init->try_init_env();
	}

	bool TooltipWnd::initCurrDict() {
		spdlog::info("inited dict of {}", current_word->text);
		std::string       lookup_string;
		const std::string first_char = current_word->text;
		dictionary_data[first_char]  = {};
		for (auto curr = current_word; curr != current_block->results.end()._Ptr; ++curr) {
			lookup_string += curr->text;
			if (auto dict_entry = parser->get_entry(lookup_string);
				!dict_entry.empty()) {
				spdlog::info("loading entry: {}", lookup_string);
				dictionary_data[first_char].entries.append_range(dict_entry);
				dictionary_data[first_char].phrase = lookup_string;
			}
		};
		std::ranges::reverse(dictionary_data[first_char].entries);
		return !dictionary_data[first_char].entries.empty();
	}

	std::vector<OCRResultPacked> TooltipWnd::ocrSplitText(const Poly2I& rect, const Text& text, const bool horizontal) {
		auto                         it  = text.text.begin();
		const auto                   end = text.text.end();
		std::vector<OCRResultPacked> out{};
		out.reserve(text.char_lengths.size());
		for (const auto& [lower, upper] : text.char_lengths) {
			std::string utf16i;
			try {
				utf8::append(utf8::next(it, end), utf16i);
			} catch (const utf8::not_enough_room& _) {
				continue;
			}
			Poly2I char_rect;
			if (horizontal) {
				char_rect = {
					cv::Point{static_cast<int>(std::lerp(rect[0].x, rect[1].x, lower)), rect[0].y},
					cv::Point{static_cast<int>(std::lerp(rect[0].x, rect[1].x, upper)), rect[1].y},
					cv::Point{static_cast<int>(std::lerp(rect[3].x, rect[2].x, upper)), rect[2].y},
					cv::Point{static_cast<int>(std::lerp(rect[3].x, rect[2].x, lower)), rect[3].y}
				};
			} else {
				char_rect = {
					cv::Point{rect[0].x, static_cast<int>(std::lerp(rect[0].y, rect[3].y, lower))},
					cv::Point{rect[1].x, static_cast<int>(std::lerp(rect[1].y, rect[2].y, lower))},
					cv::Point{rect[2].x, static_cast<int>(std::lerp(rect[1].y, rect[2].y, upper))},
					cv::Point{rect[3].x, static_cast<int>(std::lerp(rect[0].y, rect[3].y, upper))}
				};
			}
			out.emplace_back(char_rect, utf16i);
		}

		return std::move(out);
	}

	void TooltipWnd::processOCRResults(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft,
		std::vector<OCRBlock>&        out
	) {
		out.clear();
		out.reserve(res.size());
		for (const auto& [rect, text] : res) {
			const Poly2I moved_rect = {
				rect.rect[0] + topleft,
				rect.rect[1] + topleft,
				rect.rect[2] + topleft,
				rect.rect[3] + topleft,
			};
			const std::array xs            = {rect.rect[0].x, rect.rect[1].x, rect.rect[2].x, rect.rect[3].x};
			const std::array ys            = {rect.rect[0].y, rect.rect[1].y, rect.rect[2].y, rect.rect[3].y};
			const auto       [left, right] = std::minmax_element(xs.begin(), xs.end());
			const auto       [top, bottom] = std::minmax_element(ys.begin(), ys.end());
			bool             horizontal    = (*right - *left) > (*bottom - *top);

			auto split_line = ocrSplitText(moved_rect, text, horizontal);

			std::vector<int> intersections = std::views::iota(std::size_t{0}, out.size())
			                                 | std::views::filter(
				                                 [out, &moved_rect](const std::size_t i) -> int {
					                                 return out[i].horizontal && intersects(out[i].poly, moved_rect);
				                                 }
			                                 )
			                                 | std::ranges::to<std::vector<int> >();
			if (intersections.empty()) {
				std::vector<cv::Point> poly{};
				poly.append_range(moved_rect);
				out.emplace_back(split_line, poly, horizontal);
			} else {
				if (horizontal) {
					std::ranges::reverse(intersections);
					OCRBlock& first = out[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = out[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						out.erase(out.begin() + intersections[i]);
					}
				} else {
					OCRBlock& first = out[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = out[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						out.erase(out.begin() + intersections[i]);
					}
				}
			}
		}
	}

	void TooltipWnd::updateWindowPosition() const {
		if (!current_word) {
			return;
		}

		const auto [screen_width, screen_height] = getScreenSize();

		int top;
		// choose above or below hover word based on which haas more room
		if (const int room_left_top = getTop(current_word->rect);
			screen_height - getBottom(current_word->rect) > room_left_top) {
			// window is too tall (it goes above top of screen)
			top = getBottom(current_word->rect) + height;
		} else {
			// window can extend up and is below screen
			top = room_left_top;
		}
		int left;
		if (getRight(current_word->rect) + width > screen_width) {
			// window is too width (it goes past right of screen)
			left = screen_width - width;
		} else {
			// window can extend right and is left of screen edge
			left = getLeft(current_word->rect);
		}
		SetWindowPos(hwnd, HWND_TOPMOST, left, top - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	const DictionaryData* TooltipWnd::getDictDataOrInit(const std::string& key) {
		const auto it = dictionary_data.find(key);
		if (it == dictionary_data.end()) {
			if (!initCurrDict()) {
				return nullptr;
			}
			return &dictionary_data.at(key);
		}

		if (it->second.entries.empty()) {
			if (!initCurrDict()) {
				return nullptr;
			}
			return &dictionary_data.at(key);
		}
		return &it->second;
	}

	void TooltipWnd::updateWindowEntry(const DictionaryData* dict_data, const std::string& phrase, const std::string& sentence) const {
		nlohmann::json page_data = nlohmann::json::array();
		for (const auto& entry : dict_data->entries) {
			// if entry.simp is a prefix of the hovered phrase
			if (!phrase.starts_with(entry.get_simp()) && !phrase.starts_with(entry.get_trad())) {
				spdlog::warn("{} doesn't begin with {} or {}, thus discarding", phrase, entry.get_simp(), entry.get_trad());
				continue;
			}

			nlohmann::json def_json = nlohmann::json::array();
			for (const auto& def : entry.definitions) {
				def_json.push_back(def);
			}

			nlohmann::json words = nlohmann::json::array();
			for (const auto& [characters] : entry.word) {
				nlohmann::json word_json = nlohmann::json::array();
				for (const auto& [simp, trad, pinyin] : characters) {
					nlohmann::json char_json = nlohmann::json::object();
					char_json["simp"]        = simp;
					char_json["trad"]        = trad;
					char_json["pinyin"]      = pinyin;
					word_json.push_back(char_json);
				}
				words.push_back(word_json);
			}

			nlohmann::json entry_json = nlohmann::json::object();
			entry_json["words"] = words;
			entry_json["def"]         = def_json;
			entry_json["c_word"]      = dict_data->phrase;
			entry_json["c_sent"]      = sentence;
			page_data.push_back(entry_json);
		}
		const std::string page_data_str = page_data.dump();
		const std::string script        = "setPage(" + page_data_str + ")";
		const auto        script_utf16  = utf8::utf8to16(script);

		// spdlog::info("script: {}", script);

		const HRESULT err = webview->ExecuteScript(
			utf8ToWide(script).c_str(),
			Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				// ReSharper disable once CppParameterMayBeConst
				[](HRESULT errorCode0, LPCWSTR resultObjectAsJson) -> HRESULT {
					log(errorCode0, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
					return S_OK;
				}
			).Get()
		);
		log(err, "ICoreWebView2::ExecuteScript", ERR_LEVEL::FATAL);

		updateWindowPosition();
	}

	std::vector<std::string> splitHanzi(const std::string& hanzi, const std::string& pinyin) {
		std::string post_pinyin = pinyin+' ';
		std::vector<std::size_t> lengths;
		std::size_t prev = 0;
		for (std::size_t it = post_pinyin.find(' '); it != std::string::npos; prev = it, it = post_pinyin.find(' ', it+1)) {
			if (const int number = post_pinyin[it-1] - '0';
				1 < number && number > 5) {
				post_pinyin.erase(it, 1);
				it -= 1;
				if (it > 0) {
					if (const int prev_number = post_pinyin[it-1] - '0';
						1 < prev_number && prev_number > 5) {
						lengths.back() += it-prev;
						continue;
					}
				}
				lengths.push_back(it-prev);
			} else {
				lengths.push_back(1);
			}
		}

		std::vector<std::string> res;

		std::size_t total_len = 0;
		const std::u16string hanzi_u16 = utf8::utf8to16(hanzi);
		for (const auto length : lengths) {
			res.push_back(utf8::utf16to8(hanzi_u16.substr(total_len, length)));
			total_len += length;
		}
		return res;
	}

	void TooltipWnd::refreshWindow() {
		if (!inited_web_view2) {
			return;
		}

		if (!current_word) {
			return;
		}

		if (current_phrase == current_word->text) {
			return;
		}

		const auto* dict_data = getDictDataOrInit(current_word->text);
		if (!dict_data) {
			return;
		}

		current_phrase = current_word->text;

		const std::string phrase = getPhrase(current_word, current_block);
		const std::string sentence = getSentence(current_block);

		updateWindowEntry(dict_data, phrase, sentence);
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	HRESULT TooltipWnd::onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
		wchar_t*      json_string;
		const HRESULT err = args->get_WebMessageAsJson(&json_string);
		log(err, "ICoreWebView2WebMessageReceivedEventArgs::get_WebMessageAsJson", ERR_LEVEL::WARN);

		nlohmann::json json = nlohmann::json::parse(wideToUtf8(json_string));
		if (const auto it = json.find("key"); it != json.end()) {
			if (it.value() == "contextmenu") {
				createContextMenu(
					json.at("x"),
					json.at("y"),
					json.at("character"),
					json.at("word"),
					json.at("pinyin"),
					json.at("sentence"),
					json.at("definition")
				);
			} else if (it.value() == "ankiadd") {
				addAnkiCard(
					json.at("character"),
					json.at("word"),
					json.at("pinyin"),
					json.at("sentence"),
					json.at("definition")
				);
			} else if (it.value() == "changepage") {
				const auto* dict_data = getDictDataOrInit(json.at("to"));
				updateWindowEntry(dict_data, "", "");
			} else {
				//mousedown
			}
		}
		return S_OK;
	}

	void TooltipWnd::onNavigationComplete() {
		EventRegistrationToken token       = {};
		const HRESULT          add_msg_err = webview->add_WebMessageReceived(
			Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
				this,
				&TooltipWnd::onWebMessageReceived
			).Get(),
			&token
		);
		log(add_msg_err, "ICoreWebView2::add_WebMessageReceived", ERR_LEVEL::WARN);
	}

	void TooltipWnd::addAnkiCard(
		const std::string& character,
		const std::string& phrase,
		const std::string& pinyin,
		const std::string& sentence,
		const std::string& definition
	) const {
		auto        [find_pos_first, find_pos_second]     = utf8Find(sentence, character);
		const std::string sentence_add = std::format(
			"{}{{{{c1::{}}}}}{}",
			std::string(sentence.begin(), find_pos_first),
			phrase,
			std::string(find_pos_second, sentence.end())
		);

		anki->add_note(phrase, pinyin, definition, sentence_add);
	}


	void TooltipWnd::createContextMenu(
		const int          x,
		const int          y,
		const std::string& character,
		const std::string& phrase,
		const std::string& pinyin,
		const std::string& sentence,
		const std::string& definition
	) const {
		const HMENU& menu = CreatePopupMenu();
		AppendMenu(menu, MF_STRING, 1, "Copy character");
		AppendMenu(menu, MF_STRING, 2, "Copy phrase");
		AppendMenu(menu, MF_STRING, 3, "Copy sentence");
		AppendMenu(menu, MF_STRING, 4, "Add to Anki");
		const int selected = TrackPopupMenu(
			menu,
			TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NOANIMATION,
			x,
			y,
			0,
			hwnd,
			nullptr
		);
		switch (selected) {
			case 1: {
				clip::set_text(character);
				break;
			}
			case 2: {
				clip::set_text(phrase);
				break;
			}
			case 3: {
				clip::set_text(sentence);
				break;
			}
			case 4: {
				addAnkiCard(character, phrase, pinyin, sentence, definition);
			}
			default:
				break;
		}
		DestroyMenu(menu);
	}

	std::string TooltipWnd::getSentence(OCRBlock* hover_block) {
		return hover_block->results
		       | std::ranges::views::transform(
			       [](auto& r) -> std::string& { return r.text; }
		       )
		       | std::views::join | std::ranges::to<std::string>();
	}

	std::string TooltipWnd::getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block) {
		std::string result;
		for (const auto* curr = hover_word; curr != hover_block->results.end()._Ptr; ++curr) {
			result += curr->text;
		}
		return result;
	}

	LRESULT TooltipWnd::wndProc(const UINT msg, const WPARAM wparam, const LPARAM lparam) {
		switch (msg) {
			case WM_CREATE: {
				if (!inited_web_view2) {
					ShowWindow(hwnd, SW_SHOWNOACTIVATE);
					initWebView2();
				}
				break;
			}
			// case WM_KEYDOWN: {
			// 	break;
			// }
			case WM_CONTEXTMENU: {
				const auto x = GET_X_LPARAM(lparam);
				const int  y = GET_Y_LPARAM(lparam);
				if (current_word && current_block) {
					createContextMenu(x, y, current_word->text, current_word->text, ":(", getSentence(current_block), ":(");
				}
				break;
			}
			case WM_SIZE: {
				if (wv_controller) {
					RECT rc;
					GetClientRect(hwnd, &rc);
					const HRESULT err = wv_controller->put_Bounds(rc);
					log(err, "ICoreWebView2Controller.put_Bounds", ERR_LEVEL::WARN);
				};
				break;
			}
			case WM_PAINT: {
				if (is_hovering) {
					PAINTSTRUCT ps;
					BeginPaint(hwnd, &ps);

					EndPaint(hwnd, &ps);
					return 0;
				}
				break;
			}
			case WM_DESTROY: {
				break;
			}
			case WM_NCDESTROY: {
				SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
				break;
			}
			default: {
			};
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	// ReSharper disable CppParameterMayBeConst
	LRESULT CALLBACK TooltipWnd::wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		// ReSharper restore CppParameterMayBeConst
		TooltipWnd* self;

		if (msg == WM_NCCREATE) {
			const auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
			self          = static_cast<TooltipWnd*>(cs->lpCreateParams);
			self->hwnd    = hwnd;
			SetLastError(0);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			if (GetLastError() != 0) {
				return false;
			}
		} else {
			self = reinterpret_cast<TooltipWnd*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (self) {
			return self->wndProc(msg, wparam, lparam);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	std::unique_ptr<TooltipWnd> TooltipWnd::initTooltip(
		const std::vector<OCRResult>& res,
		const cv::Rect&               rect,
		const std::filesystem::path&  webpage_path,
		const std::shared_ptr<DictParser>& parser,
		const std::shared_ptr<Anki::Interface>& anki
	) {

		if (!isInitialised) {
			WNDCLASS wc{};
			wc.lpfnWndProc   = &wndProcSetup;
			wc.hInstance     = GetModuleHandle(nullptr);
			wc.lpszClassName = className.c_str();
			wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW));
			if (!RegisterClassA(&wc)) {
				spdlog::error("registering window class failed");
				return nullptr;
			}
			isInitialised = true;
		}

		return std::make_unique<TooltipWnd>(res, rect, webpage_path, parser, anki);
	}

	// 40ms
	void TooltipWnd::updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		processOCRResults(new_res, cv::Point{rect.x, rect.y}, results);

		rect        = new_rect;
		current_block = nullptr;
		current_word  = nullptr;
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			refreshHovering();
			if (is_hovering) {
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(hwnd);
			} else {
				if (inited_web_view2) {
					ShowWindow(hwnd, SW_HIDE);
				}
				UpdateWindow(hwnd);
			}
		}
	}

	void TooltipWnd::refreshHovering() {
		POINT win_mouse_pos;
		GetCursorPos(&win_mouse_pos);

		const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};

		if (results.empty()) {
			is_hovering = false;
			return;
		}

		// if mouse is in the captured rect
		if (!rect.contains(mouse_pos)) {
			is_hovering = false;
			return;
		}

		// caching previous rect (optimisation)
		if (is_hovering) {
			if (current_word && !current_word->rect.empty() && cv::pointPolygonTest(
				current_word->rect,
				mouse_pos,
				false
			) > 0) {
				return;
			}
		}

		const auto intersect_iter = std::ranges::find_if(
			results,
			[&mouse_pos](const OCRBlock& block) -> bool {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				if (block.poly.empty())
					return false;
				return cv::pointPolygonTest(block.poly, mouse_pos, false) > 0;
			}
		);

		// mouse isn't in any of the OCR areas
		if (intersect_iter == results.end()) {
			is_hovering = false;
			return;
		}

		const auto word_iter = std::ranges::find_if(
			intersect_iter->results,
			[&mouse_pos](const OCRResultPacked& res) -> bool {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				return cv::pointPolygonTest(res.rect, mouse_pos, false) > 0;
			}
		);

		if (word_iter == intersect_iter->results.end()) {
			is_hovering = false;
			return;
		}

		is_hovering  = true;
		need_refresh = true;
		current_word   = word_iter._Ptr;
		current_block  = intersect_iter._Ptr;
	}

	void TooltipWnd::updateLoop() {
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			refreshHovering();
			if (is_hovering) {
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(hwnd);
			} else {
				if (inited_web_view2) {
					ShowWindow(hwnd, SW_HIDE);
				}
				UpdateWindow(hwnd);
			}
		}
		if (need_refresh && is_hovering) {
			refreshWindow();
			need_refresh = false;
		}

		MSG msg;
		while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	HWND TooltipWnd::getHwnd() const {
		return hwnd;
	}
}
