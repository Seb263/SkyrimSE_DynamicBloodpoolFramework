#pragma once

#include "DataHandler.hpp"

#include "Utils/MiscUtils.hpp"

namespace SettingsIni
{
	// Initialization & default values
	inline int iGeneral_VerboseMode = 1;
	inline bool bGeneral_AsynchronousStartup = false;
	inline bool bGeneral_OverwriteInvalidScripts = true;
	inline bool bGeneral_ExtractScriptSources = false;

	// Core settings
	inline int iCore_TargetFramerate = 60;
	inline float fCore_HeightOffset = 1.5f;
	inline float fCore_CancelDistance = 60.0f;
	inline float fCore_RandomRotationDeg = 20.0f;

	// Stability
	inline float fStability_DistanceThreshold = 3.0f;
	inline float fStability_RotationThreshold = 0.2f;
	inline float fStability_StableDuration = 1.0f;
	inline float fStability_MaxWaitDuration = 30.0f;
	inline float fStability_PollDelay = 0.1f;

	// Decals
	inline std::string sDecals_FileLayerBase = "Root";
	inline std::string sDecals_FileLayerExt = "RootExt";
	inline std::string sDecals_BaseGeometry = "Decal:01,Decal:02,Decal:03,Decal:04";
	inline std::string sDecals_ExtGeometry = "DecalExt:01,DecalExt:02,DecalExt:03,DecalExt:04";

	// Raycasting / Physics
	inline float fRaycast_MaxStepHeight = 5.0f;
	inline float fRaycast_MaxSlopeUp = 8.0f;
	inline float fRaycast_MaxSlopeDown = 55.0f;
	inline float fRaycast_SlopeMultiplier = 1.25f;
	inline bool bRaycast_AdvancedRaycasting = true;
	inline bool bRaycast_CheckSlopeParent = true;
	inline int iRaycast_ScalingConstraintMode = 0;
	inline bool bRaycast_AutoOrientSlope = true;
	inline float fRaycast_AutoOrientSlopeMinDeg = 8.0f;

	// Flow
	inline float fFlow_Stability = 0.3f;
	inline float fFlow_StutterRate = 0.6f;
	inline float fFlow_StutterEaseIn = 0.8f;
	inline float fFlow_StutterEaseOut = 0.3f;

	// Maintenance
	inline int iMaintenance_MaxBloodpools = 12;
	inline float fMaintenance_DespawnDuration = 5.0f;

	// Dynamic values
	inline std::vector<std::string> vDecalBaseGeometry = {};
	inline std::vector<std::string> vDecalExtGeometry = {};

	class SettingsManager
	{
	public:
		static SettingsManager& GetSingleton()
		{
			static SettingsManager instance;
			return instance;
		}

