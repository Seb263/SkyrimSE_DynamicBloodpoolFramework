#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Header.h"
#include "Core/Utils.hpp"

#include "Utils/TimeUtils.hpp"

namespace BloodPoolCore
{
	class Maintenance
	{
	public:

		static bool EnforceBloodpoolQuota(RE::TESObjectREFR* currentBloodpool)
		{
			if (!currentBloodpool) return false;

			const int maxRefs = SettingsIni::iMaintenance_MaxBloodpools;
			std::unordered_map<RE::TESObjectREFR*, float> bloodpoolDistances;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return false;

			RE::TES::GetSingleton()->ForEachReference([&](RE::TESObjectREFR* a_ref) {
				if (!MiscUtils::GetValidReference(a_ref, true) || !a_ref->HasKeyword(ModData::bloodpoolKeyword)) {
					return RE::BSContainer::ForEachResult::kContinue;
				}

				bloodpoolDistances[a_ref] = player->GetPosition().GetDistance(a_ref->GetPosition());
				return RE::BSContainer::ForEachResult::kContinue;
			});

			if (static_cast<int>(bloodpoolDistances.size()) <= maxRefs) return false;
			
			bool currentBloodpoolDeleted = false;
			while (static_cast<int>(bloodpoolDistances.size()) > maxRefs) {
				auto farthestIt = std::max_element(
					bloodpoolDistances.begin(), bloodpoolDistances.end(),
					[](const auto& a, const auto& b) { return a.second < b.second; }
				);

				if (farthestIt != bloodpoolDistances.end()) {
					auto* ref = farthestIt->first;

					if (ref == currentBloodpool) currentBloodpoolDeleted = true;

					bloodpoolDistances.erase(farthestIt);
					ref->formFlags |= RE::TESForm::RecordFlags::kDeleted;
					if (SettingsIni::fMaintenance_DespawnDuration > 0.0f) SmoothDelete(ref);
					else RE::GarbageCollector::GetSingleton()->Add(ref, true);
				}
			}

			return currentBloodpoolDeleted;
		}

		static void SmoothDelete(RE::TESObjectREFR* bloodpoolRef)
		{
			if (!bloodpoolRef) return;
			TimeUtils::WaitUntil3DReady(bloodpoolRef, [](RE::TESObjectREFR* bloodpoolRef, const bool result) {
				if (!result || !bloodpoolRef) return;

				TimeUtils::DoWhileInGame(Utils::GetBloodpoolRefreshDelta,
					[bloodpoolHandle = bloodpoolRef->GetHandle()](TimeUtils::CallResult result, const float progress) {
						auto* bloodpoolRef = MiscUtils::ResolveHandle(bloodpoolHandle);
						if (!bloodpoolRef) return false;

						if (TimeUtils::IsEnd(result)) {
							RE::GarbageCollector::GetSingleton()->Add(bloodpoolRef, true);
							return true;
						}

						auto* model = bloodpoolRef->Get3D();
						if (!model) return false;

						RE::BSVisit::TraverseScenegraphGeometries(model, [&](RE::BSGeometry* geometry) {
							if (geometry) {
								geometry->UpdateMaterialAlpha(1.0f - progress, false);
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});

						return true;
					},
				SettingsIni::fMaintenance_DespawnDuration);
			});
		}
	};
};
