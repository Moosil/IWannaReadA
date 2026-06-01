#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "config.h"

namespace iwra {
	class AnkiInterface {
	public:
		~AnkiInterface();

		AnkiInterface& operator=(const AnkiInterface&) = delete;

		AnkiInterface(const AnkiInterface&) = delete;

		AnkiInterface& operator=(AnkiInterface&& other) noexcept {
			if (this != &other) {
				client = std::move(other.client);
				config = std::move(other.config);
			}
			return *this;
		}

		AnkiInterface(AnkiInterface&& other) noexcept:
			client{std::move(other.client)},
			config{std::move(other.config)} {}

		explicit AnkiInterface(const std::shared_ptr<Config>& config);

		static nlohmann::json getRequestBody(const std::string& request_name, const nlohmann::json& params = nullptr);

		static nlohmann::json getFindNoteRequest(const std::string& query);

		static nlohmann::json getCardInfoRequest(unsigned long long note_id);

		static nlohmann::json getCardInfoRequest(const std::vector<unsigned long long>& note_id);

		static nlohmann::json getUpdateNoteFieldRequest(
			unsigned long long                        note_id,
			const std::map<std::string, std::string>& fields
		);

		static nlohmann::json getAddNodeRequest(
			const std::string&                        deck_name,
			const std::string&                        card_type,
			const std::map<std::string, std::string>& fields
		);

		static nlohmann::json getMultiRequest(const std::vector<nlohmann::json>& requests);

		static nlohmann::json getResponseJson(const httplib::Result& response);

		void checkConnection();

		bool addNote(
			const std::string& hanyu,
			const std::string& pinyin,
			const std::string& definition,
			const std::string& sentence
		);

		void requestPermission();

		[[nodiscard]] bool getConnected() const {
			return connected;
		}

		// ReSharper disable once CppInconsistentNaming
		[[nodiscard]] bool requiresAPIKey() const {
			return requires_api_key;
		}

		[[nodiscard]] [[maybe_unused]] httplib::Result postAndReceive(const std::string& request);

		[[nodiscard]] [[maybe_unused]] httplib::Result postAndReceive(const nlohmann::json& json);

	private:
		// ReSharper disable once CppInconsistentNaming
		using CardID = unsigned long long;

		int port;

		httplib::Client client;

		std::shared_ptr<Config> config;

		bool connected{false};

		bool requires_api_key{false};

		static constexpr int ankiconnect_version = 6;
	};
}