		SettingsManager()
		{
			bindings = {
				// General
				{ "General", "iVerboseMode", &iGeneral_VerboseMode },
				{ "General", "bAsynchronousStartup", &bGeneral_AsynchronousStartup },
				{ "General", "bOverwriteInvalidScripts", &bGeneral_OverwriteInvalidScripts },
				{ "General", "bExtractScriptSources", &bGeneral_ExtractScriptSources },

				// Core settings
				{ "Core", "iTargetFramerate", &iCore_TargetFramerate },
				{ "Core", "fHeightOffset", &fCore_HeightOffset },
				{ "Core", "fCancelDistance", &fCore_CancelDistance },
				{ "Core", "fRandomRotationDeg", &fCore_RandomRotationDeg },

				// Stability
				{ "Stability", "fDistanceThreshold", &fStability_DistanceThreshold },
				{ "Stability", "fRotationThreshold", &fStability_RotationThreshold },
				{ "Stability", "fStableDuration", &fStability_StableDuration },
				{ "Stability", "fMaxWaitDuration", &fStability_MaxWaitDuration },
				{ "Stability", "fPollDelay", &fStability_PollDelay },

				// Decals
				{ "Decals", "sLayerBase", &sDecals_FileLayerBase },
				{ "Decals", "sLayerExt", &sDecals_FileLayerExt },
				{ "Decals", "sBaseGeometry", &sDecals_BaseGeometry },
				{ "Decals", "sExtGeometry", &sDecals_ExtGeometry },

				// Raycasting / Physics
				{ "Raycast", "fMaxStepHeight", &fRaycast_MaxStepHeight },
				{ "Raycast", "fMaxSlopeUp", &fRaycast_MaxSlopeUp },
				{ "Raycast", "fMaxSlopeDown", &fRaycast_MaxSlopeDown },
				{ "Raycast", "fSlopeMultiplier", &fRaycast_SlopeMultiplier },
				{ "Raycast", "bAdvancedRaycasting", &bRaycast_AdvancedRaycasting },
				{ "Raycast", "bCheckSlopeParent", &bRaycast_CheckSlopeParent },
				{ "Raycast", "iScalingConstraintMode", &iRaycast_ScalingConstraintMode },
				{ "Raycast", "bAutoOrientSlope", &bRaycast_AutoOrientSlope },
				{ "Raycast", "fAutoOrientSlopeMinDeg", &fRaycast_AutoOrientSlopeMinDeg },

				// Flow
				{ "Flow", "fFlowStability", &fFlow_Stability },
				{ "Flow", "fFlowStutterRate", &fFlow_StutterRate },
				{ "Flow", "fFlowStutterEaseIn", &fFlow_StutterEaseIn },
				{ "Flow", "fFlowStutterEaseOut", &fFlow_StutterEaseOut },

				// Maintenance
				{ "Maintenance", "iMaxBloodpools", &iMaintenance_MaxBloodpools },
				{ "Maintenance", "fDespawnDuration", &fMaintenance_DespawnDuration }
			};

			for (auto& bind : bindings) {
				std::visit([&](auto* ptr) {
					bind.defaultValue = *ptr;
				}, bind.var);
			}
		}

