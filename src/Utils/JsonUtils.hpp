#pragma once

#include "Utils/MiscUtils.hpp"

class JsonUtils
{
public:

	static void MergeJsonRecursive(json& target, const json& source)
	{
		for (auto it = source.begin(); it != source.end(); ++it) {
			if (target.contains(it.key())) {
				if (target[it.key()].is_object() && it.value().is_object()) {
					MergeJsonRecursive(target[it.key()], it.value());
				} else if (target[it.key()].is_array() && it.value().is_array()) {
					for (const auto& item : it.value()) {
						bool exists = std::find(target[it.key()].begin(), target[it.key()].end(), item) != target[it.key()].end();
						if (!exists) {
							target[it.key()].push_back(item);
						}
					}
				} else {
					target[it.key()] = it.value();
				}
			} else {
				target[it.key()] = it.value();
			}
		}
	}

	static void ProcessKeysWithDelimiter(json& jsonData, char delimiter = '|') {
		if (jsonData.is_array()) {
			for (auto& value : jsonData) {
				if (value.is_object() || value.is_array()) ProcessKeysWithDelimiter(value, delimiter);
			}
		} else if (jsonData.is_object()) {
			json newJsonData;
			for (auto& [key, value] : jsonData.items()) {
				if (value.is_object() || value.is_array()) ProcessKeysWithDelimiter(value, delimiter);
				for (const auto& newKey : MiscUtils::SplitString<std::unordered_set<std::string>>(key, delimiter)) {
					newJsonData[newKey] = value;
				}
			}
			jsonData = std::move(newJsonData);
		}
	}
};
