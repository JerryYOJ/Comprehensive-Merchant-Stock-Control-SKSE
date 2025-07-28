#pragma once

#include "json.hpp"
#include <string>
#include <vector>
#include <map>
#include <random>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <boost/algorithm/string.hpp>

struct LocalForm {
	RE::FormID localID;
	std::string modname;

	operator RE::FormID() const {
		if (localID >= 0xFF000000) return localID;
		return RE::TESDataHandler::GetSingleton()->LookupFormID(localID, modname);
	}

	RE::FormID Get() const { if (localID >= 0xFF000000) return localID;  return RE::TESDataHandler::GetSingleton()->LookupFormID(localID, modname); }
};

// Structure to represent value ranges (e.g., "2.0~2.5" or "2.0")
struct ValueRange {
	float min;
	float max;
	bool isRange;

	ValueRange() : min(1.0f), max(1.0f), isRange(false) {}
	
	ValueRange(const std::string& valueStr) {
		ParseValue(valueStr);
	}

	void ParseValue(const std::string& valueStr) {
		logger::trace("Parsing value string: '{}'", valueStr);
		size_t tildePos = valueStr.find('~');
		if (tildePos != std::string::npos) {
			// Range value
			isRange = true;
			min = std::stof(valueStr.substr(0, tildePos));
			max = std::stof(valueStr.substr(tildePos + 1));
			logger::debug("Parsed range value: {} ~ {} (min: {}, max: {})", min, max, min, max);
		} else {
			// Fixed value
			isRange = false;
			min = max = std::stof(valueStr);
			logger::debug("Parsed fixed value: {}", min);
		}
	}

	float GetRandomValue() const {
		if (!isRange) {
			logger::trace("Returning fixed value: {}", min);
			return min;
		}
		
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(min, max);
		float result = dis(gen);
		logger::trace("Generated random value {} from range [{}, {}]", result, min, max);
		return result;
	}

	float GetValue() const {
		float result = isRange ? GetRandomValue() : min;
		logger::trace("GetValue() returning: {}", result);
		return result;
	}
};

// Structure for comparison operations (>, <, =, >=, <=)
struct ComparisonFilter {
	enum ComparisonType { NONE, GREATER, LESS, EQUAL, GREATER_EQUAL, LESS_EQUAL };
	ComparisonType type;
	float value;

	ComparisonFilter() : type(NONE), value(0.0f) {}
	
	ComparisonFilter(const std::string& filterStr) {
		ParseFilter(filterStr);
	}

	void ParseFilter(const std::string& filterStr) {
		logger::trace("Parsing comparison filter: '{}'", filterStr);
		if (filterStr == "NONE" || filterStr.empty()) {
			type = NONE;
			logger::debug("Comparison filter set to NONE");
			return;
		}

		if (filterStr.find(">=") != std::string::npos) {
			type = GREATER_EQUAL;
			value = std::stof(filterStr.substr(2));
			logger::debug("Parsed GREATER_EQUAL filter with value: {}", value);
		} else if (filterStr.find("<=") != std::string::npos) {
			type = LESS_EQUAL;
			value = std::stof(filterStr.substr(2));
			logger::debug("Parsed LESS_EQUAL filter with value: {}", value);
		} else if (filterStr.find(">") != std::string::npos) {
			type = GREATER;
			value = std::stof(filterStr.substr(1));
			logger::debug("Parsed GREATER filter with value: {}", value);
		} else if (filterStr.find("<") != std::string::npos) {
			type = LESS;
			value = std::stof(filterStr.substr(1));
			logger::debug("Parsed LESS filter with value: {}", value);
		} else if (filterStr.find("=") != std::string::npos) {
			type = EQUAL;
			value = std::stof(filterStr.substr(1));
			logger::debug("Parsed EQUAL filter with value: {}", value);
		} else {
			type = NONE;
			logger::debug("Could not parse comparison filter, set to NONE");
		}
	}

