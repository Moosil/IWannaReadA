#include "anki_connect.h"

#include <utility>
#include <spdlog/spdlog.h>

namespace iwra {
	AnkiInterface::~AnkiInterface() {
		spdlog::info("disconnected AnkiConnect HTTP client");
	}

	AnkiInterface::AnkiInterface(const std::shared_ptr<Config>& config):
		port{config->getAnkiPort()},
		client{"127.0.0.1", port},
		config{config} {
		client.set_connection_timeout(config->getAnkiConnectionTimeoutMs());
	}

	nlohmann::json AnkiInterface::getRequestBody(const std::string& request_name, const nlohmann::json& params) {
		nlohmann::json req = {
			{"action", request_name},
			{"version", ankiconnect_version}
		};
		if (!params.is_null()) {
			req["params"] = params;
		}
		return req;
	}

	nlohmann::json AnkiInterface::getFindNoteRequest(const std::string& query) {
		auto params     = nlohmann::json::object();
		params["query"] = query;
		return getRequestBody("findNotes", params);
	}

	nlohmann::json AnkiInterface::getCardInfoRequest(const CardID note_id) {
		return getCardInfoRequest(std::vector{note_id});
	}

	nlohmann::json AnkiInterface::getCardInfoRequest(const std::vector<CardID>& note_id) {
		auto params     = nlohmann::json::object();
		params["cards"] = note_id;
		return getRequestBody("cardsInfo", params);
	}

	nlohmann::json AnkiInterface::getUpdateNoteFieldRequest(
		const CardID                              note_id,
		const std::map<std::string, std::string>& note_fields
	) {
		auto note      = nlohmann::json::object();
		note["id"]     = note_id;
		note["fields"] = note_fields;
		auto params    = nlohmann::json::object();
		params["note"] = note;
		return getRequestBody("updateNoteFields", params);
	}

	inline nlohmann::json AnkiInterface::getAddNodeRequest(
		const std::string&                        deck_name,
		const std::string&                        note_type,
		const std::map<std::string, std::string>& note_fields,
		const std::vector<std::string>&           tags
	) {
		auto note                 = nlohmann::json::object();
		note["deckName"]          = deck_name;
		note["modelName"]         = note_type;
		note["fields"]            = note_fields;
		auto options              = nlohmann::json::object();
		options["allowDuplicate"] = true;
		auto tags_json            = nlohmann::json::array();
		for (const auto& tag : tags) {
			tags_json.push_back(tag);
		}
		auto params       = nlohmann::json::object();
		params["note"]    = note;
		params["options"] = options;
		params["tags"]    = tags_json;
		return getRequestBody("addNote", params);
	}

	inline nlohmann::json AnkiInterface::getMultiRequest(const std::vector<nlohmann::json>& requests) {
		auto params       = nlohmann::json::object();
		params["actions"] = requests;
		return getRequestBody("multi", params);
	}

	inline nlohmann::json AnkiInterface::getResponseJson(const httplib::Result& response) {
		std::string body = response->body;
		return nlohmann::json::parse(body);
	}

	std::string AnkiInterface::formatString(
		const std::string& input,
		const std::string& simp,
		const std::string& trad,
		const std::string& pinyin,
		const std::string& definition,
		const std::string& phrase,
		const std::string& sentence,
		const std::string& cloze_sentence) {
		std::string res = fmt::format(
			fmt::runtime(input),
			fmt::arg("simp", simp),
			fmt::arg("trad", trad),
			fmt::arg("pinyin", pinyin),
			fmt::arg("definition", definition),
			fmt::arg("phrase", phrase),
			fmt::arg("sentence", sentence),
			fmt::arg("cloze_sentence", cloze_sentence)
		);

		size_t pos = 0;
		while ((pos = res.find_first_of("{}", pos)) != std::string::npos) {
			if (res[pos] == '{') {
				res.replace(pos, 1, "{{");
			} else {
				res.replace(pos, 1, "}}");
			}
			pos += 2;
		}
		return res;
	}

	void AnkiInterface::checkConnection() {
		spdlog::info("[AnkiConnect] checking connection to 127.0.0.1:{}", port);
		if (!connected) {
			requestPermission();
		}

		if (requires_api_key && !config->getAnkiAPIKey().has_value()) {
			spdlog::error("[AnkiConnect] Anki api key required, but no api key supplied");
		}
	}

