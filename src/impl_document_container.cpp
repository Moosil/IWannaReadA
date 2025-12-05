#include "impl_document_container.h"

#include <d2d1helper.h>
#include <dwrite.h>

#include "tooltip.h"
#include "log.h"
#include "util.h"

namespace ocr {
	using namespace litehtml;

	HTMLDocument::HTMLDocument(
		TooltipWnd&                                          wnd,
		const Microsoft::WRL::ComPtr<IDWriteFactory>&        p_direct_write_factory,
		const Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>& p_render_target
	) :
		window{wnd} {
		initDirectWrite(p_direct_write_factory, p_render_target);
	}

	uint_ptr HTMLDocument::create_font(
		const char*   faceName,
		int           size,
		int           weight,
		font_style    italic,
		unsigned      decoration,
		font_metrics* fm
	) {
		std::string s_face_name = faceName;
		if (s_face_name[0] == '\"' && s_face_name[s_face_name.size() - 1] == '\"') {
			s_face_name = s_face_name.substr(1, s_face_name.size() - 2);
		}

		const std::string key = s_face_name + "|" + std::to_string(size) + "|" +
					  std::to_string(weight) + "|" + std::to_string(italic) + "|" +
					  std::to_string(decoration);

		const auto it = font_cache.find(key);
		if (it != font_cache.end()) {
			fm->height      = it->second.height;
			fm->ascent      = it->second.ascent;
			fm->descent     = it->second.descent;
			fm->draw_spaces = true;
			fm->x_height    = it->second.x_height;
			return it->second.font_ptr;
		}


		std::wstring w_face_name        = utf8ToWide(s_face_name);
		const auto   d_write_font_style = italic == font_style_italic
		                                ? DWRITE_FONT_STYLE_ITALIC
		                                : DWRITE_FONT_STYLE_NORMAL;

		Microsoft::WRL::ComPtr<IDWriteFontCollection> font_collection = nullptr;
		UINT32                                        family_index    = -1;
		BOOL                                          exists          = FALSE;
		Microsoft::WRL::ComPtr<IDWriteFontFamily>     font_family     = nullptr;
		Microsoft::WRL::ComPtr<IDWriteFont>           font            = nullptr;
		DWRITE_FONT_METRICS                           font_metrics{};

		HRESULT err = direct_write_factory->GetSystemFontCollection(font_collection.GetAddressOf());
		log(err, "IDWriteFactory::GetSystemFontCollection", ERR_LEVEL::FATAL);

		err = font_collection->FindFamilyName(w_face_name.c_str(), &family_index, &exists);
		log(err, "IDWriteFontCollection::FindFamilyName", ERR_LEVEL::FATAL);

		if (!exists) {
			spdlog::error("Font family \"{}\" not found", s_face_name);
			w_face_name = L"Calibri";

			family_index = -1;
			exists       = FALSE;

			err = font_collection->FindFamilyName(w_face_name.c_str(), &family_index, &exists);
			log(err, "IDWriteFontCollection::FindFamilyName", ERR_LEVEL::FATAL);
		}
		err = font_collection->GetFontFamily(family_index, font_family.GetAddressOf());
		log(err, "IDWriteFontCollection::GetFontFamily", ERR_LEVEL::FATAL);

		err = font_family->GetFirstMatchingFont(
			static_cast<DWRITE_FONT_WEIGHT>(weight),
			DWRITE_FONT_STRETCH_NORMAL,
			d_write_font_style,
			font.GetAddressOf()
		);
		log(err, "IDWriteFontFamily::GetFirstMatchingFont", ERR_LEVEL::FATAL);

		font->GetMetrics(&font_metrics);

		uint_ptr                                  ptr = ++max_font;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> text_format;
		err = direct_write_factory->CreateTextFormat(
			w_face_name.c_str(),
			nullptr,
			static_cast<DWRITE_FONT_WEIGHT>(weight),
			d_write_font_style,
			DWRITE_FONT_STRETCH_NORMAL,
			static_cast<float>(size),
			L"zh-CN",
			text_format.GetAddressOf()
		);
		log(err, "IDWriteFactory::CreateTextFormat", ERR_LEVEL::FATAL);

		fm->height      = font_metrics.lineGap + font_metrics.ascent + font_metrics.descent;
		fm->ascent      = font_metrics.ascent;
		fm->descent     = font_metrics.descent;
		fm->draw_spaces = true;
		fm->x_height    = font_metrics.xHeight;

		fonts.emplace(ptr, Font{std::move(text_format), static_cast<Decoration>(decoration)});
		font_cache.insert({key, {
			ptr, fm->height, fm->ascent, fm->descent, fm->x_height
		}});
		return ptr;
	}