	bool Matches(float testValue) const {
		bool result;
		switch (type) {
			case GREATER: 
				result = testValue > value;
				logger::trace("Comparison {} > {}: {}", testValue, value, result);
				return result;
			case LESS: 
				result = testValue < value;
				logger::trace("Comparison {} < {}: {}", testValue, value, result);
				return result;
			case EQUAL: 
				result = testValue == value;
				logger::trace("Comparison {} == {}: {}", testValue, value, result);
				return result;
			case GREATER_EQUAL: 
				result = testValue >= value;
				logger::trace("Comparison {} >= {}: {}", testValue, value, result);
				return result;
			case LESS_EQUAL: 
				result = testValue <= value;
				logger::trace("Comparison {} <= {}: {}", testValue, value, result);
				return result;
			case NONE: 
				logger::trace("Comparison filter is NONE, returning true");
				return true;
			default: 
				logger::trace("Unknown comparison type, returning true");
				return true;
		}
	}
};

// Structure for globals' expression
struct GlobalsFilter {
	std::string globalEditorID;
	ComparisonFilter againstValue;

	GlobalsFilter() : globalEditorID(""), againstValue() {}

	GlobalsFilter(const std::string& filterStr) {
		ParseFilter(filterStr);
	}

	void ParseFilter(const std::string& filterStr) {
		logger::trace("Parsing globals filter: '{}'", filterStr);
		if (filterStr.empty() || filterStr == "NONE") {
			globalEditorID = "";
			againstValue.type = ComparisonFilter::NONE;
			logger::debug("Globals filter set to NONE");
			return;
		}

		// Find comparison operators in order of precedence (>= and <= first, then >, <, =)
		size_t opPos = std::string::npos;
		std::string opStr;
		
		if ((opPos = filterStr.find(">=")) != std::string::npos) {
			opStr = ">=";
		} else if ((opPos = filterStr.find("<=")) != std::string::npos) {
			opStr = "<=";
		} else if ((opPos = filterStr.find(">")) != std::string::npos) {
			opStr = ">";
		} else if ((opPos = filterStr.find("<")) != std::string::npos) {
			opStr = "<";
		} else if ((opPos = filterStr.find("=")) != std::string::npos) {
			opStr = "=";
		}
		
		if (opPos != std::string::npos) {
			// Extract the global editor ID (trim whitespace)
			globalEditorID = filterStr.substr(0, opPos);
			TrimString(globalEditorID);
			
			// Extract the value part and parse it
			std::string valueStr = filterStr.substr(opPos + opStr.length());
			TrimString(valueStr);
			
			// Parse the comparison filter
			againstValue.ParseFilter(opStr + valueStr);
			logger::debug("Parsed globals filter - ID: '{}', operator: '{}', value: {}", 
						globalEditorID, opStr, againstValue.value);
		} else {
			// No operator found, treat as just a global name check
			globalEditorID = filterStr;
			TrimString(globalEditorID);
			againstValue.type = ComparisonFilter::NONE;
			logger::debug("Parsed globals filter - ID only: '{}'", globalEditorID);
		}
	}

private:
	void TrimString(std::string& str) {
		// Remove leading whitespace
		str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
		
		// Remove trailing whitespace
		str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), str.end());
	}
};

// Structure for item filters
struct ItemFilter {
	std::string formEditorID;
	std::string keywordEditorID;
	ComparisonFilter weightFilter;
	ComparisonFilter valueFilter;

	ItemFilter() = default;
	
	ItemFilter(const std::string& filterStr) {
		ParseFilter(filterStr);
	}

