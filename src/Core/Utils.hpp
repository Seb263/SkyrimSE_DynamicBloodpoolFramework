#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Header.h"

#include "Utils/NativeUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace BloodPoolCore
{
	class Utils
	{
	public:

		static bool IsReferenceTooFar(const InstanceParams& p) {
			if (auto* bloodpoolRef = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(p.refHandle), true)) {
				if (auto* origin = !p.nodePos.empty() ? bloodpoolRef->GetNodeByName(p.nodePos) : bloodpoolRef->Get3D(false)) {
					return origin->world.translate.GetDistance(p.settings.position) > SettingsIni::fCore_CancelDistance;
				}
			}
			return false;
		}

		static void ApplyBloodpoolFade(RE::TESObjectREFR* bloodpoolRef, const BloodpoolConfig& bloodpoolConfig,
			const bool isExtended, const float progress, const float localProgress)
		{
			auto& settings = bloodpoolConfig.settingsExtended;
			auto& geometry = !isExtended ? bloodpoolConfig.geometry : bloodpoolConfig.geometryExtended;

			float fade = 1.0f;

			if (isExtended) {
				if (!settings.fadeIn.has_value()) return;
				const float fadeInStart = settings.fadeIn->x;
				const float fadeInEnd = settings.fadeIn->y;

				if (localProgress >= fadeInStart) {
					const float t = (localProgress - fadeInStart) / (fadeInEnd - fadeInStart);
					fade = std::clamp(t, 0.0f, 1.0f);
				}
			} else {
				if (!settings.fadeOut.has_value()) return;
				const float fadeOutStart = settings.fadeOut->x;
				const float fadeOutEnd = settings.fadeOut->y;

				if (progress > fadeOutStart) {
					const float t = (progress - fadeOutStart) / (fadeOutEnd - fadeOutStart);
					fade = 1.0f - std::clamp(t, 0.0f, 1.0f);
				}
			}

			for (const auto& nodeName : geometry) {
				if (auto* object = bloodpoolRef->GetNodeByName(nodeName)) {
					if (auto* mesh = object->AsGeometry()) {
						if (isExtended || fade > 0.0f) {
							mesh->UpdateMaterialAlpha(fade * bloodpoolConfig.shaders.alpha, false);
						} else {
							mesh->SetAppCulled(true);
						}
					}
				}
			}
		}

		struct PendingTransform
		{
			RE::NiNode* node;
			std::string name;
			RE::NiPoint3 translate;
		};
		static bool ApplyBloodpoolScale(RE::NiNode* rootNode, BloodpoolConfig::Settings& settings, BloodpoolConfig::NodesRig& rig,
			const BloodpoolConfig::RigOrder& rigOrder, float progress)
		{
			if (!rootNode) return false;

			const float scale = settings.size * progress;

			auto* bloodpoolRef = rootNode->GetUserData();
			if (!bloodpoolRef || !bloodpoolRef->parentCell || !bloodpoolRef->parentCell->IsAttached()) return false;

			auto* bhkWorld = bloodpoolRef->parentCell->GetbhkWorld();
			if (!bhkWorld) return false;

			const float worldScale = RE::bhkWorld::GetWorldScale();
			const float rotationZ = bloodpoolRef->GetAngleZ();

			std::vector<PendingTransform> pending;
			std::unordered_map<std::string, RE::NiNode*> nodeMap;
			for (auto& child : rootNode->GetChildren()) {
				if (auto* node = child ? child->AsNode() : nullptr) {
					nodeMap[node->name.c_str()] = node;
				}
			}

			auto getNodeByName = [&](const std::string& name) -> RE::NiNode* {
				auto it = nodeMap.find(name);
				return (it != nodeMap.end()) ? it->second : nullptr;
			};

			auto isNodeValid = [&](const BloodpoolConfig::NodeData& data, const RE::NiPoint3& targetPos) -> bool {
				auto currentPos = targetPos;
				currentPos.z += rootNode->local.translate.z;

				auto getParentPosition = [&](const BloodpoolConfig::NodeData& nodeData) -> std::optional<RE::NiPoint3> {
					if (nodeData.parentName.empty()) return std::nullopt;

					auto it = rig.find(nodeData.parentName);
					if (it == rig.end()) return std::nullopt;

					auto itNode = nodeMap.find(it->second.name);
					if (itNode == nodeMap.end()) return std::nullopt;

					auto* node = itNode->second;
					auto itPending = std::find_if(pending.begin(), pending.end(),
						[node](const PendingTransform& t) { return t.node == node; });

					return (itPending != pending.end()) ? std::make_optional(itPending->translate) : std::nullopt;
				};

				if (SettingsIni::bRaycast_CheckSlopeParent) {
					if (auto parentPos = getParentPosition(data)) {
						if (!CheckSlopeParent(*parentPos, currentPos)) return false;
					}
				}

				if (SettingsIni::iRaycast_ScalingConstraintMode == 1) {
					for (auto& [_, childData] : rig) {
						if (childData.parentName == data.name && childData.locked) {
							auto* childNode = getNodeByName(childData.name);
							auto* parentNode = getNodeByName(data.parentName);
							if (!childNode || !parentNode) continue;

							if (!IsNodeWithinParentChildBounds(targetPos, parentNode, childNode)) return false;
						}
					}
				}

				return true;
			};

			bool anyLocked = false;
			for (const auto& name : rigOrder) {
				auto itData = rig.find(name);
				if (itData == rig.end()) continue;
				auto& data = itData->second;

				if (data.locked) continue;

				auto* node = getNodeByName(name);
				if (!node) continue;

				const float radius = data.depth * scale;
				RE::NiPoint3 targetPos{
					data.offsetXY.x * radius,
					data.offsetXY.y * radius,
					!data.parentName.empty() && getNodeByName(data.parentName)
						? getNodeByName(data.parentName)->local.translate.z
						: node->local.translate.z
				};

				const float raycastDistance = (settings.size / 0.707f) * SettingsIni::fRaycast_SlopeMultiplier;
				bool nodeValid = PerformZRaycast(targetPos, rootNode, raycastDistance, rotationZ, bhkWorld, worldScale);

				if (nodeValid) nodeValid = isNodeValid(data, targetPos);

				if (!nodeValid) {
					data.locked = true;
					anyLocked = true;
					LockChildrens(rig, data.name);
					continue;
				}

				targetPos.z += rootNode->local.translate.z;
				pending.push_back({ node, name, targetPos });
			}

			if (pending.empty()) return false;
			if (SettingsIni::iRaycast_ScalingConstraintMode == 2 && anyLocked) return false;

			for (auto& t : pending) {
				t.node->local.translate = t.translate;
			}
			NiUtils::UpdateObjectDownward(rootNode);

			return true;
		}

		static void ApplyBloodpoolUVReveal(RE::TESObjectREFR* bloodpoolRef, const BloodpoolConfig::Geometry& geometry,
			const RE::NiPoint2& pivot, float progress)
		{
			constexpr float kMinScale = 0.01f;

			const float scale = std::clamp(progress, kMinScale, 1.0f);

			const RE::NiPoint2 uvPivot{ pivot.x, 1.0f - pivot.y };
			const RE::NiPoint2 uvOffset{ uvPivot.x * (1.0f - scale), uvPivot.y * (1.0f - scale) };
			const RE::NiPoint2 uvScale{ scale, scale };

			for (const auto& nodeName : geometry) {
				auto* object = bloodpoolRef->GetNodeByName(nodeName);
				auto* mesh = object ? object->AsGeometry() : nullptr;
				if (!mesh) continue;

				auto* lightingShader = mesh->lightingShaderProp_cast();
				if (!lightingShader) continue;

				auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
				if (!material) continue;

				material->texCoordOffset[0] = uvOffset;
				material->texCoordOffset[1] = uvOffset;
				material->texCoordScale[0] = uvScale;
				material->texCoordScale[1] = uvScale;
			}
		}

		static float ApplyEase(float t, float easePower, float easeMidpoint)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			const float pivot = std::clamp(1.0f - easeMidpoint, 0.001f, 0.999f);

			if (t < pivot) {
				const float u = t / pivot;
				return pivot * std::pow(u, easePower);
			} else {
				const float u = (t - pivot) / (1.0f - pivot);
				return pivot + (1.0f - pivot) * (1.0f - std::pow(1.0f - u, easePower));
			}
		}

		static BloodpoolConfig::FlowCurve BuildFlowCurve(std::uint32_t seed)
		{
			const float stability = std::clamp(SettingsIni::fFlow_Stability, 0.0f, 1.0f);
			const float stutterRate = std::clamp(SettingsIni::fFlow_StutterRate, 0.0f, 1.0f);

			if (stability >= 1.0f || stutterRate <= 0.0f) {
				return { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
			}

			constexpr int kSamples = 200;
			constexpr int kFlowSlots = 6;
			constexpr float kMinSpeed = 0.0f;

			const float slotWidth = 1.0f / static_cast<float>(kFlowSlots);
			const float intensity = 1.0f - stability;

			auto hashSlot = [seed](int slot, std::uint32_t salt) -> float {
				std::uint32_t h = seed ^ (static_cast<std::uint32_t>(slot) * 0x9E3779B1u) ^ salt;
				h ^= h >> 15; h *= 0x85EBCA6Bu;
				h ^= h >> 13; h *= 0xC2B2AE35u;
				h ^= h >> 16;
				return static_cast<float>(h) / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
			};

			std::vector<float> speeds(kSamples + 1, 1.0f);

			for (int slot = 0; slot < kFlowSlots; ++slot) {
				if (hashSlot(slot, 0x1u) > stutterRate) continue; // No stutter

				const float slotStart = slot * slotWidth;
				const float widthFactor = 0.4f + hashSlot(slot, 0x3u) * 0.5f; // 40% to 90% of slot width
				const float stutterWidth = slotWidth * widthFactor;
				const float posInSlot = hashSlot(slot, 0x2u);

				const float stutterStart = slotStart + posInSlot * (slotWidth - stutterWidth);
				const float stutterEnd = stutterStart + stutterWidth;

				const int sampleStart = std::clamp(static_cast<int>(stutterStart * kSamples), 0, kSamples);
				const int sampleEnd = std::clamp(static_cast<int>(stutterEnd * kSamples), 0, kSamples);

				for (int i = sampleStart; i <= sampleEnd; ++i) {
					const float localT = (stutterWidth > 0.0f)
						? std::clamp((static_cast<float>(i) / kSamples - stutterStart) / stutterWidth, 0.0f, 1.0f) : 0.0f;

					float dipShape;
					if (localT < 0.5f) {
						const float u = localT / 0.5f;
						dipShape = std::pow(u, std::max(SettingsIni::fFlow_StutterEaseIn, 0.01f));
					} else {
						const float u = (localT - 0.5f) / 0.5f;
						dipShape = 1.0f - std::pow(u, std::max(SettingsIni::fFlow_StutterEaseOut, 0.01f));
					}

					const float speed = std::max(1.0f - dipShape * intensity, kMinSpeed);
					speeds[i] = std::min(speeds[i], speed);
				}
			}

			std::vector<float> cumulative(kSamples + 1, 0.0f);
			for (int i = 1; i <= kSamples; ++i) {
				cumulative[i] = cumulative[i - 1] + speeds[i];
			}

			const float total = cumulative[kSamples];

			BloodpoolConfig::FlowCurve curve;
			curve.reserve(kSamples + 1);
			for (int i = 0; i <= kSamples; ++i) {
				const float t = static_cast<float>(i) / kSamples;
				const float warped = (total > 0.0f) ? (cumulative[i] / total) : t;
				curve.emplace_back(t, warped);
			}

			return curve;
		}

		static float SampleFlowCurve(const BloodpoolConfig::FlowCurve& curve, float t)
		{
			if (curve.size() < 2) return t;

			t = std::clamp(t, curve.front().first, curve.back().first);

			std::size_t lo = 0, hi = curve.size() - 1;
			while (lo + 1 < hi) {
				const std::size_t mid = (lo + hi) / 2;
				if (curve[mid].first <= t) lo = mid; else hi = mid;
			}

			const auto& a = curve[lo];
			const auto& b = curve[hi];
			const float span = b.first - a.first;
			const float alpha = (span > 0.0f) ? (t - a.first) / span : 0.0f;

			return a.second + (b.second - a.second) * alpha;
		}

		static float ComputeMaxDeltaZ(float distXY, float maxSlopeDegrees)
		{
			return distXY * std::tan(maxSlopeDegrees * RE::NI_PI / 180.0f);
		}

		static bool PerformZRaycast(RE::NiPoint3& targetTranslate, const RE::NiAVObject* rootNode,
			const float bloodpoolSize, const float bloodpoolRotation, RE::bhkWorld* bhkWorld, const float worldScale)
		{
			const float gravityAllowed = SettingsIni::fRaycast_MaxStepHeight;

			if (!rootNode) return false;

			const float a = -bloodpoolRotation;
			const float c = std::cos(a);
			const float s = std::sin(a);

			const float localX = targetTranslate.x;
			float localY = targetTranslate.y;

			const float rotatedX = localX * c - localY * s;
			const float rotatedY = localX * s + localY * c;

			RE::NiPoint3 rayBase = rootNode->world.translate;
			rayBase.x += rotatedX;
			rayBase.y += rotatedY;
			rayBase.z += targetTranslate.z;

			const float maxDeltaZUp = ComputeMaxDeltaZ((bloodpoolSize / 2.0f), SettingsIni::fRaycast_MaxSlopeUp);
			const float maxDeltaZDown = ComputeMaxDeltaZ((bloodpoolSize / 2.0f), SettingsIni::fRaycast_MaxSlopeDown);

			const float posMinZ = rootNode->world.translate.z + maxDeltaZUp + gravityAllowed;
			const float posMaxZ = rootNode->world.translate.z - maxDeltaZDown - gravityAllowed;

			auto raycastAttempt = [&](float startZ, float endZ) -> bool {
				if (startZ > posMinZ) startZ = posMinZ;
				if (endZ < posMaxZ) endZ = posMaxZ;

				RE::NiPoint3 rayStart = rayBase;
				RE::NiPoint3 rayEnd = rayBase;
				rayStart.z = startZ;
				rayEnd.z = endZ;

				RE::bhkPickData pickData;
				pickData.rayInput.from = rayStart * worldScale;
				pickData.rayInput.to = rayEnd * worldScale;
				pickData.rayInput.enableShapeCollectionFilter = false;

				static auto collisionGroup = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup();
				auto& cFilter = pickData.rayInput.filterInfo;
				cFilter.SetSystemGroup(collisionGroup);
				cFilter.SetCollisionLayer(RE::COL_LAYER::kPathPick);

				bhkWorld->PickObject(pickData);

				if (!pickData.rayOutput.HasHit()) return false;

				const RE::NiPoint3 hitPos = rayStart + (rayEnd - rayStart) * pickData.rayOutput.hitFraction;
				targetTranslate.z = hitPos.z - rootNode->world.translate.z;

				return true;
			};

			// First basic test with a minor vertical offset
			if (raycastAttempt(rayBase.z + gravityAllowed, rayBase.z - gravityAllowed)) return true;

			// Second advanced test based on the theoretical maximum up/down window of the bloodpool
			if (SettingsIni::bRaycast_AdvancedRaycasting) {
				if (raycastAttempt(rayBase.z + maxDeltaZUp + gravityAllowed, rayBase.z - maxDeltaZDown - gravityAllowed)) {
					return true;
				}
			}

			// Failed to raycast the Z axis
			return false;
		}

		static bool CheckSlopeParent(const RE::NiPoint3& parentNodePos, const RE::NiPoint3& currentNodePos)
		{
			const float deltaZ = currentNodePos.z - parentNodePos.z;
			const float absDeltaZ = std::abs(deltaZ);

			if (absDeltaZ <= SettingsIni::fRaycast_MaxStepHeight) return true;

			const float dx = currentNodePos.x - parentNodePos.x;
			const float dy = currentNodePos.y - parentNodePos.y;
			const float distXY = std::sqrt(dx * dx + dy * dy);

			const float slopeDeg = std::atan2(absDeltaZ, distXY) * (180.0f / RE::NI_PI);
			const float maxSlope = (deltaZ > 0.0f) ? SettingsIni::fRaycast_MaxSlopeUp : SettingsIni::fRaycast_MaxSlopeDown;

			return slopeDeg <= maxSlope;
		}

		static bool IsNodeWithinParentChildBounds(const RE::NiPoint3& targetPos, const RE::NiNode* parentNode, const RE::NiNode* childNode)
		{
			if (!parentNode || !childNode) return true;

			RE::NiPoint3 minBound{
				std::min(parentNode->world.translate.x, childNode->world.translate.x),
				std::min(parentNode->world.translate.y, childNode->world.translate.y),
				std::min(parentNode->world.translate.z, childNode->world.translate.z)
			};

			RE::NiPoint3 maxBound{
				std::max(parentNode->world.translate.x, childNode->world.translate.x),
				std::max(parentNode->world.translate.y, childNode->world.translate.y),
				std::max(parentNode->world.translate.z, childNode->world.translate.z)
			};

			return (targetPos.x >= minBound.x && targetPos.x <= maxBound.x) &&
				   (targetPos.y >= minBound.y && targetPos.y <= maxBound.y) &&
				   (targetPos.z >= minBound.z && targetPos.z <= maxBound.z);
		}

		static void LockChildrens(BloodpoolConfig::NodesRig& rig, const std::string& parentName)
		{
			for (auto& [_, data] : rig) {
				if (data.parentName == parentName && !data.locked) {
					data.locked = true;
					LockChildrens(rig, data.name);
				}
			}
		}

		static std::chrono::nanoseconds GetBloodpoolRefreshDelta()
		{
			const int targetFPS = SettingsIni::iCore_TargetFramerate;
			const auto minDelta = FRAME_DELAY();

			if (targetFPS < 0) return minDelta;

			const double targetDeltaSec = (1.0 / static_cast<double>(targetFPS)) * TimeUtils::GetTimeMultiplier();

			auto targetDeltaNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(targetDeltaSec));

			return std::max(minDelta, targetDeltaNs);
		}
	};
};
