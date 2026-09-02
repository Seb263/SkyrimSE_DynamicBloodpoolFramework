#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/JsonUtils.hpp"
#include "Utils/MiscUtils.hpp"

namespace JSONHandler
{
	using namespace ModData;

	class Main
	{
	public:
		static void LoadMappings();

	private:
		static void ProcessPoolProfilesMapping(const json& jsonData);

		template <typename T>
		static bool ValidateDecalModel(const json& texValue, T& model);
		static RE::BGSTextureSet* CreateTextureSetFromJson(const json& texValue);

		template <typename T>
		static T* ParseFormFromJson(const json& j, const bool useEditorID);

		static void ClampRange(float& a, float& b, float minVal, float maxVal, const bool reorder = false);
		static std::uint32_t ParseHexColor(const std::string& value, std::uint32_t fallback = 0x0);
	};
}
