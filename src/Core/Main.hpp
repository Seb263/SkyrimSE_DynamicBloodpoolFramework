#pragma once

#include "Core/Header.h"
#include "Core/Maintenance.hpp"
#include "Core/Utils.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace BloodPoolCore
{
	class Main
	{
	public:
		static void EmitBloodpoolStart(InstanceParams& p, EmitEndCallback&& endCallback = [](bool, RE::TESObjectREFR*) {})
		{
			bool result = false;

			RE::TESObjectREFR* bloodpoolRef = nullptr;
			if (SetBloodpoolInitialZAxis(p)) {
				if (bloodpoolRef = CreateBloodpoolReference(p)) {
					TRACE("Bloodpool created [REF:{:08X}]", bloodpoolRef->formID);
					result = true;
				}
			}

			if (!result) {
				EmitBloodpoolEnd(endCallback, false, bloodpoolRef);
				return;
			}
			
			auto bloodpoolConfig = std::make_shared<BloodpoolConfig>(BuildBloodpoolConfig(p, bloodpoolRef));
			if (!bloodpoolConfig) {
				EmitBloodpoolEnd(endCallback, false, bloodpoolRef);
				return;
			}

			if (Maintenance::EnforceBloodpoolQuota(bloodpoolRef)) {
				EmitBloodpoolEnd(endCallback, false, bloodpoolRef);
				return;
			}

			TimeUtils::WaitUntil3DReady(bloodpoolRef, [p, bloodpoolConfig, endCallback](RE::TESObjectREFR* bloodpoolRef, const bool result) {
				if (!result || !bloodpoolRef) {
					EmitBloodpoolEnd(endCallback, false, bloodpoolRef);
					return;
				}

				auto processLayer = [&](const bool isExtended) {
					RE::BSFixedString rootNodeName = !isExtended ? SettingsIni::sDecals_FileLayerBase : SettingsIni::sDecals_FileLayerExt;
					auto& settings = !isExtended ? bloodpoolConfig->settings : bloodpoolConfig->settingsExtended;
					auto& rig = !isExtended ? bloodpoolConfig->rig : bloodpoolConfig->rigExtended;
					auto& rigOrder = !isExtended ? bloodpoolConfig->rigOrder : bloodpoolConfig->rigOrderExtended;
					auto& geometry = !isExtended ? bloodpoolConfig->geometry : bloodpoolConfig->geometryExtended;

					if (auto* rootObj = bloodpoolRef->GetNodeByName(rootNodeName)) {
						if (auto* rootNode = rootObj->AsNode()) {
							BuildBloodpoolRig(rig, rootNode, p, isExtended);
							BuildBloodpoolGeometry(geometry, bloodpoolRef, isExtended);
							AssignClosestParent(rig);
							AssignRingNeighbors(rig);
							rigOrder = OrderRigByParent(rig);

							rootObj->local.translate.z += settings.offsetZ;
						}
					}
				};

				processLayer(false);
				if (bloodpoolConfig->hasExtended) {
					processLayer(true);
					ApplyExtendedRelativeTransform(bloodpoolRef, p);
				}

				NiUtils::UpdateObjectDownward(bloodpoolRef->Get3D());

				DisplayBloodpoolGeometry(bloodpoolRef, *bloodpoolConfig);

				const float baseDuration = bloodpoolConfig->settings.duration;
				float totalDuration = baseDuration;
				float extendedStartStep = 0.0f;

				if (bloodpoolConfig->hasExtended) {
					const float extendedDuration = bloodpoolConfig->settingsExtended.duration;
					const float blending = std::clamp(bloodpoolConfig->settingsExtended.blending, 0.0f, 1.0f);

					const float extendedStartTime = baseDuration * blending;
					totalDuration = std::max(baseDuration, extendedStartTime + extendedDuration);

					extendedStartStep = extendedStartTime / totalDuration;
				}

				auto success = std::make_shared<std::atomic_bool>(true);
				TimeUtils::DoWhileInGame(Utils::GetBloodpoolRefreshDelta,
					[success, p, bloodpoolConfig, extendedStartStep, totalDuration, endCallback](TimeUtils::CallResult result, const float progress) {
						auto* bloodpoolRef = MiscUtils::GetValidReference(bloodpoolConfig ? MiscUtils::ResolveHandle(bloodpoolConfig->refHandle) : nullptr, true);

						if (TimeUtils::IsEnd(result)) {
							if (result == TimeUtils::CallResult::kEndTimedOut) *success = false;
							EmitBloodpoolEnd(endCallback, *success, bloodpoolRef);
							return true;
						}

						if (!bloodpoolRef) { *success = false; return false; }

						if (Utils::IsReferenceTooFar(p)) return false;

						auto processRef = [&](const bool isExtended, const float localProgress) -> bool {
							RE::BSFixedString rootNodeName = !isExtended ? SettingsIni::sDecals_FileLayerBase : SettingsIni::sDecals_FileLayerExt;
							auto& settings = !isExtended ? bloodpoolConfig->settings : bloodpoolConfig->settingsExtended;
							auto& rig = !isExtended ? bloodpoolConfig->rig : bloodpoolConfig->rigExtended;
							auto& rigOrder = !isExtended ? bloodpoolConfig->rigOrder : bloodpoolConfig->rigOrderExtended;
							auto& geometry = !isExtended ? bloodpoolConfig->geometry : bloodpoolConfig->geometryExtended;

							if (bloodpoolConfig->hasExtended) {
								Utils::ApplyBloodpoolFade(bloodpoolRef, *bloodpoolConfig, isExtended, progress, localProgress);
							}

							if (auto* rootObj = bloodpoolRef->GetNodeByName(rootNodeName)) {
								if (auto* rootNode = rootObj->AsNode()) {
									const auto& flowCurve = !isExtended ? bloodpoolConfig->flowCurve : bloodpoolConfig->flowCurveExtended;
									const float flowedProgress = Utils::SampleFlowCurve(flowCurve, localProgress);
									const float easedProgress = Utils::ApplyEase(flowedProgress, settings.ease, settings.easeMidpoint);
									if (easedProgress <= 1.0f) {
										const bool result = Utils::ApplyBloodpoolScale(rootNode, settings, rig, rigOrder, easedProgress);
										
										using Method = ModData::PoolProfile::PoolVariant::Settings::Method;
										if (result && p.settings.method == Method::kReveal) {
											Utils::ApplyBloodpoolUVReveal(bloodpoolRef, geometry, settings.pivot, easedProgress);
										}
										
										return result;
									}

									return true;
								}
							}

							return false;
						};

						// Step 1 progression (relative)
						const float step1Duration = bloodpoolConfig->settings.duration / totalDuration;
						float step1Progress = (progress / step1Duration);

						if (step1Progress < 1.0f) {
							if (!processRef(false, step1Progress)) return false;
						}

						// Step 2 progression (relative)
						if (p.hasExtended) {
							if (progress >= extendedStartStep) {
								float step2Progress = std::clamp((progress - extendedStartStep) / (1.0f - extendedStartStep), 0.0f, 1.0f);
								if (!processRef(true, step2Progress)) return false;
							}
						}

						return true;
					},
				totalDuration);
			});
		}

	private:

		static bool SetBloodpoolInitialZAxis(InstanceParams& p)
		{
			if (!p.cell || !p.cell->IsAttached()) return false;

			auto* bhkWorld = p.cell->GetbhkWorld();
			if (!bhkWorld) return false;

			const float worldScale = RE::bhkWorld::GetWorldScale();

			static auto collisionGroup = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup();
			static auto cFilter = RE::CFilter{};
			cFilter.SetSystemGroup(collisionGroup);
			cFilter.SetCollisionLayer(RE::COL_LAYER::kPathPick);

			auto raycastZ = [&](RE::NiPoint3& position, RE::hkVector4& normal, float up, float down) -> bool {
				RE::NiPoint3 rayStart = position;
				RE::NiPoint3 rayEnd = position;

				rayStart.z += up;
				rayEnd.z -= down;

				RE::bhkPickData pickData;
				pickData.rayInput.from = rayStart * worldScale;
				pickData.rayInput.to = rayEnd   * worldScale;
				pickData.rayInput.filterInfo = cFilter;
				pickData.rayInput.enableShapeCollectionFilter = false;

				bhkWorld->PickObject(pickData);

				if (!pickData.rayOutput.HasHit()) return false;

				const RE::NiPoint3 hitPos = rayStart + (rayEnd - rayStart) * pickData.rayOutput.hitFraction;

				normal = pickData.rayOutput.normal;

				position.z = hitPos.z;
				return true;
			};

			// First layer Raycast
			RE::hkVector4 slopeVector;
			if (!raycastZ(p.settings.position, slopeVector, 12.0f, 128.0f)) return false;
			
			if (SettingsIni::bRaycast_AutoOrientSlope) {
				if (GetSlopeAngleDegrees(slopeVector) >= SettingsIni::fRaycast_AutoOrientSlopeMinDeg) {
					p.rotation = ComputeSlopeOrientedRotation(slopeVector, p.settings.pivot);
				}
			}

			// Second layer Raycast
			if (p.hasExtended) {
				const float slopeFactor = GetSlopeFactor(slopeVector);
				const float zDeltaMax = std::max((p.settingsExtended.size / 0.707f) * slopeFactor, SettingsIni::fRaycast_MaxStepHeight);
				
				p.settingsExtended.position = ComputeExtendedCenter(p);

				if (!raycastZ(p.settingsExtended.position, slopeVector, zDeltaMax, zDeltaMax)) {
					p.hasExtended = false;
				}
			}

			return true;
		}

		static float GetSlopeFactor(const RE::hkVector4& vec)
		{
			const float x = _mm_cvtss_f32(vec.quad);
			const float y = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(1, 1, 1, 1)));
			const float z = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(2, 2, 2, 2)));

			// Calculation of the difference between the z component and the average of the absolute values ​​of x and y
			const float slope = (1.0f - std::abs(z)) + ((std::abs(x) + std::abs(y)) / 2.0f);

			return std::clamp(slope, 0.0f, 1.0f);
		}

		static float GetSlopeAngleDegrees(const RE::hkVector4& normal)
		{
			const float z = std::abs(_mm_cvtss_f32(_mm_shuffle_ps(normal.quad, normal.quad, _MM_SHUFFLE(2, 2, 2, 2))));
			return std::acos(std::clamp(z, 0.0f, 1.0f)) * (180.0f / RE::NI_PI);
		}

		static float ComputeSlopeOrientedRotation(const RE::hkVector4& normal, const RE::NiPoint2& pivot)
		{
			const float nx = _mm_cvtss_f32(normal.quad);
			const float ny = _mm_cvtss_f32(_mm_shuffle_ps(normal.quad, normal.quad, _MM_SHUFFLE(1, 1, 1, 1)));

			const float yaw = std::atan2(ny, -nx);

			const float dx = pivot.x - 0.5f;
			const float dy = pivot.y - 0.5f;
			const float pivotAngle = (dx != 0.0f || dy != 0.0f) ? std::atan2(dy, dx) : 0.0f;

			const float angleDeg = (yaw + pivotAngle) * 180.0f / RE::NI_PI;

			return [](float a) {
				a = std::fmod(a, 360.0f);
				return a < 0.0f ? a + 360.0f : a;
			}(angleDeg);
		}


		static RE::NiPoint3 ComputeExtendedCenter(const InstanceParams& p)
		{
			const float sizeA = p.settings.size;
			const float sizeB = p.settingsExtended.size;

			const float localX = (sizeA * ((p.settings.pivot.x - 0.5f) * 2.0f)) - (sizeB * ((p.settingsExtended.pivot.x - 0.5f) * 2.0f));
			const float localY = (sizeA * ((p.settings.pivot.y - 0.5f) * 2.0f)) - (sizeB * ((p.settingsExtended.pivot.y - 0.5f) * 2.0f));

			const float rad = -p.rotation * (RE::NI_PI / 180.0f);
			const float cosR = std::cos(rad);
			const float sinR = std::sin(rad);

			const float rotatedX = localX * cosR - localY * sinR;
			const float rotatedY = localX * sinR + localY * cosR;

			RE::NiPoint3 result = p.settings.position;
			result.x -= rotatedX;
			result.y -= rotatedY;

			return result;
		}

		struct BloodpoolCacheHash
		{
			std::size_t operator()(const std::tuple<std::string, RE::BGSTextureSet*, RE::BGSTextureSet*>& key) const {
				auto h1 = std::hash<std::string>{}(std::get<0>(key));
				auto h2 = std::hash<RE::BGSTextureSet*>{}(std::get<1>(key));
				auto h3 = std::hash<RE::BGSTextureSet*>{}(std::get<2>(key));
        
				std::size_t seed = h1;
				seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				return seed;
			}
		};

		inline static std::unordered_map<
			std::tuple<std::string, RE::BGSTextureSet*, RE::BGSTextureSet*>, RE::TESObjectACTI*, BloodpoolCacheHash
		> cachedActivators;
		static RE::TESObjectREFR* CreateBloodpoolReference(const InstanceParams& p)
		{
			if (!p.settings.textureSet) return nullptr;

			auto* baseTex = p.settings.textureSet;
			auto* extTex = p.settingsExtended.textureSet ? p.settingsExtended.textureSet : baseTex;
			auto key = std::make_tuple(p.settings.model, baseTex, extTex);

			RE::TESObjectACTI* generatedForm = nullptr;

			if (auto it = cachedActivators.find(key); it != cachedActivators.end()) {
				generatedForm = it->second;
			} else {
				const auto activatorFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectACTI>();
				auto* duplicatedForm = activatorFactory ? activatorFactory->Create() : nullptr;
				if (!duplicatedForm) {
					logger::error("Failed to initialize Activator Factory.");
					return nullptr;
				}

				duplicatedForm->model = p.settings.model;
				duplicatedForm->flags.set(RE::TESObjectACTI::ActiFlags::kIgnoredBySandbox);
				duplicatedForm->AddKeyword(ModData::bloodpoolKeyword);

				auto* actiForm = duplicatedForm->As<RE::TESObjectACTI>();
				if (!actiForm) return nullptr;

				auto* swap = actiForm->As<RE::TESModelTextureSwap>();
				if (!swap) return nullptr;

				static const std::vector<std::string> nameStorage = []() {
					std::vector<std::string> v;
					v.reserve(SettingsIni::vDecalBaseGeometry.size() + SettingsIni::vDecalExtGeometry.size());
					v.insert(v.end(), SettingsIni::vDecalBaseGeometry.begin(), SettingsIni::vDecalBaseGeometry.end());
					v.insert(v.end(), SettingsIni::vDecalExtGeometry.begin(), SettingsIni::vDecalExtGeometry.end());
					return v;
				}();

				const std::uint32_t kTextureCount = static_cast<std::uint32_t>(nameStorage.size());
				swap->numAlternateTextures = kTextureCount;
				swap->alternateTextures = new RE::TESModelTextureSwap::AlternateTexture[kTextureCount]();

				for (std::uint32_t i = 0; i < nameStorage.size(); ++i) {
					auto& tex = swap->alternateTextures[i];
					tex.index3D = i;
					tex.name3D = nameStorage[i].c_str();
					tex.textureSet = (i < SettingsIni::vDecalBaseGeometry.size()) ? baseTex : extTex;
				}

				cachedActivators[key] = actiForm;
				generatedForm = actiForm;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !generatedForm) return nullptr;

			auto* placedRef = NativeUtils::PlaceAtMe(player, generatedForm, p.settings.position, { 0.0f, 0.0f, p.rotation * (RE::NI_PI / 180.0f) });
			if (!placedRef) return nullptr;

			placedRef->formFlags |= RE::TESForm::RecordFlags::kDisableFade;
			placedRef->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);

			return placedRef;
		}

		static void BuildBloodpoolGeometry(BloodpoolConfig::Geometry& geometry, RE::TESObjectREFR* bloodpoolRef, const bool extended)
		{
			if (!bloodpoolRef) return;

			const std::vector<std::string>& geoNames = !extended ? SettingsIni::vDecalBaseGeometry : SettingsIni::vDecalExtGeometry;

			for (const auto& name : geoNames) {
				if (auto* object = bloodpoolRef->GetNodeByName(name.c_str())) {
					if (auto* mesh = object->AsGeometry()) {
						geometry.push_back(mesh->name.c_str());
					}
				}
			}
		}

		static void BuildBloodpoolRig(BloodpoolConfig::NodesRig& rig, RE::NiNode* rootNode, const InstanceParams& p, const bool extended)
		{
			if (!rootNode) return;

			RE::NiPoint2 origin{
				(std::clamp(extended ? p.settingsExtended.pivot.x : p.settings.pivot.x, 0.0f, 1.0f) * 2.0f) - 1.0f,
				(std::clamp(extended ? p.settingsExtended.pivot.y : p.settings.pivot.y, 0.0f, 1.0f) * 2.0f) - 1.0f
			};

			const auto rootPos = rootNode->local.translate;
			float maxOffset = 0.0f;

			for (auto& child : rootNode->GetChildren()) {
				auto* node = child ? child->AsNode() : nullptr;
				if (!node) continue;

				const auto& pos = node->local.translate;
				const float dx = pos.x - rootPos.x;
				const float dy = pos.y - rootPos.y;

				maxOffset = std::max({ maxOffset, std::abs(dx), std::abs(dy) });
			}

			if (maxOffset <= 0.0001f) return;

			for (auto& child : rootNode->GetChildren()) {
				auto* node = child ? child->AsNode() : nullptr;
				if (!node) continue;

				const auto& pos = node->local.translate;
				const float dx = pos.x - rootPos.x - (origin.x * maxOffset);
				const float dy = pos.y - rootPos.y - (origin.y * maxOffset);
				const float dist = std::sqrt(dx * dx + dy * dy);

				BloodpoolConfig::NodeData data;
				data.name = node->name.c_str();
				data.offsetXY = { dx, dy };
				data.offsetXY.Unitize();
				data.depth = dist / maxOffset;

				rig.emplace(data.name, std::move(data));
				
				node->local.translate = RE::NiPoint3{};
			}
		}
		
		static BloodpoolConfig BuildBloodpoolConfig(const InstanceParams& p, RE::TESObjectREFR* bloodpoolRef)
		{
			BloodpoolConfig config;

			config.refHandle = bloodpoolRef ? bloodpoolRef->GetHandle() : RE::ObjectRefHandle{};

			constexpr float kOffsetStep = 0.05f;
			constexpr float kOffsetMax = 0.1f;

			static float offsetAccumulator = kOffsetStep;
			const float offsetZ = SettingsIni::fCore_HeightOffset + offsetAccumulator + MiscUtils::GetRandomNumber(0.0f, kOffsetStep / 2.0f);

			offsetAccumulator += (kOffsetStep * 2.0f);
			if (offsetAccumulator >= kOffsetMax) offsetAccumulator = 0.0f;

			// First layer
			config.settings.size = p.settings.size;
			config.settings.duration = p.settings.duration;
			config.settings.ease = p.settings.easePower;
			config.settings.easeMidpoint = p.settings.easeMidpoint;
			config.settings.offsetZ = offsetZ + (p.hasExtended && p.settingsExtended.onTop ? 0.0f : kOffsetStep);

			config.settings.pivot = p.settings.pivot;

			const std::uint32_t flowSeed = bloodpoolRef ? bloodpoolRef->formID : 0u;
			config.flowCurve = Utils::BuildFlowCurve(flowSeed);

			// Second layer
			if (p.hasExtended) {
				config.hasExtended = true;

				config.settingsExtended.size = p.settingsExtended.size;
				config.settingsExtended.duration = p.settingsExtended.duration;
				config.settingsExtended.blending = p.settingsExtended.blendFactor;
				config.settingsExtended.ease = p.settingsExtended.easePower;
				config.settingsExtended.easeMidpoint = p.settingsExtended.easeMidpoint;
				config.settingsExtended.offsetZ = offsetZ + (p.settingsExtended.onTop ? kOffsetStep : 0.0f);

				config.settingsExtended.fadeOut = p.settingsExtended.fadeOut;
				config.settingsExtended.fadeIn = p.settingsExtended.fadeIn;

				config.settingsExtended.pivot = p.settingsExtended.pivot;

				config.flowCurveExtended = Utils::BuildFlowCurve(flowSeed ^ 0xE0000000u);				
			}

			config.shaders = p.shaders;

			return config;
		}

		static void AssignClosestParent(BloodpoolConfig::NodesRig& rig)
		{
			constexpr float MinOffsetSimilarity = 0.5f;
			constexpr float MaxOffsetDiff = 0.5f;

			for (auto& [name, node] : rig) {
				float minDist = std::numeric_limits<float>::max();
				std::string closestParent;

				for (const auto& [otherName, otherNode] : rig) {
					if (otherName == name || otherNode.depth >= node.depth) continue;

					const float dot = node.offsetXY.x * otherNode.offsetXY.x + node.offsetXY.y * otherNode.offsetXY.y;

					if (dot < MinOffsetSimilarity) continue;

					if (std::abs(node.offsetXY.x - otherNode.offsetXY.x) > MaxOffsetDiff ||
						std::abs(node.offsetXY.y - otherNode.offsetXY.y) > MaxOffsetDiff)
						continue;

					RE::NiPoint2 nodePosXY  = node.offsetXY * node.depth;
					RE::NiPoint2 otherPosXY = otherNode.offsetXY * otherNode.depth;

					float dx = nodePosXY.x - otherPosXY.x;
					float dy = nodePosXY.y - otherPosXY.y;
					float dist = std::sqrt(dx * dx + dy * dy);

					if (dist < minDist) {
						minDist = dist;
						closestParent = otherName;
					}
				}

				node.parentName = closestParent;
			}
		}

		static void AssignRingNeighbors(BloodpoolConfig::NodesRig& rig)
		{
			constexpr float kDepthSimilarity = 0.12f;

			for (auto& [name, node] : rig) {
				const float angle = std::atan2(node.offsetXY.y, node.offsetXY.x);

				float bestPos = std::numeric_limits<float>::max();
				float bestNeg = std::numeric_limits<float>::max();
				std::string neighborA, neighborB;

				for (const auto& [otherName, other] : rig) {
					if (otherName == name) continue;
					if (std::abs(other.depth - node.depth) > kDepthSimilarity) continue;

					const float otherAngle = std::atan2(other.offsetXY.y, other.offsetXY.x);

					float diff = otherAngle - angle;
					while (diff <= -RE::NI_PI) diff += 2.0f * RE::NI_PI;
					while (diff > RE::NI_PI) diff -= 2.0f * RE::NI_PI;

					if (diff > 0.0f && diff < bestPos) {
						bestPos = diff;
						neighborA = otherName;
					} else if (diff < 0.0f && -diff < bestNeg) {
						bestNeg = -diff;
						neighborB = otherName;
					}
				}

				node.ringNeighborA = neighborA.empty() ? std::nullopt : std::make_optional(neighborA);
				node.ringNeighborB = neighborB.empty() ? std::nullopt : std::make_optional(neighborB);
			}
		}

		static BloodpoolConfig::RigOrder OrderRigByParent(const BloodpoolConfig::NodesRig& rig)
		{
			BloodpoolConfig::RigOrder ordered;
			ordered.reserve(rig.size());
			std::unordered_set<std::string> visited;

			std::function<void(const BloodpoolConfig::NodeData&)> visit = [&](const BloodpoolConfig::NodeData& node) {
				if (visited.count(node.name)) return;
				visited.insert(node.name);

				if (!node.parentName.empty()) {
					auto it = rig.find(node.parentName);
					if (it != rig.end()) visit(it->second);
				}

				ordered.push_back(node.name);
			};

			for (const auto& [name, node] : rig) visit(node);

			return ordered;
		}

		static void ApplyExtendedRelativeTransform(RE::TESObjectREFR* bloodpoolRef, const InstanceParams& p)
		{
			if (!bloodpoolRef) return;

			auto* rootObj = bloodpoolRef->GetNodeByName(SettingsIni::sDecals_FileLayerExt);
			if (!rootObj) return;

			const float sizeA = p.settings.size;
			const float sizeB = p.settingsExtended.size;

			const float localX =
				(sizeA * ((p.settings.pivot.x - 0.5f) * 2.0f)) -
				(sizeB * ((p.settingsExtended.pivot.x - 0.5f) * 2.0f));

			const float localY =
				(sizeA * ((p.settings.pivot.y - 0.5f) * 2.0f)) -
				(sizeB * ((p.settingsExtended.pivot.y - 0.5f) * 2.0f));

			rootObj->local.translate.x -= localX;
			rootObj->local.translate.y -= localY;

			const float deltaZ = p.settingsExtended.position.z - p.settings.position.z;

			if (auto* rootNode = rootObj->AsNode()) {
				for (auto& child : rootNode->GetChildren()) {
					auto* node = child ? child->AsNode() : nullptr;
					if (!node) continue;

					node->local.translate.z = deltaZ;
				}
			}
		}

		static void DisplayBloodpoolGeometry(RE::TESObjectREFR* bloodpoolRef, const BloodpoolConfig& bloodpoolConfig)
		{
			auto processGeometry = [&](const std::vector<std::string>& geometry) {
				for (const auto& nodeName : geometry) {
					if (auto* object = bloodpoolRef->GetNodeByName(nodeName)) {
						if (auto* mesh = object->AsGeometry()) {
							mesh->SetAppCulled(false);
							SetShaderProperties(mesh, bloodpoolConfig);
						}
					}
				}
			};

			processGeometry(bloodpoolConfig.geometry);
			if (bloodpoolConfig.hasExtended) {
				processGeometry(bloodpoolConfig.geometryExtended);
			}
		}

		static void SetShaderProperties(RE::BSGeometry* geometry, const BloodpoolConfig& bloodpoolConfig)
		{
			if (!geometry) return;

			using Shaders = ModData::PoolProfile::PoolVariant::Shaders;
			using Feature = RE::BSShaderMaterial::Feature;

			auto* lightingShader = geometry->lightingShaderProp_cast();
			if (!lightingShader) return;

			auto* initialMaterial = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (!initialMaterial) return;

			if (bloodpoolConfig.shaders.type == Shaders::Type::kGreyscale) {
				auto* hairTint = static_cast<RE::BSLightingShaderMaterialHairTint*>(initialMaterial);
				lightingShader->SetFlags(RE::BSShaderProperty::EShaderPropertyFlag8::kHairTint, true);
				hairTint->tintColor = bloodpoolConfig.shaders.tint;
			}

			lightingShader->emissiveColor->red = bloodpoolConfig.shaders.emissiveColor.red;
			lightingShader->emissiveColor->green = bloodpoolConfig.shaders.emissiveColor.green;
			lightingShader->emissiveColor->blue = bloodpoolConfig.shaders.emissiveColor.blue;
			lightingShader->emissiveMult = bloodpoolConfig.shaders.emissiveMultiple;

			initialMaterial->specularPower = bloodpoolConfig.shaders.glossiness;
			initialMaterial->specularColor = bloodpoolConfig.shaders.specularColor;
			initialMaterial->specularColorScale = bloodpoolConfig.shaders.specularStrength;
			initialMaterial->refractionPower = bloodpoolConfig.shaders.refractionStrength;
			initialMaterial->subSurfaceLightRolloff = bloodpoolConfig.shaders.subSurfaceLightRolloff;
			initialMaterial->rimLightPower = bloodpoolConfig.shaders.rimLightPower;
			initialMaterial->materialAlpha = bloodpoolConfig.shaders.alpha;

			lightingShader->SetMaterial(initialMaterial, true);
			lightingShader->SetupGeometry(geometry);
			lightingShader->FinishSetupGeometry(geometry);
		}

		static void EmitBloodpoolEnd(const std::function<void(bool, RE::TESObjectREFR*)>& endCallback = nullptr,
			const bool success = false, RE::TESObjectREFR* blodpoolRef = nullptr)
		{
			bool deleted = false;
			
			if (!success) {
				logger::warn("Bloodpool emission failed.");
				if (blodpoolRef && !blodpoolRef->IsDeleted()) {
					RE::GarbageCollector::GetSingleton()->Add(blodpoolRef, true);
					deleted = true;
				}
			}

			if (endCallback) endCallback(success, (deleted ? nullptr : blodpoolRef));
		}
	};
};
