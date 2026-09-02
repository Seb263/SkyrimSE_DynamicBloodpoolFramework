#include "JSONHandler.h"

namespace JSONHandler
{
	void Main::LoadMappings()
	{
		const auto start = std::chrono::high_resolution_clock::now();
		logger::info("Loading JSON files ({})...", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"));
        
		const auto& dataFiles = MiscUtils::GetAllFiles<true>("Data\\SKSE\\DynamicBloodpoolFramework"sv, ".json"sv);
		
		json mergedData{};
		
		for (const auto& fileName : dataFiles) {
			try {
				std::ifstream fileStream(fileName);
				json fileData = json::parse(fileStream);
				logger::info("Parsing JSON Data In \"{}\"", fileName);
				JsonUtils::ProcessKeysWithDelimiter(fileData, '|');
				JsonUtils::MergeJsonRecursive(mergedData, fileData);
			} catch (const std::exception& e) {
				REPORT_AND_FAIL("Error while processing JSON file '{}': {}", fileName, e.what());
			}
		}

		ProcessPoolProfilesMapping(mergedData);
		MiscUtils::ClearGetFormLookupCache();

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		logger::info("Loading JSON files ({}): DONE after {} seconds", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"), elapsed.count());

		if (debugVerboseMode > 1) TRACE("Content of compiled JSON: {}", mergedData.dump(4));
	}

	void Main::ProcessPoolProfilesMapping(const json& jsonData)
	{
		poolProfilesMapping.clear();
		if (!jsonData.contains("PoolProfilesMapping")) return;

		auto parseCommonSettings = [&](auto& dst, const json& src) -> bool {
			if (src.contains("Pivot") && src["Pivot"].is_array() && src["Pivot"].size() == 2) {
				dst.pivot = { src["Pivot"][0].get<float>(), src["Pivot"][1].get<float>() };
				ClampRange(dst.pivot.x, dst.pivot.y, 0.0f, 1.0f, false);
			}

			dst.duration = src.value("Duration", 1.0f);
			dst.easePower = src.value("EasePower", 1.0f);
			dst.easeMidpoint = src.value("EaseMidpoint", 1.0f);

			dst.textureSet = nullptr;
			auto texValue = src.value("TextureSet", json{});
			if (texValue.is_object()) {
				if (auto* generated = CreateTextureSetFromJson(texValue)) dst.textureSet = generated;
			} else dst.textureSet = ParseFormFromJson<RE::BGSTextureSet>(src.value("TextureSet", ""), false);

			if (!dst.textureSet) {
				logger::error("Invalid or missing TextureSet in pool variant.");
				return false;
			}

			return true;
		};

		std::unordered_map<std::string, std::vector<json>> variantsToMerge;
		for (const auto& item : jsonData["PoolProfilesMapping"]) {
			const std::string profileID = item.value("ProfileID", "");
			if (profileID.empty()) continue;
			variantsToMerge[profileID].push_back(item);
		}

		for (auto& [profileID, entries] : variantsToMerge) {
			std::sort(entries.begin(), entries.end(), [](const json& a, const json& b) {
				return a.value("Priority", 0) < b.value("Priority", 0);
			});

			PoolProfile group{};
			group.profileID = profileID;

			for (auto& item : entries) {
				bool profileOverride = item.value("Override", false);

				if (item.contains("Variants") && item["Variants"].is_array()) {
					std::vector<PoolProfile::PoolVariant> parsedVariants;

					for (const auto& vBase : item["Variants"]) {
						PoolProfile::PoolVariant pv{};
						if (!parseCommonSettings(pv.settings, vBase)) continue;

						if (!ValidateDecalModel(vBase.value("Model", "DynamicBloodpoolFramework\\DynamicDecal-Puddle.nif"), pv.settings.model)) continue;

						using Method = PoolProfile::PoolVariant::Settings::Method;
						const auto type = vBase.value("Method", "");
						pv.settings.method = type == "Scaling" ? Method::kScaling : Method::kReveal;

						// SizeRange
						if (vBase.contains("SizeRange") && vBase["SizeRange"].is_array() && vBase["SizeRange"].size() == 2) {
							pv.settings.sizeRange = { vBase["SizeRange"][0].get<float>(), vBase["SizeRange"][1].get<float>() };
							ClampRange(pv.settings.sizeRange.x, pv.settings.sizeRange.y, 8.0f, 128.0f, true);
						}

						// DurationMultRange
						if (vBase.contains("DurationMultRange") && vBase["DurationMultRange"].is_array() && vBase["DurationMultRange"].size() == 2) {
							pv.settings.durationMultRange = { vBase["DurationMultRange"][0].get<float>(), vBase["DurationMultRange"][1].get<float>() };
							ClampRange(pv.settings.durationMultRange.x, pv.settings.durationMultRange.y, 0.1f, 10.0f, true);
						}

						// FadeOutRange
						if (vBase.contains("FadeOutRange") && vBase["FadeOutRange"].is_array() && vBase["FadeOutRange"].size() == 2) {
							pv.settings.fadeOutRange = RE::NiPoint2{ vBase["FadeOutRange"][0].get<float>(), vBase["FadeOutRange"][1].get<float>() };
							ClampRange(pv.settings.fadeOutRange->x, pv.settings.fadeOutRange->y, 0.0f, 1.0f, true);
						}

						// Extended
						if (vBase.contains("Extended") && vBase["Extended"].is_object()) {
							const auto& vExt = vBase["Extended"];
							PoolProfile::PoolVariant::SettingsExtended ext{};
							if (parseCommonSettings(ext, vExt)) {
								ext.blendFactor = std::clamp(vExt.value("BlendFactor", 0.0f), 0.0f, 1.0f);
								ext.onTop = vExt.value("OnTop", false);

								if (vExt.contains("SizeMultRange") && vExt["SizeMultRange"].is_array() && vExt["SizeMultRange"].size() == 2) {
									ext.sizeMultRange = { vExt["SizeMultRange"][0].get<float>(), vExt["SizeMultRange"][1].get<float>() };
									ClampRange(ext.sizeMultRange.x, ext.sizeMultRange.y, 1.0f, 3.0f, true);
								}

								if (vExt.contains("FadeInRange") && vExt["FadeInRange"].is_array() && vExt["FadeInRange"].size() == 2) {
									ext.fadeInRange = { vExt["FadeInRange"][0].get<float>(), vExt["FadeInRange"][1].get<float>() };
									ClampRange(ext.fadeInRange->x, ext.fadeInRange->y, 0.0f, 1.0f, true);
								}

								pv.settingsExtended = std::move(ext);
							}
						}

						// Shaders
						if (vBase.contains("Shaders") && vBase["Shaders"].is_object()) {
							const auto& vShaders = vBase["Shaders"];
							auto& shaders = pv.shaders;
							using ShaderType = PoolProfile::PoolVariant::Shaders::Type;

							const auto type = vShaders.value("Type", "");
							shaders.type = type == "Greyscale" ? ShaderType::kGreyscale : ShaderType::kDefault;

							shaders.tint = ParseHexColor(vShaders.value("Tint", ""), shaders.tint.ToInt());
							shaders.emissiveColor = ParseHexColor(vShaders.value("EmissiveColor", ""), shaders.emissiveColor.ToInt());
							shaders.emissiveMultiple = std::clamp(vShaders.value("EmissiveMultiple", shaders.emissiveMultiple), 0.0f, 100.0f);
							shaders.specularColor = ParseHexColor(vShaders.value("SpecularColor", ""), shaders.specularColor.ToInt());
							shaders.specularStrength = std::clamp(vShaders.value("SpecularStrength", shaders.specularStrength), 0.0f, 100.0f);
							shaders.glossiness = std::clamp(vShaders.value("Glossiness", shaders.glossiness), 0.0f, 10000.0f);
							shaders.refractionStrength = std::clamp(vShaders.value("RefractionStrength", shaders.refractionStrength), 0.0f, 100.0f);
							shaders.subSurfaceLightRolloff = std::clamp(vShaders.value("SubSurfaceLightRolloff", shaders.subSurfaceLightRolloff), 0.0f, 100.0f);
							shaders.rimLightPower = std::clamp(vShaders.value("RimLightPower", shaders.rimLightPower), 0.0f, 100.0f);
							shaders.alpha = std::clamp(vShaders.value("Alpha", shaders.alpha), 0.0f, 2.0f);
						}

						parsedVariants.push_back(std::move(pv));
					}

					if (group.variants.empty()) {
						group.variants = std::move(parsedVariants);
					} else {
						if (profileOverride) {
							group.variants.insert(group.variants.end(),
								std::make_move_iterator(parsedVariants.begin()),
								std::make_move_iterator(parsedVariants.end()));
						} else {
							group.variants = std::move(parsedVariants);
						}
					}
				}
			}

			logger::info("Profile \"{}\" loaded with {} variants", profileID, group.variants.size());
			poolProfilesMapping[profileID] = std::move(group);
		}

		logger::info("Loaded Bloodpool profiles: {} profiles", poolProfilesMapping.size());
	}

	template <typename T>
	bool Main::ValidateDecalModel(const json& texValue, T& model)
	{
		const auto path = texValue.get<std::string>();
		if (path.empty()) {
			logger::error("Missing bloodpool model path in pool variant.");
			return false;
		}

		RE::BSResourceNiBinaryStream stream("Meshes\\" + path);
		if (!stream.good()) {
			std::string fallbackPath = "Data\\Meshes\\" + path;
			std::ifstream file(fallbackPath, std::ios::binary | std::ios::ate);
			if (!file.good() || file.tellg() <= 0) {
				logger::error("Invalid bloodpool model path. File could not be found: \"{}\".", fallbackPath);
				return false;
			}
		}

		model = path;

		return true;
	}

	RE::BGSTextureSet* Main::CreateTextureSetFromJson(const json& texValue)
	{
		if (!texValue.is_object()) return nullptr;

		static std::unordered_map<std::string, RE::BGSTextureSet*> textureSetCache;

		const std::string jsonKey = texValue.dump();
		auto it = textureSetCache.find(jsonKey);
		if (it != textureSetCache.end()) return it->second;

		const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSTextureSet>();
		auto* textureSet = factory ? factory->Create() : nullptr;
		if (!textureSet) {
			logger::error("Failed to initialize BGSTextureSet Factory.");
			return nullptr;
		}

		textureSet->InitializeData();

		static const std::unordered_map<std::string, RE::BSTextureSet::Texture> slotMap = {
			{ "Diffuse",     RE::BSTextureSet::Texture::kDiffuse },
			{ "Normal",      RE::BSTextureSet::Texture::kNormal },
			{ "Glow",        RE::BSTextureSet::Texture::kGlowMap },
			{ "Height",      RE::BSTextureSet::Texture::kHeight },
			{ "Environment", RE::BSTextureSet::Texture::kEnvironment },
			{ "Specular",    RE::BSTextureSet::Texture::kSpecular }
		};

		for (const auto& [key, slot] : slotMap) {
			if (!texValue.contains(key) || !texValue[key].is_string()) continue;

			const auto path = texValue[key].get<std::string>();
			if (path.empty()) continue;

			RE::BSResourceNiBinaryStream stream("Textures\\" + path);
			if (!stream.good()) {
				std::string fallbackPath = "Data\\Textures\\" + path;
				std::ifstream file(fallbackPath, std::ios::binary | std::ios::ate);
				if (!file.good() || file.tellg() <= 0) {
					logger::error("Invalid texture file for slot \"{}\". File could not be found: \"{}\".", key, fallbackPath);
					return nullptr;
				}
			}

			textureSet->SetTexturePath(slot, path.c_str());
		}

		textureSetCache[jsonKey] = textureSet;

		return textureSet;
	}

	template <typename T>
	T* Main::ParseFormFromJson(const json& j, const bool useEditorID)
	{
		if (j.is_string()) {
			std::string str = j.get<std::string>();
			if (str.empty()) return nullptr;

			if (str.find(':') == std::string::npos) {
				return MiscUtils::GetFormFromEditorID<T>(str);
			} else {
				return MiscUtils::GetFormFromAssoc<T>(str);
			}
		}
		return nullptr;
	}

	void Main::ClampRange(float& a, float& b, float minVal, float maxVal, const bool reorder)
	{
		a = std::clamp(a, minVal, maxVal);
		b = std::clamp(b, minVal, maxVal);
		
		if (reorder && a > b) std::swap(a, b);
	};

	std::uint32_t Main::ParseHexColor(const std::string& value, std::uint32_t fallback)
	{
		const auto hex = (!value.empty() && value[0] == '#') ? value.substr(1) : value;
		if (hex.size() != 6) return fallback;

		try {
			return static_cast<std::uint32_t>(std::stoul(hex, nullptr, 16));
		} catch (...) {
			return fallback;
		}
	}
}
