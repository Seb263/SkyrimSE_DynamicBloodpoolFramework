#pragma once

#include "Core/Header.h"
#include "Core/Main.hpp"

#include "Utils/MiscUtils.hpp"

#include "API/Mod-API.h"

namespace BloodPoolCore
{
	class Init
	{
	public:

		static bool EmitBloodPool(const DBF_API::Interface::Parameters parameters)
		{
			using namespace ModData;

			if (parameters.profileID.empty()) return false;

			auto it = poolProfilesMapping.find(parameters.profileID);
			if (it == poolProfilesMapping.end()) return false;

			const PoolProfile& profile = it->second;
			if (profile.variants.empty()) return false;

			auto& variants = profile.variants;
			if (variants.empty()) return false;

			const int maxIndex = static_cast<int>(variants.size());
			const int index = static_cast<int>(std::floor(MiscUtils::GetRandomNumber(0.0f, static_cast<float>(maxIndex))));
			const auto& selected = variants[index];

			InstanceParams instance = BuildInstanceParams(selected, parameters);

			if (parameters.waitForStableOrigin && instance.refHandle) {
				WaitForStableOrigin(instance, parameters, [parameters, callback = parameters.callback](InstanceParams instance, bool success) {
					BuildInstancePositionParams(instance, parameters);
					if (!success || !instance.cell) {
						if (callback) callback(false, nullptr);
						return;
					}

					if (debugVerboseMode > 1) LogInstanceParams(instance);
					Main::EmitBloodpoolStart(instance, [callback](bool result, RE::TESObjectREFR* bloodpoolRef) {
						if (callback) callback(result, bloodpoolRef);
					});
				});
			} else {
				BuildInstancePositionParams(instance, parameters);
				if (!instance.cell) return false;

				if (debugVerboseMode > 1) LogInstanceParams(instance);
				Main::EmitBloodpoolStart(instance, [callback = parameters.callback](bool result, RE::TESObjectREFR* bloodpoolRef) {
					if (callback) callback(result, bloodpoolRef);
				});
			}

			return true;
		}

	private:

		static InstanceParams BuildInstanceParams(const ModData::PoolProfile::PoolVariant& variant, const DBF_API::Interface::Parameters parameters)
		{
			InstanceParams p;

			float durationMult = MiscUtils::GetRandomNumber(variant.settings.durationMultRange.x, variant.settings.durationMultRange.y);
			if (parameters.override.durationMult) durationMult = parameters.override.durationMult.value_or(1.0f);

			// First layer
			p.settings.method = variant.settings.method;
			p.settings.model = variant.settings.model;
			p.settings.textureSet = variant.settings.textureSet;
			p.settings.pivot = variant.settings.pivot;
			p.settings.size = MiscUtils::GetRandomNumber(variant.settings.sizeRange.x, variant.settings.sizeRange.y);
			p.settings.duration = variant.settings.duration * (durationMult / (variant.settingsExtended ? 2.0f : 1.0f));
			p.settings.easePower = variant.settings.easePower;
			p.settings.easeMidpoint = variant.settings.easeMidpoint;

			p.settings.size *= parameters.override.scale.value_or(1.0f);

			// Second Layer
			if (variant.settingsExtended) {
				p.hasExtended = true;

				const auto& ext = *variant.settingsExtended;
				p.settingsExtended.textureSet = ext.textureSet;
				p.settingsExtended.pivot = ext.pivot;
				p.settingsExtended.size = p.settings.size * MiscUtils::GetRandomNumber(ext.sizeMultRange.x, ext.sizeMultRange.y);
				p.settingsExtended.duration = ext.duration * (durationMult / 2.0f);
				p.settingsExtended.easePower = ext.easePower;
				p.settingsExtended.easeMidpoint = ext.easeMidpoint;
				p.settingsExtended.blendFactor = ext.blendFactor;
				p.settingsExtended.onTop = ext.onTop;

				p.settingsExtended.fadeOut = variant.settings.fadeOutRange;
				p.settingsExtended.fadeIn = ext.fadeInRange;
			}

			auto resolveNode = [&](const bool isPos) {
				auto& originNodeVariant = isPos ? parameters.originNodePos : parameters.originNodeRot;
				auto& outNodeName = isPos ? p.nodePos : p.nodeRot;

				RE::NiAVObject* node = nullptr;

				if (std::holds_alternative<RE::NiAVObject*>(originNodeVariant)) {
					if (node = std::get<RE::NiAVObject*>(originNodeVariant)) {
						outNodeName = node->name;
						if (isPos) {
							if (auto* ref = node->GetUserData()) p.refHandle = ref->GetHandle();
						}
					}
				} else if (std::holds_alternative<RE::BSFixedString>(originNodeVariant)) {
					if (parameters.originRef) {
						if (isPos) p.refHandle = parameters.originRef->GetHandle();

						if (auto nodeName = std::get<RE::BSFixedString>(originNodeVariant); !nodeName.empty()) {
							if (node = parameters.originRef->GetNodeByName(nodeName)) {
								outNodeName = node->name;
							}
						}

						if (outNodeName.empty()) {
							if (node = parameters.originRef->Get3D(false); node && !node->name.empty()) {
								outNodeName = node->name;
							}
						}
					}
				}
			};

			p.shaders = variant.shaders;

			p.refHandle = RE::ObjectRefHandle{};
			resolveNode(true);
			resolveNode(false);

			return p;
		}

