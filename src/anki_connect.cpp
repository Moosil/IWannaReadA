#include <spdlog/spdlog.h>


#include "anki_connect.h"


namespace Anki {
	Interface::Interface(const int port) :
		client{std::make_unique<httplib::Client>("127.0.0.1", port)} {
		//client->set_connection_timeout(0, 500'000); // 500 ms

		spdlog::info("connected AnkiConnect HTTP client to 127.0.0.1:{}", port);
	}

	Interface::~Interface() {
		spdlog::info("disconnected AnkiConnect HTTP client");
	}


	nlohmann::json Interface::get_request_body(const std::string& request_name, const nlohmann::json& params) {
		nlohmann::json req = {
			{"action", request_name},
			{"version", ankiconnect_version}
		};
		if (!params.is_null()) {
			req["params"] = params;
		}
		return req;
	}

	nlohmann::json Interface::get_find_note_request(const std::string& query) {
		auto params = nlohmann::json::object();
		params["query"] = query;
		return get_request_body("findNotes", params);
	}

	nlohmann::json Interface::get_card_info_request(const CardID note_id) {
		return get_card_info_request(std::vector{note_id});
	}

	nlohmann::json Interface::get_card_info_request(const std::vector<CardID>& note_id) {
		auto params = nlohmann::json::object();
		params["cards"] = note_id;
		return get_request_body("cardsInfo", params);
	}

	nlohmann::json Interface::get_update_note_field_request(
		const CardID                              note_id,
		const std::map<std::string, std::string>& fields
	) {
		auto note = nlohmann::json::object();
		note["id"] = note_id;
		note["fields"] = fields;
		auto params = nlohmann::json::object();
		params["note"] = note;
		return get_request_body("updateNoteFields", params);
	}

	void Interface::add_note(
		const std::string& hanyu,
		const std::string& pinyin,
		const std::string& definition,
		const std::string& sentence
	) const {
		spdlog::info("[AnkiConnect] attempting to add card: {} | {} | {} | {}", hanyu, pinyin, definition, sentence);
		const nlohmann::json find_note_request = get_find_note_request("deck:chinese_read_text hanyu:" + hanyu);
		const httplib::Result find_note_result = post_and_receive(find_note_request);
		if (!find_note_result) {
			spdlog::error("[AnkiConnect] findNote failed: HTTP {}", to_string(find_note_result.error()));
			return;
		}

		const nlohmann::json find_note_json = get_response_json(find_note_result);
		if (!find_note_json["error"].is_null()) {
			spdlog::error("[AnkiConnect] findNote failed: {}", find_note_json["error"].get<std::string>());
			return;
		}

		if (find_note_json["result"].empty()) {
			nlohmann::json add_node_request = get_add_node_request(
				"chinese_read_text",
				"chinese_read_text_card",
				{{"hanyu", hanyu}, {"pinyin", pinyin}, {"definition", definition}, {"sentence", sentence}}
			);
			const httplib::Result add_node_result = post_and_receive(add_node_request);
			if (!add_node_result) {
				spdlog::error("[AnkiConnect] addNote failed: HTTP {}", to_string(add_node_result.error()));
				return;
			}

			if (const nlohmann::json add_node_json = get_response_json(add_node_result); !add_node_json["error"].is_null()) {
				spdlog::error("[AnkiConnect] addNote failed: {}", add_node_json["error"].get<std::string>());
			}
		} else {
			const CardID note_id = find_note_json["result"][0].get<CardID>();

			const nlohmann::json                    card_info_request = get_card_info_request(note_id);
			const httplib::Result card_info_result  = post_and_receive(card_info_request);
			if (!card_info_result) {
				spdlog::error("[AnkiConnect] getCardInfo failed: HTTP {}", to_string(find_note_result.error()));
				return;
			}

			const nlohmann::json card_info_json = get_response_json(card_info_result);
			if (!card_info_json["error"].is_null()) {
				spdlog::error("[AnkiConnect] getCardInfo failed: {}", card_info_json["error"].get<std::string>());
				return;
			}

			const std::string old_sentence = card_info_json["result"][0]["fields"]["sentence"]["value"].get<std::string>();

			if (!old_sentence.contains(sentence)) {
				nlohmann::json update_note_field_request = get_update_note_field_request(note_id, {{"sentence", old_sentence + "<br>" + sentence}});
				const httplib::Result update_note_field_result = post_and_receive(update_note_field_request);
				if (!update_note_field_result) {
					spdlog::error("[AnkiConnect] updateNoteFields failed: HTTP {}", to_string(update_note_field_result.error()));
					return;
				}

				if (const nlohmann::json update_note_field_json = get_response_json(update_note_field_result); !update_note_field_json["error"].is_null()) {
					spdlog::error("[AnkiConnect] updateNoteFields failed: {}", update_note_field_json["error"].get<std::string>());
				}
			}
		}
	}

	inline nlohmann::json Interface::get_add_node_request(
		const std::string&                        deck_name,
		const std::string&                        card_type,
		const std::map<std::string, std::string>& fields
	) {
		auto note = nlohmann::json::object();
		note["deckName"] = deck_name;
		note["modelName"] = card_type;
		note["fields"] = fields;
		auto params = nlohmann::json::object();
		params["note"] = note;
		return get_request_body("addNote", params);
	}

	inline nlohmann::json Interface::get_multi_request(const std::vector<nlohmann::json>& requests) {
		auto params = nlohmann::json::object();
		params["actions"] = requests;
		return get_request_body("multi", params);
	}

	inline nlohmann::json Interface::get_response_json(const httplib::Result& response) {
		std::string body = response->body;
		return nlohmann::json::parse(body);
	}

	httplib::Result Interface::post_and_receive(const std::string& request) const {
		return client->Post("/", request, "application/json");
	}

	inline httplib::Result Interface::post_and_receive(const nlohmann::json& json) const {
		const auto str = json.dump();
		return post_and_receive(str);
	}
}
