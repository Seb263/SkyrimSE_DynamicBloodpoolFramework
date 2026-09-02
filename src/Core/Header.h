#pragma once

#include "DataHandler.hpp"

namespace BloodPoolCore
{
	struct BloodpoolConfig
	{
		struct NodeData
		{
			std::string name;
			std::string parentName;
			RE::NiPoint2 offsetXY;
			float depth;
			bool locked = false;

			std::optional<std::string> ringNeighborA;
			std::optional<std::string> ringNeighborB;
		};

		struct Settings
		{
			bool isExtended = false;
			float size = 64.0f;
			float duration = 1.0f;
			float ease = 1.0f;
			float easeMidpoint = 1.0f;
			float blending = 1.0f;
			float offsetZ = 0.0f;
			RE::NiPoint2 pivot = { 0.5f, 0.5f };
			std::optional<RE::NiPoint2> fadeOut = std::nullopt;
			std::optional<RE::NiPoint2> fadeIn = std::nullopt;
		};

		using NodesRig = std::unordered_map<std::string, NodeData>;
		using RigOrder = std::vector<std::string>;
		using Geometry = std::vector<std::string>;
		using FlowCurve = std::vector<std::pair<float, float>>;

		RE::ObjectRefHandle refHandle;

		Settings settings;
		Settings settingsExtended;

		NodesRig rig;
		NodesRig rigExtended;

		RigOrder rigOrder;
		RigOrder rigOrderExtended;

		Geometry geometry;
		Geometry geometryExtended;

		FlowCurve flowCurve;
		FlowCurve flowCurveExtended;

		ModData::PoolProfile::PoolVariant::Shaders shaders;

		bool hasExtended = false;
	};

	struct InstanceParams
	{
		RE::ObjectRefHandle refHandle{};
		RE::BSFixedString nodePos = "";
		RE::BSFixedString nodeRot = "";
		RE::TESObjectCELL* cell = nullptr;

		float rotation = 0.0f;

		struct Settings
		{
			using Method = ModData::PoolProfile::PoolVariant::Settings::Method;

			Method method = Method::kReveal;
			std::string model = "";
			RE::BGSTextureSet* textureSet = nullptr;
			RE::NiPoint2 pivot = { 0.5f, 0.5f };
			float size = 64.0f;
			float duration = 1.0f;
			float easePower = 1.0f;
			float easeMidpoint = 1.0f;
			float blendFactor = 1.0f;
			bool onTop = false;

			RE::NiPoint3 position{};
			std::optional<float> spread;
			std::optional<RE::NiPoint2> fadeOut;
			std::optional<RE::NiPoint2> fadeIn;
		};

		Settings settings;
		Settings settingsExtended;

		ModData::PoolProfile::PoolVariant::Shaders shaders;

		bool hasExtended = false;
	};

	using EmitEndCallback = std::function<void(bool, RE::TESObjectREFR*)>;
};