		static void BuildInstancePositionParams(InstanceParams& instance, const DBF_API::Interface::Parameters parameters)
		{
			RE::NiAVObject* nodePos = nullptr;
			RE::NiAVObject* nodeRot = nullptr;
			if (auto* ref = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(instance.refHandle), true)) {
				if (nodePos = !instance.nodePos.empty() ? ref->GetNodeByName(instance.nodePos) : ref->Get3D(false)) {
					instance.settings.position = nodePos ? nodePos->world.translate : RE::NiPoint3{};
				}
				nodeRot = !instance.nodeRot.empty() ? ref->GetNodeByName(instance.nodeRot) : nullptr;
				if (!nodeRot && nodePos) nodeRot = nodePos;
			}

			RE::NiPoint3 dir;
			if (nodePos && nodeRot) {
				dir = nodePos->world.translate - nodeRot->world.translate;
			} else if (nodePos || nodeRot) {
				RE::NiPoint3 localDir = parameters.nodeSpreadDirection;
				localDir.Unitize();
				dir = (nodeRot ? nodeRot : nodePos)->world.rotate * localDir;
			} else {
				dir = parameters.nodeSpreadDirection;
			}

			dir.Unitize();
			const float yaw = atan2(dir.y, -dir.x);

			RE::NiPoint2 pivot = instance.settings.pivot;
			float dx = pivot.x - 0.5f;
			float dy = pivot.y - 0.5f;

			float pivotAngle = 0.0f;
			if (dx != 0.0f || dy != 0.0f) pivotAngle = atan2(dy, dx);

			float angleDeg = (yaw + pivotAngle) * 180.0f / RE::NI_PI;
			if (parameters.override.spread) {
				angleDeg += MiscUtils::GetRandomNumber(-parameters.override.spread.value(), parameters.override.spread.value());
			} else {
				angleDeg += MiscUtils::GetRandomNumber(-SettingsIni::fCore_RandomRotationDeg, SettingsIni::fCore_RandomRotationDeg);
			}

			instance.rotation = [](float a) {
				a = std::fmod(a, 360.0f);
				return a < 0.0f ? a + 360.0f : a;
			}(angleDeg);

			if (parameters.override.position) {
				instance.settings.position = parameters.override.position.value();
				instance.settingsExtended.position = parameters.override.position.value();
			}
			if (parameters.override.rotation) {
				instance.rotation = parameters.override.rotation.value();
			}

			SetCellFromPosition(instance, instance.settings.position);
		}

		static bool SetCellFromPosition(InstanceParams& p, const RE::NiPoint3 position)
		{
			const auto& tes = RE::TES::GetSingleton();
			if (!tes) return false;

			auto* cell = tes->interiorCell;
			if (cell && cell->IsAttached()) {
				p.cell = cell;
				return true;
			}

			if (const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; gridLength > 0) {
				for (std::uint32_t x = 0; x < gridLength; ++x) {
					for (std::uint32_t y = 0; y < gridLength; ++y) {
						if (auto* cellTmp = tes->gridCells->GetCell(x, y); cellTmp && cellTmp->IsAttached()) {
							if (const auto cellCoords = cellTmp->GetCoordinates(); cellCoords) {
								const RE::NiPoint2 worldPos{ cellCoords->worldX, cellCoords->worldY };

								if (position.x >= worldPos.x && position.x < worldPos.x + 4096.0f &&
									position.y >= worldPos.y && position.y < worldPos.y + 4096.0f) {
									p.cell = cellTmp;
									return true;
								}
							}
						}
					}
				}
			}

			return false;
		}

		static void WaitForStableOrigin(const InstanceParams p, const DBF_API::Interface::Parameters parameters,
			std::function<void(const InstanceParams, bool)> callback)
		{
			auto* ref = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(p.refHandle), true);
			if (!ref) {
				callback(p, false);
				return;
			}

			auto* origin = !p.nodePos.empty() ? ref->GetNodeByName(p.nodePos) : ref->Get3D(false);
			if (!origin) {
				callback(p, false);
				return;
			}

			auto lastPos = std::make_shared<RE::NiPoint3>();
			auto lastRot = std::make_shared<RE::NiPoint3>();
			auto stableEnd = std::make_shared<float>(std::numeric_limits<float>::max());
			auto success = std::make_shared<std::atomic_bool>(false);