	void ParseFilter(const std::string& filterStr) {
		logger::trace("Parsing item filter: '{}'", filterStr);
		std::vector<std::string> parts = SplitString(filterStr, "|");
		if (parts.size() >= 4) {
			formEditorID = (parts[0] == "NONE") ? "" : parts[0];
			keywordEditorID = (parts[1] == "NONE") ? "" : parts[1];
			weightFilter.ParseFilter(parts[2]);
			valueFilter.ParseFilter(parts[3]);
			logger::debug("Parsed item filter - Form: '{}', Keyword: '{}', Weight filter parsed, Value filter parsed", 
						formEditorID, keywordEditorID);
		} else {
			logger::warn("Item filter has insufficient parts ({}), expected at least 4", parts.size());
		}
	}

private:
	std::vector<std::string> SplitString(const std::string& str, const char* delimiter) {
		std::vector<std::string> result;
		boost::split(result, str, boost::is_any_of(delimiter));
		return result;
	}
};

// Structure for merchant filters
struct MerchantFilter {
	std::string formEditorID;
	ComparisonFilter relationship; // 0-8, 4 is default
	GlobalsFilter globalCondition;

	MerchantFilter() {}
	
	MerchantFilter(const std::string& filterStr) {
		ParseFilter(filterStr);
	}

	void ParseFilter(const std::string& filterStr) {
		logger::trace("Parsing merchant filter: '{}'", filterStr);
		std::vector<std::string> parts = SplitString(filterStr, "|");
		if (parts.size() >= 3) {
			formEditorID = (parts[0] == "NONE") ? "" : parts[0];
			relationship.ParseFilter(parts[1]);
			
			// Parse global filter (e.g., "PerkInvestorWhiterunBlacksmith=0")
			if (parts[2] != "NONE") {
				globalCondition.ParseFilter(parts[2]);
			}
			logger::debug("Parsed merchant filter - Form: '{}', Relationship filter parsed, Global: '{}'", 
						formEditorID, globalCondition.globalEditorID);
		} else {
			logger::warn("Merchant filter has insufficient parts ({}), expected at least 3", parts.size());
		}
	}

private:
	std::vector<std::string> SplitString(const std::string& str, const char* delimiter) {
		std::vector<std::string> result;
		boost::split(result, str, boost::is_any_of(delimiter));
		return result;
	}
};

// Structure for player filters
struct PlayerFilter {
	ComparisonFilter levelFilter;
	int skillID;
	int skillLevel;
	std::string perkEditorID;

	PlayerFilter() : skillLevel(-1), skillID(-1) {}
	
	PlayerFilter(const std::string& filterStr) {
		ParseFilter(filterStr);
	}

	void ParseFilter(const std::string& filterStr) {
		logger::trace("Parsing player filter: '{}'", filterStr);
		std::vector<std::string> parts = SplitString(filterStr, "|");
		if (parts.size() >= 3) {
			levelFilter.ParseFilter(parts[0]);
			
			// Parse skill filter (e.g., "skillid(level)")
			if (parts[1] != "NONE") {
				size_t parenPos = parts[1].find('(');
				if (parenPos != std::string::npos) {
					skillID = std::stoi(parts[1].substr(0, parenPos));
					std::string levelStr = parts[1].substr(parenPos + 1);
					levelStr.pop_back(); // Remove closing parenthesis
					skillLevel = std::stoi(levelStr);
					logger::debug("Parsed skill requirement: ID {} level {}", skillID, skillLevel);
				}
			}
			
			perkEditorID = (parts[2] == "NONE") ? "" : parts[2];
			logger::debug("Parsed player filter - Level filter parsed, Skill: {}({}), Perk: '{}'", 
						skillID, skillLevel, perkEditorID);
		} else {
			logger::warn("Player filter has insufficient parts ({}), expected at least 3", parts.size());
		}
	}

private:
	std::vector<std::string> SplitString(const std::string& str, const char* delimiter) {
		std::vector<std::string> result;
		boost::split(result, str, boost::is_any_of(delimiter));
		return result;
	}
};

// Structure for filter sets
struct FilterSet {
	std::vector<ItemFilter> itemFilters;
	std::vector<MerchantFilter> merchantFilters;
	std::vector<PlayerFilter> playerFilters;

