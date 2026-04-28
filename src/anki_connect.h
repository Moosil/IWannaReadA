#pragma once

#include <future>
#include <nlohmann/json.hpp>
#include <httplib.h>

namespace Anki {
	class Interface {
	public:
		Interface(std::string deck_name, std::string card_type, int port = 8765);

		~Interface();

		Interface& operator=(const Interface&) = delete;
		Interface(const Interface&) = delete;

		Interface& operator=(Interface&& other) noexcept {
			if (this != &other) {
				client = std::move(other.client);
				deck_name = std::move(other.deck_name);
				card_type = std::move(other.card_type);
			}
			return *this;
		};
		Interface(Interface&& other) noexcept:
			client{std::move(other.client)},
			deck_name{other.deck_name},
			card_type{other.card_type} {
		};

		static nlohmann::json get_request_body(const std::string& request_name, const nlohmann::json& params = nullptr);
		;

		static nlohmann::json get_find_note_request(const std::string& query);

		static nlohmann::json get_card_info_request(unsigned long long note_id);

		static nlohmann::json get_card_info_request(const std::vector<unsigned long long>& note_id);

		static nlohmann::json get_update_note_field_request(unsigned long long note_id, const std::map<std::string, std::string>& fields);

		void add_note(const std::string& hanyu, const std::string& pinyin, const std::string& definition, const std::string& sentence) const;

		static nlohmann::json get_add_node_request(const std::string& deck_name, const std::string& card_type, const std::map<std::string, std::string>& fields);

		static nlohmann::json get_multi_request(const std::vector<nlohmann::json>& requests);

		static nlohmann::json get_response_json(const httplib::Result& response);

		[[nodiscard]] [[maybe_unused]] httplib::Result post_and_receive(const std::string& request) const;

		[[nodiscard]]  [[maybe_unused]] httplib::Result post_and_receive(const nlohmann::json& json) const;
	private:
		using CardID = unsigned long long;

		std::unique_ptr<httplib::Client> client;

		std::string deck_name;
		std::string card_type;

		static constexpr int ankiconnect_version = 6;
	};
}