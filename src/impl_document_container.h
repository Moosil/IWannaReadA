#pragma once

#include <d2d1.h>
#include <litehtml.h>
#include <wrl/client.h>

#include "tooltip.h"

struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct IDWriteTextFormat;
struct IDWriteFactory;


namespace ocr {
	class TooltipWnd;

	class HTMLDocument final : public litehtml::document_container {
	public:
		struct LayoutKey {
			litehtml::uint_ptr font;
			std::wstring text;

			bool operator==(const LayoutKey& other) const {
				return font == other.font && text == other.text;
			}
		};

		struct LayoutKeyHash {
			std::size_t operator()(const LayoutKey& k) const {
				return std::hash<litehtml::uint_ptr>()(k.font) ^ std::hash<std::wstring>()(k.text);
			}
		};

		struct FontCacheItem {
			litehtml::uint_ptr font_ptr;
			int height;
			int ascent;
			int descent;
			int x_height;
		};

		enum class Decoration {
			none = 0,
			Underline,
			Strikethrough,
			Overline
		};

		struct Font {
			Microsoft::WRL::ComPtr<IDWriteTextFormat> format = nullptr;
			Decoration decoration;
		};

		virtual ~HTMLDocument() = default;

		HTMLDocument(
			TooltipWnd&                                        wnd,
			const Microsoft::WRL::ComPtr<IDWriteFactory>&      p_direct_write_factory,
			const Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>& p_render_target
		);

		litehtml::uint_ptr create_font(
			const char* faceName,
			int size,
			int weight,
			litehtml::font_style italic,
			unsigned decoration,
			litehtml::font_metrics* fm
		) override;

		void delete_font(litehtml::uint_ptr hFont) override;

		int text_width(const char* text, litehtml::uint_ptr hFont) override;

		void draw_text(
			litehtml::uint_ptr hdc,
			const char* text,
			litehtml::uint_ptr hFont,
			litehtml::web_color color,
			const litehtml::position& pos
		) override;

		int pt_to_px(int pt) const override;

		int get_default_font_size() const override;

		const char* get_default_font_name() const override;

		void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;

		void load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;

		void get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;

		void draw_background(litehtml::uint_ptr hdc, const std::vector<litehtml::background_paint>& bg) override;

		void draw_borders(
			litehtml::uint_ptr hdc,
			const litehtml::borders& borders,
			const litehtml::position& draw_pos,
			bool root
		) override;

		void set_caption(const char* caption) override;

		void set_base_url(const char* p_base_url) override;

		void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override;

		void on_anchor_click(const char* url, const litehtml::element::ptr& el) override;

		void set_cursor(const char* cursor) override;

		void transform_text(litehtml::string& text, litehtml::text_transform tt) override;

		void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override;

		void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;

		void del_clip() override;

		void get_client_rect(litehtml::position& client) const override;

		litehtml::element::ptr create_element(
			const char* tag_name,
			const litehtml::string_map& attributes,
			const std::shared_ptr<litehtml::document>& doc
		) override;

		void get_media_features(litehtml::media_features& media) const override;

		void get_language(litehtml::string& language, litehtml::string& culture) const override;
	private:
		litehtml::uint_ptr max_font = -1;
		litehtml::uint_ptr curr_font = -1;
		std::unordered_map<litehtml::uint_ptr, Font> fonts;
		std::unordered_map<std::string, FontCacheItem> font_cache;
		std::unordered_map<LayoutKey, Microsoft::WRL::ComPtr<IDWriteTextLayout>, LayoutKeyHash> layout_cache;

		TooltipWnd&                                  window;
		Microsoft::WRL::ComPtr<IDWriteFactory>       direct_write_factory = nullptr;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush                = nullptr;

		void initDirectWrite(
			const Microsoft::WRL::ComPtr<IDWriteFactory>& p_direct_write_factory,
			const Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>& p_render_target
		);

		void loadFont(litehtml::uint_ptr hFont);

		void setBrushColour(litehtml::web_color color) const;
	};
}
