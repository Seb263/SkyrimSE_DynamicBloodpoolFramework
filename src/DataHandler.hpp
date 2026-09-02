#pragma once

namespace ModData
{
	constexpr std::string_view MOD_NAME = "Dynamic Bloodpool Framework";

	struct PluginForm
	{
		std::string_view name;
		void**           formPtr;
		uint32_t         formID;
		std::string_view pluginName;
		bool             optional = false;
	};

	struct DefaultForm
	{
		void**      formPtr;
		std::string formStr;
	};

	// Properties storing game form references
	static inline const std::vector<PluginForm> pluginForms = {};

	struct PoolProfile
	{
		struct PoolVariant
		{
			struct Settings
			{
				enum Method
				{
					kReveal,
					kScaling
				};

				Method method = Method::kReveal;
				std::string model = "";
				RE::BGSTextureSet* textureSet = nullptr;
				RE::NiPoint2 pivot = { 0.5f, 0.5f };
				RE::NiPoint2 sizeRange = { 1.0f, 1.0f };
				float duration = 1.0f;
				float easePower = 1.0f;
				float easeMidpoint = 1.0f;
				RE::NiPoint2 durationMultRange = { 1.0f, 1.0f };

				std::optional<RE::NiPoint2> fadeOutRange;
			};

			struct SettingsExtended
			{
				RE::BGSTextureSet* textureSet = nullptr;
				RE::NiPoint2 pivot = { 0.5f, 0.5f };
				RE::NiPoint2 sizeMultRange = { 1.0f, 1.0f };
				float duration = 1.0f;
				float easePower = 1.0f;
				float easeMidpoint = 1.0f;
				float blendFactor = 0.0f;
				bool onTop = false;

				std::optional<RE::NiPoint2> fadeInRange;
			};

			struct Shaders
			{
				enum Type
				{
					kDefault,
					kGreyscale
				};
				Type type = Type::kDefault;
				
				RE::NiColor tint = 0x170000;
				
				RE::NiColor emissiveColor = 0x000000;
				float emissiveMultiple = 1.0f;

				RE::NiColor specularColor = 0xFFFFFF;
				float specularStrength = 1.0f;

				float glossiness = 180.0f;
				float refractionStrength = 0.0f;
				float subSurfaceLightRolloff = 1.0f;
				float rimLightPower = 1.0f;
				float alpha = 1.0f;
			};

			Settings settings;
			std::optional<SettingsExtended> settingsExtended = std::nullopt;

			Shaders shaders;
		};

		int priority = 0;
		std::vector<PoolVariant> variants;
		std::string profileID;
	};

	inline RE::TESDataHandler* TESdataHandler;
	inline RE::BGSKeyword* bloodpoolKeyword;

	inline std::unordered_map<std::string, PoolProfile> poolProfilesMapping;

	inline auto lastLoadPoint = std::chrono::steady_clock::now();
}
