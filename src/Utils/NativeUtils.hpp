#pragma once

class NativeUtils
{
	public:

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* ref, RE::TESForm* baseForm, RE::NiPoint3 position, RE::NiPoint3 angle = {}, bool forcePersist = false)
	{
		const auto boundObject = baseForm->As<RE::TESBoundObject>();
		if (!boundObject || !ref) return nullptr;

		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(boundObject, position, angle, ref->GetParentCell(), ref->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), forcePersist, true);
		const auto handlePtr = handle.get();
		return (handlePtr && handlePtr.get() ? handlePtr.get() : nullptr);
	};
};