		bool ReadSettings()
		{
			std::wstring   wpath_str(path.begin(), path.end());
			const wchar_t* wpath = wpath_str.c_str();

			bool readStatus = false;

			logger::info("Trying to read INI file at path: {}", path);

			if (std::filesystem::exists(path)) {
				CSimpleIniA ini;
				ini.SetUnicode();

				if (ini.LoadFile(wpath) >= 0) {
					for (const auto& bind : bindings) {
						std::visit([&](auto* ptr) {
							using T = std::decay_t<decltype(*ptr)>;
							if constexpr (std::is_same_v<T, bool>) {
								*ptr = ini.GetBoolValue(bind.section, bind.key, *ptr);
							} else if constexpr (std::is_same_v<T, int>) {
								*ptr = static_cast<int>(ini.GetLongValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, float>) {
								*ptr = static_cast<float>(ini.GetDoubleValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, std::string>) {
								*ptr = ini.GetValue(bind.section, bind.key, ptr->c_str());
							}
						}, bind.var);
					}
					readStatus = true;
				} else {
					logger::error("Failed to load INI file at {}", path);
				}
			} else {
				logger::warn("INI file does not exist at {}", path);
			}

			// Clamping logic

			// General
			iGeneral_VerboseMode = std::clamp(iGeneral_VerboseMode, 0, 2);

			// Stability
			fStability_DistanceThreshold = std::clamp(fStability_DistanceThreshold, 1.0f, 100.0f);
			fStability_RotationThreshold = std::clamp(fStability_RotationThreshold, 0.0f, RE::NI_PI);
			fStability_StableDuration = std::clamp(fStability_StableDuration, 0.5f, 6.0f);
			fStability_MaxWaitDuration = std::clamp(fStability_MaxWaitDuration, 1.0f, 60.0f);
			fStability_PollDelay = std::clamp(fStability_PollDelay, 0.05f, 1.0f);

			// Core settings
			iCore_TargetFramerate = (iCore_TargetFramerate < 0) ? -1 : std::max(iCore_TargetFramerate, 5);
			fCore_HeightOffset = std::clamp(fCore_HeightOffset, 0.1f, 10.0f);
			fCore_CancelDistance = std::clamp(fCore_CancelDistance, 8.0f, 512.0f);
			fCore_RandomRotationDeg = std::clamp(fCore_RandomRotationDeg, 0.0f, 180.0f);

			// Raycasting / Physics
			fRaycast_MaxStepHeight = std::clamp(fRaycast_MaxStepHeight, 1.0f, 20.0f);
			fRaycast_MaxSlopeUp = std::clamp(fRaycast_MaxSlopeUp, 0.0f, 90.0f);
			fRaycast_MaxSlopeDown = std::clamp(fRaycast_MaxSlopeDown, 0.0f, 90.0f);
			fRaycast_SlopeMultiplier = std::clamp(fRaycast_SlopeMultiplier, 1.0f, 3.0f);
			iRaycast_ScalingConstraintMode = std::clamp(iRaycast_ScalingConstraintMode, 0, 2);
			fRaycast_AutoOrientSlopeMinDeg = std::clamp(fRaycast_AutoOrientSlopeMinDeg, 0.0f, 90.0f);

			// Flow
			fFlow_Stability = std::clamp(fFlow_Stability, 0.0f, 1.0f);
			fFlow_StutterRate = std::clamp(fFlow_StutterRate, 0.0f, 1.0f);
			fFlow_StutterEaseIn = std::clamp(fFlow_StutterEaseIn, 0.0f, 10.0f);
			fFlow_StutterEaseOut = std::clamp(fFlow_StutterEaseOut, 0.0f, 10.0f);

			// Maintenance
			iMaintenance_MaxBloodpools = std::clamp(iMaintenance_MaxBloodpools, 1, 96);
			fMaintenance_DespawnDuration = std::clamp(fMaintenance_DespawnDuration, 0.0f, 10.0f);

			// External / Misc data
			[&]() {
				using namespace ModData;

				vDecalBaseGeometry = MiscUtils::SplitString<std::vector<std::string>>(sDecals_BaseGeometry, ',');
				vDecalExtGeometry = MiscUtils::SplitString<std::vector<std::string>>(sDecals_ExtGeometry, ',');

				debugVerboseMode = iGeneral_VerboseMode;
			}();

			return readStatus;
		}

		std::optional<std::variant<bool, int, float, std::string>> GetValueVariant(const std::string& key_section)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("GetValueVariant: Invalid key_section format: '{}'", key_section);
				return std::nullopt;
			}
			std::string section = key_section.substr(0, sep);
			std::string key     = key_section.substr(sep + 1);
			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					if (auto v = std::get_if<bool*>        (&bind.var)) return **v;
					if (auto v = std::get_if<int*>         (&bind.var)) return **v;
					if (auto v = std::get_if<float*>       (&bind.var)) return **v;
					if (auto v = std::get_if<std::string*> (&bind.var)) return **v;
				}
			}
			return std::nullopt;
		}

		template <typename T>
		T GetValue(const std::string& key_section, const T& defaultValue = T{})
		{
			auto opt = GetValueVariant(key_section);
			if (!opt) {
				logger::error("GetValue: No binding found for '{}'", key_section);
				return defaultValue;
			}

			return std::visit([&](auto&& val) -> T {
				using V = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, std::string>) {
					if constexpr (std::is_same_v<V, std::string>) return val;
				} else if constexpr (std::is_same_v<T, bool>) {
					if constexpr (std::is_same_v<V, bool>)  return val;
					if constexpr (std::is_same_v<V, int>)   return val != 0;
					if constexpr (std::is_same_v<V, float>) return val != 0.0f;
				} else if constexpr (std::is_same_v<T, int>) {
					if constexpr (std::is_same_v<V, int>)   return val;
					if constexpr (std::is_same_v<V, float>) return static_cast<int>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1 : 0;
				} else if constexpr (std::is_same_v<T, float>) {
					if constexpr (std::is_same_v<V, float>) return val;
					if constexpr (std::is_same_v<V, int>)   return static_cast<float>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1.0f : 0.0f;
				}
				logger::error("GetValue: Type mismatch for '{}'", key_section);
				return defaultValue;
			}, *opt);
		}

		std::optional<std::variant<bool, int, float, std::string>> GetDefaultValueVariant(const std::string& key_section)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("GetDefaultValueVariant: Invalid key_section format: '{}'", key_section);
				return std::nullopt;
			}
			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					return bind.defaultValue;
				}
			}
			return std::nullopt;
		}

		template <typename T>
		T GetDefaultValue(const std::string& key_section, const T& fallback = T{})
		{
			auto opt = GetDefaultValueVariant(key_section);
			if (!opt) {
				logger::error("GetDefaultValue: No binding found for '{}'", key_section);
				return fallback;
			}

			return std::visit([&](auto&& val) -> T {
				using V = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, bool>) {
					if constexpr (std::is_same_v<V, bool>)  return val;
					if constexpr (std::is_same_v<V, int>)   return val != 0;
					if constexpr (std::is_same_v<V, float>) return val != 0.0f;
				} else if constexpr (std::is_same_v<T, int>) {
					if constexpr (std::is_same_v<V, int>)   return val;
					if constexpr (std::is_same_v<V, float>) return static_cast<int>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1 : 0;
				} else if constexpr (std::is_same_v<T, float>) {
					if constexpr (std::is_same_v<V, float>) return val;
					if constexpr (std::is_same_v<V, int>)   return static_cast<float>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1.0f : 0.0f;
				} else if constexpr (std::is_same_v<T, std::string>) {
					if constexpr (std::is_same_v<V, std::string>) return val;
				}
				logger::error("GetDefaultValue: Type mismatch for '{}'", key_section);
				return fallback;
			}, *opt);
		}

		template <typename T>
		bool SetValue(const std::string& key_section, const T& value)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("SetValue: Invalid key_section format: '{}'", key_section);
				return false;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			if (section.empty() || key.empty()) {
				logger::error("SetValue: Empty section or key in '{}'", key_section);
				return false;
			}

			for (auto& bind : bindings) {
				if (section == bind.section && key == bind.key) {
					bool matched = std::visit([&](auto* ptr) -> bool {
						using PtrType = std::decay_t<decltype(*ptr)>;
						if constexpr (std::is_same_v<PtrType, T>) {
							*ptr = value;
							return true;
						}
						return false;
					}, bind.var);

					if (!matched) {
						logger::error("SetValue: Type mismatch for '{}:{}'", section, key);
						return false;
					}

					CSimpleIniA ini;
					ini.SetUnicode();
					if (std::filesystem::exists(path)) ini.LoadFile(path.c_str());

					if constexpr (std::is_same_v<T, bool>) {
						ini.SetBoolValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, int>) {
						ini.SetLongValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, float>) {
						ini.SetDoubleValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, std::string>) {
						ini.SetValue(section.c_str(), key.c_str(), value.c_str());
					} else {
						return false;
					}

					if (ini.SaveFile(path.c_str()) < 0) {
						logger::error("SetValue: Failed to save INI file at '{}'", path);
						return false;
					}

					return true;
				}
			}

			logger::error("SetValue: No binding found for '{}:{}'", section, key);
			return false;
		}

	private:
		inline static std::string path = "Data/SKSE/Plugins/DynamicBloodpoolFramework.ini";
		inline static std::string prefix = "DBF";

		using IniValue = std::variant<bool*, int*, float*, std::string*>;

		struct IniBinding
		{
			const char* section;
			const char* key;
			IniValue var;
			std::variant<bool, int, float, std::string> defaultValue;
			bool syncGlobal = false;
		};

		std::vector<IniBinding> bindings;
	};
}