	void HTMLDocument::delete_font(uint_ptr hFont) {
		// pass
	}

	int HTMLDocument::text_width(const char* text, uint_ptr hFont) {
		loadFont(hFont);
		const std::wstring wstring = utf8ToWide(text);
		return static_cast<int>(std::round(
			getTextSize(wstring, fonts.at(curr_font).format, direct_write_factory).first
		));
	}

	void HTMLDocument::draw_text(
		uint_ptr        hdc,
		const char*     text,
		uint_ptr        hFont,
		web_color       color,
		const position& pos
		) {
		const auto render_target = reinterpret_cast<ID2D1HwndRenderTarget*>(hdc);
		if (!render_target) {
			return;
		}

		loadFont(hFont);

		const std::wstring wstring = utf8ToWide(text);
		const LayoutKey    key{hFont, wstring};

		Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
		auto                                      it = layout_cache.find(key);
		if (it != layout_cache.end()) {
			text_layout = it->second;
		} else {
			const auto  text_length                    = static_cast<UINT32>(wstring.length());
			const auto& [font_format, font_decoration] = fonts.at(curr_font);
			const auto  [width, height]                = getTextSize(wstring, font_format, direct_write_factory);
			HRESULT     err                            = direct_write_factory->CreateTextLayout(
				wstring.c_str(),
				text_length,
				font_format.Get(),
				width,
				height,
				text_layout.GetAddressOf()
			);
			log(err, "IDWriteFactory::CreateTextLayout", ERR_LEVEL::FATAL);

			switch (font_decoration) {
				case Decoration::none: {
					break;
				}
				case Decoration::Underline: {
					err = text_layout->SetUnderline(true, {0, text_length});
					log(err, "IDWriteTextLayout::SetUnderline", ERR_LEVEL::FATAL);
					break;
				}
				case Decoration::Strikethrough: {
					err = text_layout->SetStrikethrough(true, {0, text_length});
					log(err, "IDWriteTextLayout::SetStrikethrough", ERR_LEVEL::FATAL);
					break;
				}
				case Decoration::Overline: {
					break; //TODO implement
				}
			}

			layout_cache.emplace(std::move(key), text_layout);
		}

		setBrushColour(color);

		render_target->DrawTextLayout(
			{static_cast<float>(pos.x), static_cast<float>(pos.y)},
			text_layout.Get(),
			brush.Get()
			);
	}

	int HTMLDocument::pt_to_px(int pt) const {
		return 72 * pt / getMonitorDPI().first;
	}

	int HTMLDocument::get_default_font_size() const {
		return 24;
	}

	const char* HTMLDocument::get_default_font_name() const {
		return "Calibri"; //TODO placeholder
	}

	void HTMLDocument::draw_list_marker(uint_ptr hdc, const list_marker& marker) {
	}

	void HTMLDocument::load_image(const char* src, const char* baseurl, bool redraw_on_ready) {
		// pass
	}

	void HTMLDocument::get_image_size(const char* src, const char* baseurl, size& sz) {
		// pass
	}

	void HTMLDocument::draw_background(uint_ptr hdc, const std::vector<background_paint>& bg) {
		const auto render_target = reinterpret_cast<ID2D1HwndRenderTarget*>(hdc);
		if (!render_target) {
			return;
		}

		for (const auto& layer : bg) {
			D2D1_RECT_F rect;
			if (layer.is_root) {
				// For root background, fill entire viewport / render target
				auto [width, height] = render_target->GetSize();
				rect                 = D2D1::RectF(0, 0, width, height);
			} else {
				rect = D2D1::RectF(
					static_cast<FLOAT>(layer.clip_box.left()),
					static_cast<FLOAT>(layer.clip_box.top()),
					static_cast<FLOAT>(layer.clip_box.right()),
					static_cast<FLOAT>(layer.clip_box.bottom())
				);
			}

			setBrushColour(layer.color);

			D2D1_LAYER_PARAMETERS layerParams{};
			layerParams.contentBounds = rect;
			layerParams.opacity       = 1.0f;
			if (layer.color.alpha != 255) {
				render_target->PushLayer(&layerParams, nullptr);
			}

			const bool has_radius =
					layer.border_radius.top_left_x != 0 ||
					layer.border_radius.top_left_y != 0 ||
					layer.border_radius.top_right_x != 0 ||
					layer.border_radius.top_right_y != 0 ||
					layer.border_radius.bottom_right_x != 0 ||
					layer.border_radius.bottom_right_y != 0 ||
					layer.border_radius.bottom_left_x != 0 ||
					layer.border_radius.bottom_left_y != 0;

			if (has_radius) {
				const auto radius_x = static_cast<float>(layer.border_radius.top_left_x);
				const auto radius_y = static_cast<float>(layer.border_radius.top_left_y);

				// Fill the rounded rectangle
				render_target->FillRoundedRectangle({rect, radius_x, radius_y}, brush.Get());
			} else {
				// Simple rectangle fill
				render_target->FillRectangle(rect, brush.Get());
			}
			if (layer.color.alpha != 255) {
				render_target->PopLayer();
			}
		}
	}