	void ParseFilters(const nlohmann::json& filtersJson) {
		logger::trace("Parsing filter set from JSON");
		
		if (filtersJson.contains("item")) {
			logger::debug("Found {} item filters", filtersJson["item"].size());
			for (const auto& itemFilter : filtersJson["item"]) {
				itemFilters.emplace_back(itemFilter.get<std::string>());
			}
		}
		
		if (filtersJson.contains("merchant")) {
			logger::debug("Found {} merchant filters", filtersJson["merchant"].size());
			for (const auto& merchantFilter : filtersJson["merchant"]) {
				merchantFilters.emplace_back(merchantFilter.get<std::string>());
			}
		}
		
		if (filtersJson.contains("player")) {
			logger::debug("Found {} player filters", filtersJson["player"].size());
			for (const auto& playerFilter : filtersJson["player"]) {
				playerFilters.emplace_back(playerFilter.get<std::string>());
			}
		}
		
		logger::debug("Parsed filter set - Items: {}, Merchants: {}, Players: {}", 
					itemFilters.size(), merchantFilters.size(), playerFilters.size());
	}
};

// Structure for configuration entries
struct ConfigEntry {
	ValueRange value;
	FilterSet filters;

	ConfigEntry() = default;
	
	ConfigEntry(const nlohmann::json& entryJson) {
		logger::trace("Creating config entry from JSON");
		if (entryJson.contains("value")) {
			value.ParseValue(entryJson["value"].get<std::string>());
		}
		
		if (entryJson.contains("filters")) {
			filters.ParseFilters(entryJson["filters"]);
		}
		logger::debug("Created config entry with value range [{}, {}] and filter set", 
					value.min, value.max);
	}
};

class ConfigManager : public SINGLETON<ConfigManager> {
public:
	static ConfigManager& getInstance() {
		static ConfigManager instance("Data/SKSE/Plugins/DynamicMerchant.json");
		return instance;
	}

	// Get price multiplier for given conditions
	float GetPriceMultiplier(RE::Actor* trader = nullptr, RE::InventoryEntryData* item = nullptr, RE::PlayerCharacter* player = nullptr) {
		logger::trace("Getting price multiplier for trader: {}, item: {}, player: {}", 
					(void*)trader, (void*)item, (void*)player);
		
		static auto trader_id = trader->formID;
		static std::unordered_map<RE::FormID, float>cache;

		if (trader_id == trader->formID) {
			if (cache.contains(item->object->formID)) return cache[item->object->formID];
		}
		else {
			trader_id = trader->formID;
			cache.clear();
		}

		for (size_t i = 0; i < priceEntries.size(); ++i) {
			const auto& entry = priceEntries[i];
			logger::trace("Checking price entry {} of {}", i + 1, priceEntries.size());
			if (MatchesFilters(entry.filters, trader, item, player)) {
				float multiplier = entry.value.GetValue();
				logger::info("Price multiplier {} applied from entry {}", multiplier, i + 1);
				cache[item->object->formID] = multiplier;
				return multiplier;
			}
		}
		logger::debug("No price entries matched, returning default multiplier 1.0");
		cache[item->object->formID] = 1.0f;
		return 1.0f; // Default multiplier
	}

	// Get count multiplier for given conditions
	float GetCountMultiplier(RE::Actor* trader = nullptr, RE::InventoryEntryData* item = nullptr, RE::PlayerCharacter* player = nullptr) {
		logger::trace("Getting count multiplier for trader: {}, item: {}, player: {}", 
					(void*)trader, (void*)item, (void*)player);
		
		static auto trader_id = trader->formID;
		static std::unordered_map<RE::FormID, float>cache;

		if (trader_id == trader->formID) {
			if (cache.contains(item->object->formID)) return cache[item->object->formID];
		}
		else {
			trader_id = trader->formID;
			cache.clear();
		}
		
		for (size_t i = 0; i < countEntries.size(); ++i) {
			const auto& entry = countEntries[i];
			logger::trace("Checking count entry {} of {}", i + 1, countEntries.size());
			if (MatchesFilters(entry.filters, trader, item, player)) {
				float multiplier = entry.value.GetValue();
				logger::info("Count multiplier {} applied from entry {}", multiplier, i + 1);
				cache[item->object->formID] = multiplier;
				return multiplier;
			}
		}
		logger::debug("No count entries matched, returning default multiplier 1.0");
		cache[item->object->formID] = 1.0f;
		return 1.0f; // Default multiplier
	}

