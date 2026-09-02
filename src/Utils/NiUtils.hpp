#pragma once

class NiUtils
{

public:

	static bool IsReferenceRagdollReady(RE::TESObjectREFR* ref)
	{
		if (!ref || !ref->Is3DLoaded()) return false;

		RE::NiAVObject* niAVObject = ref->Get3D();
		if (!niAVObject) return false;
		
		auto* hkpRigidBody = GetRigidBody(niAVObject);
		if (hkpRigidBody && hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return true;

		return false;
	}

	static RE::hkpRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (!a_object) return nullptr;

		const auto collisionObject = a_object->GetCollisionObject();
		if (!collisionObject) return nullptr;

		const auto bhkRigidBody = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
		if (!bhkRigidBody || !bhkRigidBody->referencedObject) return nullptr;

		const auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(bhkRigidBody->referencedObject.get());
		return hkpRigidBody;
	}

	static void UpdateObject(RE::NiAVObject* object, SKSE::stl::enumeration<RE::NiUpdateData::Flag, std::uint32_t> flags = RE::NiUpdateData::Flag::kNone, float updateTime = 0.f)
	{
		if (!object) return;

		auto updateData = RE::NiUpdateData{ updateTime, flags };
		object->Update(updateData);
	}

	static void UpdateObjectDownward(RE::NiAVObject* object, SKSE::stl::enumeration<RE::NiUpdateData::Flag, std::uint32_t> flags = RE::NiUpdateData::Flag::kNone, float updateTime = 0.0f)
	{
		if (!object) return;

		auto updateData = RE::NiUpdateData{ updateTime, flags };
		object->UpdateDownwardPass(updateData, 0);
	}
};