	bool AnkiInterface::addNote(
		const std::map<std::string, std::string>& field_values
	) {
		checkConnection();
		if (!connected || (requires_api_key && !config->getAnkiAPIKey().has_value())) {
			return false;
		}

		const std::string deck_name = config->getAnkiDeckName().value();
		const std::string note_type = config->getAnkiNoteType().value();

		const nlohmann::json add_node_request = getAddNodeRequest(
			deck_name,
			note_type,
			field_values,
			config->getAnkiTags().value_or({})
		);
		const httplib::Result add_node_result = postAndReceive(add_node_request);
		if (!add_node_result) {
			spdlog::error("[AnkiConnect] addNote failed: HTTP {}", to_string(add_node_result.error()));
			return false;
		}

		if (const nlohmann::json add_node_json = getResponseJson(add_node_result);
			!add_node_json["error"].is_null()) {
			spdlog::error("[AnkiConnect] addNote failed: {}", add_node_json["error"].get<std::string>());
		}

		return true;
	}

	std::optional<std::vector<std::string> > AnkiInterface::getNoteTypeFieldNames(const std::string& note_type) {
		auto params                                    = nlohmann::json::object();
		params["modelName"]                            = note_type;
		const auto            note_type_fields_request = getRequestBody("modelFieldNames", params);
		const httplib::Result field_result             = postAndReceive(note_type_fields_request);
		if (!field_result) {
			spdlog::error("[AnkiConnect] modelFieldNames failed: HTTP {}", to_string(field_result.error()));
			return std::nullopt;
		}

		const nlohmann::json field_json = getResponseJson(field_result);
		if (!field_json["error"].is_null()) {
			spdlog::error("[AnkiConnect] modelFieldNames failed: {}", field_json["error"].get<std::string>());
			return std::nullopt;
		}

		return field_json["result"].get<std::vector<std::string> >();
	}

	std::optional<std::vector<std::string> > AnkiInterface::getNoteTypeFieldDescriptors(const std::string& note_type) {
		const nlohmann::json  params                   = nlohmann::json::object({"modelName", note_type});
		const nlohmann::json  note_type_fields_request = getRequestBody("modelFieldDescriptions", {params});
		const httplib::Result field_result             = postAndReceive(note_type_fields_request);
		if (!field_result) {
			spdlog::error("[AnkiConnect] modelFieldDescriptions failed: HTTP {}", to_string(field_result.error()));
			return std::nullopt;
		}

		const nlohmann::json field_json = getResponseJson(field_result);
		if (!field_json["error"].is_null()) {
			spdlog::error("[AnkiConnect] modelFieldDescriptions failed: {}", field_json["error"].get<std::string>());
			return std::nullopt;
		}

		return field_json["result"].get<std::vector<std::string> >();
	}

	void AnkiInterface::fillConfigNoteFields() {
		std::optional field_names_opt = getNoteTypeFieldNames(config->getAnkiNoteType().value());
		if (!field_names_opt.has_value()) {
			return;
		}

		std::optional field_descriptors_opt = getNoteTypeFieldDescriptors(config->getAnkiNoteType().value());


		config->fillAnkiNoteFields(
			std::views::iota(std::size_t{0}, field_names_opt.value().size()) | std::views::transform(
				[&field_names_opt, &field_descriptors_opt](const std::size_t i) {
					return std::make_pair(
						field_names_opt.value()[i],
						(field_descriptors_opt.has_value()) ? field_descriptors_opt.value()[i] : ""
					);
				}
			) | std::ranges::to<std::unordered_map<std::string, std::string> >()
		);
	}

	void AnkiInterface::requestPermission() {
		spdlog::info("[AnkiConnect] requesting API access");
		const nlohmann::json  permission_request = getRequestBody("requestPermission");
		const httplib::Result permission_result  = postAndReceive(permission_request);
		if (!permission_result) {
			spdlog::error("[AnkiConnect] requestPermission failed: HTTP {}", to_string(permission_result.error()));
			return;
		}

		const nlohmann::json permission_json = getResponseJson(permission_result);
		if (!permission_json["error"].is_null()) {
			spdlog::error("[AnkiConnect] requestPermission failed: {}", permission_json["error"].get<std::string>());
			return;
		}

		if (permission_json["permission"].get<std::string>() == "denied") {
			spdlog::error("[AnkiConnect] API access denied");
			connected = false;
			return;
		}
		connected = true;

		if (permission_json["requireApiKey"].get<bool>()) {
			requires_api_key = true;
			spdlog::info("[AnkiConnect] API access requires API key");
		} else {
			requires_api_key = false;
		}
	}

	httplib::Result AnkiInterface::postAndReceive(const std::string& request) {
		spdlog::info("[AnkiConnect] posting: {}", request);
		return client.Post("/", request, "application/json");
	}

	httplib::Result AnkiInterface::postAndReceive(const nlohmann::json& json) {
		const auto str = json.dump();
		return postAndReceive(str);
	}
}