			const auto updateDelay = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>(SettingsIni::fStability_PollDelay));
			const auto maxWaitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>(SettingsIni::fStability_MaxWaitDuration));

			if (const RE::Calendar* calendar = RE::Calendar::GetSingleton()) {
				*stableEnd = calendar->GetCurrentGameTime() + ((SettingsIni::fStability_StableDuration / 86400.0f) * calendar->GetTimescale());
			}

			TimeUtils::DoWhileInGame(updateDelay, [success, p, parameters, lastPos, lastRot, stableEnd, callback]
				(TimeUtils::CallResult result, const bool) {

					switch (result) {
					case TimeUtils::kEndDone:
						{
							if (*success) {
								InstanceParams pCopy = p;
								pCopy.settings.position = *lastPos;
								callback(pCopy, true);
							} else callback(p, false);
						}
						break;
					case TimeUtils::kLoopRepeat:
						{
							auto* ref = MiscUtils::GetValidReference(MiscUtils::ResolveHandle(p.refHandle), true);
							if (!ref) return false;

							auto* object = !p.nodePos.empty() ? ref->GetNodeByName(p.nodePos) : ref->Get3D(false);
							if (!object) return false;

							if (!NiUtils::IsReferenceRagdollReady(ref)) return true;

							RE::NiPoint3 pos = object->world.translate;
							float posDist = pos.GetDistance(*lastPos);
							*lastPos = pos;
							
							RE::NiPoint3 rot; object->world.rotate.ToEulerAnglesXYZ(rot);
							float rotDist = rot.GetDistance(*lastRot);
							*lastRot = rot;

							const RE::Calendar* calendar = RE::Calendar::GetSingleton();
							if (!calendar) return false;

							if (posDist <= SettingsIni::fStability_DistanceThreshold && rotDist <= SettingsIni::fStability_RotationThreshold) {
								if (calendar->GetCurrentGameTime() >= *stableEnd) {
									*success = true;
									return false;
								}
							} else {
								*stableEnd = calendar->GetCurrentGameTime() + ((SettingsIni::fStability_StableDuration / 86400.0f) * calendar->GetTimescale());
							}
						}
						break;
					case TimeUtils::kLoopTimeout:
						return false;
						break;
					}

					return true;
				},
			maxWaitDuration);
		}

		static void LogInstanceParams(const InstanceParams& p)
		{
			TRACE("InstanceParams:");
			auto* ref = MiscUtils::ResolveHandle(p.refHandle);
			TRACE("	 reference: {:08X}", ref ? ref->formID : 0x0);
			TRACE("	 nodePos: {}", p.nodePos.c_str());
			TRACE("	 nodeRot: {}", p.nodeRot.c_str());
			TRACE("	 cell: {:08X}", p.cell ? p.cell->formID : 0x0);
			TRACE("	 rotation: {}", p.rotation);

			auto logSettings = [](const auto& s, const char* name) {
				TRACE("	 {}:", name);
				TRACE("	   model: {}", !s.model.empty() ? s.model : "empty");
				TRACE("	   textureSet: {:08X}", s.textureSet ? s.textureSet->formID : 0x0);
				TRACE("	   pivot: ({}, {})", s.pivot.x, s.pivot.y);
				TRACE("	   size: {}", s.size);
				TRACE("	   duration: {}", s.duration);
				TRACE("	   easePower: {}", s.easePower);
				TRACE("	   easeMidpoint: {}", s.easeMidpoint);
				TRACE("	   blendFactor: {}", s.blendFactor);
				TRACE("	   onTop: {}", s.onTop);

				TRACE("	   translate: ({}, {}, {})", s.position.x, s.position.y, s.position.z);

				if (s.fadeOut.has_value()) TRACE("	   fadeOut: ({}, {})", s.fadeOut->x, s.fadeOut->y);
				else TRACE("	   fadeOut: none");

				if (s.fadeIn.has_value()) TRACE("	   fadeIn: ({}, {})", s.fadeIn->x, s.fadeIn->y);
				else TRACE("	   fadeIn: none");
			};

			logSettings(p.settings, "settings");
			logSettings(p.settingsExtended, "settingsExtended");

			TRACE("     shaders:");
			TRACE("        type: {}", p.shaders.type == ModData::PoolProfile::PoolVariant::Shaders::kGreyscale ? "kGreyscale" : "kDefault");
			TRACE("        tint: #{}", p.shaders.tint.ToHex());
			TRACE("        emissiveColor: #{}", p.shaders.emissiveColor.ToHex());
			TRACE("        emissiveMultiple: {}", p.shaders.emissiveMultiple);
			TRACE("        specularColor: #{}", p.shaders.specularColor.ToHex());
			TRACE("        specularStrength: {}", p.shaders.specularStrength);
			TRACE("        glossiness: {}", p.shaders.glossiness);
			TRACE("        refractionStrength: {}", p.shaders.refractionStrength);
			TRACE("        subSurfaceLightRolloff: {}", p.shaders.subSurfaceLightRolloff);
			TRACE("        rimLightPower: {}", p.shaders.rimLightPower);
			TRACE("        alpha: {}", p.shaders.alpha);
		}
	};
};