	// Reload configuration from file
	bool ReloadConfig() {
		logger::info("Reloading configuration from: {}", configPath);
		return LoadConfig(configPath);
	}

private:
	std::string configPath;
	std::vector<ConfigEntry> priceEntries;
	std::vector<ConfigEntry> countEntries;

	ConfigManager(const char* path) : configPath(path) {
		logger::info("Initializing ConfigManager with path: {}", path);
		LoadConfig(path);
	}

	bool LoadConfig(const std::string& path) {
		logger::info("Loading configuration from: {}", path);
		try {
			std::ifstream file(path);
			if (!file.is_open()) {
				logger::error("Failed to open config file: {}", path);
				return false;
			}

			nlohmann::json configJson;
			file >> configJson;
			logger::debug("Successfully parsed JSON from config file");

			// Clear existing entries
			priceEntries.clear();
			countEntries.clear();
			logger::debug("Cleared existing configuration entries");

			// Load price entries
			if (configJson.contains("Prices")) {
				logger::debug("Loading price entries from config");
				for (size_t i = 0; i < configJson["Prices"].size(); ++i) {
					logger::trace("Loading price entry {}", i + 1);
					priceEntries.emplace_back(configJson["Prices"][i]);
				}
				logger::info("Loaded {} price entries", priceEntries.size());
			} else {
				logger::warn("No 'Prices' section found in config");
			}

			// Load count entries
			if (configJson.contains("Counts")) {
				logger::debug("Loading count entries from config");
				for (size_t i = 0; i < configJson["Counts"].size(); ++i) {
					logger::trace("Loading count entry {}", i + 1);
					countEntries.emplace_back(configJson["Counts"][i]);
				}
				logger::info("Loaded {} count entries", countEntries.size());
			} else {
				logger::warn("No 'Counts' section found in config");
			}

			logger::info("Configuration loaded successfully - {} price entries and {} count entries", 
						priceEntries.size(), countEntries.size());
			return true;

		} catch (const std::exception& e) {
			logger::error("Error loading config: {}", e.what());
			return false;
		}
	}

