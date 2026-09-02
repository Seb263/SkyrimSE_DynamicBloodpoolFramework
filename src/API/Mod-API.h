#pragma once

/*******************************************************************
* DYNAMIC BLOODPOOL FRAMEWORK - API
* Do not forget to include this source file to your project!
*******************************************************************/

/* How to create a hook to the API and use it:
SKSE::GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) 
{
    switch (message->type) 
    {
        case MessagingInterface::kPostLoadGame:
        case MessagingInterface::kNewGame:
        {
            if (auto* apiInterface = static_cast<DBF_API::Interface*>(DBF_API::GetAPI())) {
				auto apiVersion = apiInterface->GetVersion().string(".");
				logger::info("Dynamic Bloodpool Framework API v{} registered successfully.", apiVersion);
			} else {
				logger::warn("Dynamic Bloodpool Framework API not found.");
			}
        }
        break;
    }
});
*/

namespace DBF_API
{
	inline void* g_Interface = nullptr;

	enum class InterfaceVersion : uint8_t
	{
		V1,
		Latest = V1
	};

    class Interface_V1
    {
    public:
		virtual ~Interface_V1() = default;

		struct Parameters
		{
			std::string profileID = "";
			RE::TESObjectREFR* originRef = nullptr;

			std::variant<RE::NiAVObject*, RE::BSFixedString> originNodePos = nullptr;
			std::variant<RE::NiAVObject*, RE::BSFixedString> originNodeRot = nullptr;
			RE::NiPoint3 nodeSpreadDirection = {0.0f, 0.0f, 1.0f};
			bool waitForStableOrigin = true;

			struct Override
			{
				std::optional<RE::NiPoint3> position = std::nullopt;
				std::optional<float> rotation = std::nullopt;
				std::optional<float> scale = std::nullopt;
				std::optional<float> spread = std::nullopt;
				std::optional<float> durationMult = std::nullopt;
			} override;

			std::function<void(bool, RE::TESObjectREFR*)> callback = nullptr;
		};

		virtual REL::Version GetVersion() noexcept = 0;

		virtual bool SpawnBloodpool(const Parameters parameters) noexcept = 0;
    };

	using Interface = Interface_V1;

	using _RequestPluginAPI = void* (*)(InterfaceVersion version, const char* pluginName, REL::Version pluginVersion);

    inline void* GetAPI(InterfaceVersion version = InterfaceVersion::Latest)
    {
        if (g_Interface) return g_Interface;

        const auto handle = GetModuleHandleA("DynamicBloodpoolFramework.dll");
        if (!handle) return nullptr;

        const auto request = reinterpret_cast<_RequestPluginAPI>(GetProcAddress(handle, "RequestPluginAPI"));
        if (!request) return nullptr;

        const auto plugin = SKSE::PluginDeclaration::GetSingleton();
        g_Interface = request(version, std::string(plugin->GetName()).c_str(), plugin->GetVersion());

        return g_Interface;
    }
}