	void HTMLDocument::draw_borders(
		uint_ptr        hdc,
		const borders&  borders,
		const position& draw_pos,
		bool            root
	) {
	}

	void HTMLDocument::set_caption(const char* caption) {
		// pass
	}

	void HTMLDocument::set_base_url(const char* p_base_url) {
		// pass
	}

	void HTMLDocument::link(const std::shared_ptr<document>& doc, const element::ptr& el) {
		// pass
	}

	void HTMLDocument::on_anchor_click(const char* url, const element::ptr& el) {
		// pass
	}

	void HTMLDocument::set_cursor(const char* cursor) {
		// pass
	}

	void HTMLDocument::transform_text(string& text, text_transform tt) {
		switch (tt) {
			case text_transform_none: {
				break;
			}
			case text_transform_capitalize: {
				text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
				break;
			}
			case text_transform_uppercase: {
				std::ranges::transform(
					text,
					text.begin(),
					[](const unsigned char c) { return std::toupper(c); }
				);
				break;
			}
			case text_transform_lowercase: {
				std::ranges::transform(
					text,
					text.begin(),
					[](const unsigned char c) { return std::tolower(c); }
				);
				break;
			}
		}
	}

	void HTMLDocument::import_css(string& text, const string& url, string& baseurl) {
		// pass
	}

	void HTMLDocument::set_clip(const position& pos, const border_radiuses& bdr_radius) {
		// TODO
	}

	void HTMLDocument::del_clip() {
		// TODO
	}

	void HTMLDocument::get_client_rect(position& client) const {
		const auto [width, height] = window.getExtent();
		client.x                   = 0;
		client.y                   = 0;
		client.width               = width;
		client.height              = height;
	}

	element::ptr HTMLDocument::create_element(
		const char*                      tag_name,
		const string_map&                attributes,
		const std::shared_ptr<document>& doc
	) {
		return nullptr;
	}

	void HTMLDocument::get_media_features(media_features& media) const {
		const auto [window_width, window_height]   = window.getExtent();
		const auto [monitor_width, monitor_height] = getScreenSize();

		HDC hdc            = GetDC(nullptr);
		int bits_per_pixel = GetDeviceCaps(hdc, BITSPIXEL);
		int planes         = GetDeviceCaps(hdc, PLANES);

		media.width         = window_width;
		media.height        = window_height;
		media.device_width  = monitor_width;
		media.device_height = monitor_height;
		media.resolution    = getMonitorDPI().first;
		media.type          = media_type_screen;
		media.color         = planes == 1 ? 0 : bits_per_pixel / planes;
		media.monochrome    = planes == 1 ? bits_per_pixel : 0;
		media.color_index   = 0;

		ReleaseDC(nullptr, hdc);
	}

	void HTMLDocument::get_language(string& language, string& culture) const {
		language = "en";
		culture  = "au";
	}

	void HTMLDocument::initDirectWrite(
		const Microsoft::WRL::ComPtr<IDWriteFactory>&        p_direct_write_factory,
		const Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>& p_render_target
	) {
		direct_write_factory = p_direct_write_factory;

		HRESULT err = p_render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.GetAddressOf());
		log(err, "ID2D1HwndRenderTarget::CreateSolidColorBrush", ERR_LEVEL::FATAL);
	}

	void HTMLDocument::loadFont(litehtml::uint_ptr hFont) {
		curr_font = hFont;
	}

	void HTMLDocument::setBrushColour(const litehtml::web_color color) const {
		brush->SetColor(
			{
				static_cast<float>(color.red) / 255.f,
				static_cast<float>(color.green) / 255.f,
				static_cast<float>(color.blue) / 255.f,
				static_cast<float>(color.alpha) / 255.f,
			}
		);
	}
}
