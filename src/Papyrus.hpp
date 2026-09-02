#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Init.hpp"

#include "API/Mod-API.h"

namespace Papyrus
{
	std::vector<uint32_t> GetVersion(RE::StaticFunctionTag*)
	{
		using namespace SKSE;
        const auto* plugin = PluginDeclaration::GetSingleton();
        auto version = plugin->GetVersion();

        uint32_t versionMajor = plugin->GetVersion().major();
        uint32_t versionMinor = plugin->GetVersion().minor();
        uint32_t versionPatch = plugin->GetVersion().patch();

		std::vector<uint32_t> versionVector;
		versionVector.push_back(versionMajor);
		versionVector.push_back(versionMinor);
		versionVector.push_back(versionPatch);

		return versionVector;
	}

	bool GetIniValueBool(RE::StaticFunctionTag*, const RE::BSFixedString path, const bool defaultValue = false)
	{
		return SettingsIni::SettingsManager::GetSingleton().GetValue<bool>(path.c_str(), defaultValue);
	}

	float GetIniValueFloat(RE::StaticFunctionTag*, const RE::BSFixedString path, const float defaultValue = 0.0f)
	{ 
		return SettingsIni::SettingsManager::GetSingleton().GetValue<float>(path.c_str(), defaultValue);
	}

	int GetIniValueInt(RE::StaticFunctionTag*, const RE::BSFixedString path, const int defaultValue = 0)
	{
		return SettingsIni::SettingsManager::GetSingleton().GetValue<int>(path.c_str(), defaultValue);
	}

	RE::BSFixedString GetIniValueString(RE::StaticFunctionTag*, const RE::BSFixedString path, const RE::BSFixedString defaultValue = "")
	{
		return RE::BSFixedString(SettingsIni::SettingsManager::GetSingleton().GetValue<std::string>(path.c_str(), std::string(defaultValue.c_str())));
	}

	bool GetDefaultIniValueBool(RE::StaticFunctionTag*, const RE::BSFixedString path, const bool fallback = false)
	{
		return SettingsIni::SettingsManager::GetSingleton().GetDefaultValue<bool>(path.c_str(), fallback);
	}

	float GetDefaultIniValueFloat(RE::StaticFunctionTag*, const RE::BSFixedString path, const float fallback = 0.0f)
	{
		return SettingsIni::SettingsManager::GetSingleton().GetDefaultValue<float>(path.c_str(), fallback);
	}

	int GetDefaultIniValueInt(RE::StaticFunctionTag*, const RE::BSFixedString path, const int fallback = 0)
	{
		return SettingsIni::SettingsManager::GetSingleton().GetDefaultValue<int>(path.c_str(), fallback);
	}

	RE::BSFixedString GetDefaultIniValueString(RE::StaticFunctionTag*, const RE::BSFixedString path, const RE::BSFixedString fallback = "")
	{
		return RE::BSFixedString(SettingsIni::SettingsManager::GetSingleton().GetDefaultValue<std::string>(path.c_str(), std::string(fallback.c_str())));
	}

	bool SetIniValueBool(RE::StaticFunctionTag*, const RE::BSFixedString path, const bool value)
	{
		return SettingsIni::SettingsManager::GetSingleton().SetValue(path.c_str(), value);
	}

	bool SetIniValueFloat(RE::StaticFunctionTag*, const RE::BSFixedString path, const float value)
	{
		return SettingsIni::SettingsManager::GetSingleton().SetValue(path.c_str(), value);
	}

	bool SetIniValueInt(RE::StaticFunctionTag*, const RE::BSFixedString path, const int value)
	{
		return SettingsIni::SettingsManager::GetSingleton().SetValue(path.c_str(), value);
	}

	bool SetIniValueString(RE::StaticFunctionTag*, const RE::BSFixedString path, const RE::BSFixedString value)
	{
		return SettingsIni::SettingsManager::GetSingleton().SetValue(path.c_str(), std::string(value.c_str()));
	}

	void RequestRuntimeUpdate(RE::StaticFunctionTag*)
	{
		/*SKSE::GetTaskInterface()->AddTask([]() {
			// Runtime updates
		});*/
	}

	bool SpawnBloodpoolAtLocation(RE::StaticFunctionTag*,
		RE::BSFixedString profileID, RE::TESObjectREFR* originRef,
		float posX, float posY, float posZ, float rotationZ,
		float scale, float spread, float durationMult)
	{
		DBF_API::Interface::Parameters params;

		params.profileID = profileID;
		params.originRef = originRef;
		params.override.position = RE::NiPoint3{ posX, posY, posZ };
		params.override.rotation = rotationZ;
		if (scale > 0.0f) params.override.scale = scale;
		if (spread >= 0.0f) params.override.spread = spread;
		if (durationMult > 0.0f) params.override.durationMult = durationMult;

		return BloodPoolCore::Init::EmitBloodPool(params);
	}

	bool SpawnBloodpoolAtNode(RE::StaticFunctionTag*,
		RE::BSFixedString profileID, RE::TESObjectREFR* originRef,
		RE::BSFixedString nodeName, float rotationZ,
		float scale, float spread, float durationMult)
	{
		DBF_API::Interface::Parameters params;

		params.profileID = profileID;
		params.originRef = originRef;
		params.originNodePos = nodeName;
		if (rotationZ >= 0.0f) params.override.rotation = rotationZ;
		if (scale > 0.0f) params.override.scale = scale;
		if (spread >= 0.0f) params.override.spread = spread;
		if (durationMult > 0.0f) params.override.durationMult = durationMult;

		return BloodPoolCore::Init::EmitBloodPool(params);
	}

	bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("GetVersion", "DynamicBloodpoolFramework", GetVersion);
		vm->RegisterFunction("GetIniValueBool", "DynamicBloodpoolFramework", GetIniValueBool);
		vm->RegisterFunction("GetIniValueFloat", "DynamicBloodpoolFramework", GetIniValueFloat);
		vm->RegisterFunction("GetIniValueInt", "DynamicBloodpoolFramework", GetIniValueInt);
		vm->RegisterFunction("GetIniValueString", "DynamicBloodpoolFramework", GetIniValueString);
		vm->RegisterFunction("GetDefaultIniValueBool", "DynamicBloodpoolFramework", GetDefaultIniValueBool);
		vm->RegisterFunction("GetDefaultIniValueFloat", "DynamicBloodpoolFramework", GetDefaultIniValueFloat);
		vm->RegisterFunction("GetDefaultIniValueInt", "DynamicBloodpoolFramework", GetDefaultIniValueInt);
		vm->RegisterFunction("GetDefaultIniValueString", "DynamicBloodpoolFramework", GetDefaultIniValueString);
		vm->RegisterFunction("SetIniValueBool", "DynamicBloodpoolFramework", SetIniValueBool);
		vm->RegisterFunction("SetIniValueFloat", "DynamicBloodpoolFramework", SetIniValueFloat);
		vm->RegisterFunction("SetIniValueInt", "DynamicBloodpoolFramework", SetIniValueInt);
		vm->RegisterFunction("SetIniValueString", "DynamicBloodpoolFramework", SetIniValueString);
		vm->RegisterFunction("RequestRuntimeUpdate", "DynamicBloodpoolFramework", RequestRuntimeUpdate);
		vm->RegisterFunction("SpawnBloodpoolAtLocation", "DynamicBloodpoolFramework", SpawnBloodpoolAtLocation);
		vm->RegisterFunction("SpawnBloodpoolAtNode", "DynamicBloodpoolFramework", SpawnBloodpoolAtNode);
		return true;
	}
};