	bool MatchesFilters(const FilterSet& filters, RE::Actor* trader, RE::InventoryEntryData* item, RE::PlayerCharacter* player) {
		logger::trace("Checking filter matches - Item filters: {}, Merchant filters: {}, Player filters: {}", 
					filters.itemFilters.size(), filters.merchantFilters.size(), filters.playerFilters.size());
		
		// Check item filters (OR condition between filters)
		if (!filters.itemFilters.empty() && item) {
			logger::trace("Checking {} item filters", filters.itemFilters.size());
			bool itemMatches = false;
			for (size_t i = 0; i < filters.itemFilters.size(); ++i) {
				const auto& itemFilter = filters.itemFilters[i];
				logger::trace("Checking item filter {}", i + 1);
				if (MatchesItemFilter(itemFilter, item)) {
					itemMatches = true;
					logger::debug("Item filter {} matched", i + 1);
					break;
				}
			}
			if (!itemMatches) {
				logger::debug("No item filters matched, rejecting");
				return false;
			}
		} else if (!filters.itemFilters.empty() && !item) {
			logger::debug("Item filters present but no item provided, rejecting");
			return false;
		}

		// Check merchant filters (OR condition between filters)
		if (!filters.merchantFilters.empty() && trader) {
			logger::trace("Checking {} merchant filters", filters.merchantFilters.size());
			bool merchantMatches = false;
			for (size_t i = 0; i < filters.merchantFilters.size(); ++i) {
				const auto& merchantFilter = filters.merchantFilters[i];
				logger::trace("Checking merchant filter {}", i + 1);
				if (MatchesMerchantFilter(merchantFilter, trader)) {
					merchantMatches = true;
					logger::debug("Merchant filter {} matched", i + 1);
					break;
				}
			}
			if (!merchantMatches) {
				logger::debug("No merchant filters matched, rejecting");
				return false;
			}
		} else if (!filters.merchantFilters.empty() && !trader) {
			logger::debug("Merchant filters present but no trader provided, rejecting");
			return false;
		}

		// Check player filters (OR condition between filters)
		if (!filters.playerFilters.empty() && player) {
			logger::trace("Checking {} player filters", filters.playerFilters.size());
			bool playerMatches = false;
			for (size_t i = 0; i < filters.playerFilters.size(); ++i) {
				const auto& playerFilter = filters.playerFilters[i];
				logger::trace("Checking player filter {}", i + 1);
				if (MatchesPlayerFilter(playerFilter, player)) {
					playerMatches = true;
					logger::debug("Player filter {} matched", i + 1);
					break;
				}
			}
			if (!playerMatches) {
				logger::debug("No player filters matched, rejecting");
				return false;
			}
		} else if (!filters.playerFilters.empty() && !player) {
			logger::debug("Player filters present but no player provided, rejecting");
			return false;
		}

		logger::debug("All filter checks passed, accepting");
		return true;
	}

	bool MatchesItemFilter(const ItemFilter& filter, RE::InventoryEntryData* item) {
		if (!item || !item->object) {
			logger::trace("Item filter check failed: null item or object");
			return false;
		}

		logger::trace("Checking item filter - Form: '{}', Keyword: '{}'", 
					filter.formEditorID, filter.keywordEditorID);

		// Check form editor ID
		if (!filter.formEditorID.empty()) {
			auto targetForm = RE::TESForm::LookupByEditorID(filter.formEditorID);
			if (!targetForm || targetForm->formID != item->object->GetFormID()) {
				logger::trace("Item form ID mismatch: expected {}, got {}", 
							targetForm ? targetForm->formID : 0, item->object->GetFormID());
				return false;
			}
			logger::trace("Item form ID matched");
		}

		// Check keyword
		if (!filter.keywordEditorID.empty()) {
			auto keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(filter.keywordEditorID);
			if (!keyword || !item->object->HasKeywordInArray({ keyword }, false)) {
				logger::trace("Item keyword '{}' not found", filter.keywordEditorID);
				return false;
			}
			logger::trace("Item keyword '{}' matched", filter.keywordEditorID);
		}

		// Check weight
		if (filter.weightFilter.type != ComparisonFilter::NONE) {
			float weight = item->object->GetWeight();
			logger::trace("Checking item weight: {}", weight);
			if (!filter.weightFilter.Matches(weight)) {
				logger::trace("Item weight filter failed");
				return false;
			}
			logger::trace("Item weight filter passed");
		}

		// Check value
		if (filter.valueFilter.type != ComparisonFilter::NONE) {
			int value = item->object->GetGoldValue();
			logger::trace("Checking item value: {}", value);
			if (!filter.valueFilter.Matches(static_cast<float>(value))) {
				logger::trace("Item value filter failed");
				return false;
			}
			logger::trace("Item value filter passed");
		}

		logger::debug("Item filter passed all checks");
		return true;
	}

	bool MatchesMerchantFilter(const MerchantFilter& filter, RE::Actor* trader) {
		if (!trader) {
			logger::trace("Merchant filter check failed: null trader");
			return false;
		}

		logger::trace("Checking merchant filter - Form: '{}', Global: '{}'", 
					filter.formEditorID, filter.globalCondition.globalEditorID);

		// Check form editor ID
		if (!filter.formEditorID.empty()) {
			auto targetForm = RE::TESForm::LookupByEditorID(filter.formEditorID);
			if (!targetForm || targetForm->formID != trader->formID) {
				logger::trace("Merchant form ID mismatch: expected {}, got {}", 
							targetForm ? targetForm->formID : 0, trader->formID);
				return false;
			}
			logger::trace("Merchant form ID matched");
		}

		// Check relationship
		if (filter.relationship.type != ComparisonFilter::NONE) {
			auto player = RE::PlayerCharacter::GetSingleton();
			auto rel = RE::BGSRelationship::GetRelationship(player->GetActorBase(), trader->GetActorBase());
			if (rel) {
				int relationshipLevel = static_cast<int>(rel->level.get());
				logger::trace("Checking merchant relationship level: {}", relationshipLevel);
				if (!filter.relationship.Matches(relationshipLevel)) {
					logger::trace("Merchant relationship filter failed");
					return false;
				}
				logger::trace("Merchant relationship filter passed");
			} else {
				logger::trace("No relationship found between player and merchant");
				return false;
			}
		}

		// Check global variable
		if (!filter.globalCondition.globalEditorID.empty()) {
			auto globalVar = RE::TESForm::LookupByEditorID<RE::TESGlobal>(filter.globalCondition.globalEditorID);
			if (!globalVar) {
				logger::trace("Global variable '{}' not found", filter.globalCondition.globalEditorID);
				return false;
			}
			
			float globalValue = globalVar->value;
			logger::trace("Checking global variable '{}' value: {}", 
						filter.globalCondition.globalEditorID, globalValue);
			
			if (!filter.globalCondition.againstValue.Matches(globalValue)) {
				logger::trace("Global variable filter failed");
				return false;
			}
			logger::trace("Global variable filter passed");
		}

		logger::debug("Merchant filter passed all checks");
		return true;
	}

	bool MatchesPlayerFilter(const PlayerFilter& filter, RE::PlayerCharacter* player) {
		if (!player) {
			logger::trace("Player filter check failed: null player");
			return false;
		}

		logger::trace("Checking player filter - Skill: {}({}), Perk: '{}'", 
					filter.skillID, filter.skillLevel, filter.perkEditorID);

		// Check player level
		if (filter.levelFilter.type != ComparisonFilter::NONE) {
			float level = static_cast<float>(player->GetLevel());
			logger::trace("Checking player level: {}", level);
			if (!filter.levelFilter.Matches(level)) {
				logger::trace("Player level filter failed");
				return false;
			}
			logger::trace("Player level filter passed");
		}

		// Check skill level
		if (filter.skillID >= 0 && filter.skillLevel >= 0) {
			auto av = player->AsActorValueOwner();
			if (av) {
				float currentSkillLevel = av->GetActorValue(static_cast<RE::ActorValue>(filter.skillID));
				logger::trace("Checking player skill {} level: {} (required: {})", 
							filter.skillID, currentSkillLevel, filter.skillLevel);
				if (currentSkillLevel < filter.skillLevel) {
					logger::trace("Player skill level filter failed");
					return false;
				}
				logger::trace("Player skill level filter passed");
			} else {
				logger::trace("Could not get player actor value owner");
				return false;
			}
		}

		// Check perk
		if (!filter.perkEditorID.empty()) {
			auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(filter.perkEditorID);
			if (!perk) {
				logger::trace("Perk '{}' not found", filter.perkEditorID);
				return false;
			}
			
			bool hasPerk = player->HasPerk(perk);
			logger::trace("Checking player perk '{}': {}", filter.perkEditorID, hasPerk ? "has" : "missing");
			if (!hasPerk) {
				logger::trace("Player perk filter failed");
				return false;
			}
			logger::trace("Player perk filter passed");
		}

		logger::debug("Player filter passed all checks");
		return true;
	}
};