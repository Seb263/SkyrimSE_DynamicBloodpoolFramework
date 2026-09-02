#include "API/Mod-API.h"

#include "Core/Init.hpp"

namespace DBF_API
{
    class Impl_V1 : public Interface_V1
    {
    public:
        static Impl_V1* GetSingleton() noexcept
        {
            static Impl_V1 instance;
            return &instance;
        }

        REL::Version GetVersion() noexcept override
        {
			const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
			const auto version{ plugin->GetVersion() };

			return version;
        }

		bool SpawnBloodpool(const Parameters parameters) noexcept override
		{
			return BloodPoolCore::Init::EmitBloodPool(parameters);
		}
    };
}

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI(DBF_API::InterfaceVersion version, const char* pluginName, REL::Version pluginVersion)
{
    if (!pluginName) {
        logger::error("DBF_API::RequestPluginAPI called with a nullptr plugin name");
        return nullptr;
    }

    void* api = nullptr;

    switch (version)
    {
        case DBF_API::InterfaceVersion::V1:
            api = DBF_API::Impl_V1::GetSingleton();
            break;
        default:
            logger::warn("RequestPluginAPI called with invalid InterfaceVersion {}", static_cast<uint8_t>(version));
            return nullptr;
    }

    logger::info("RequestPluginAPI called: [InterfaceVersion:{}], [PluginName:{}], [PluginVersion:{}]",
		static_cast<uint8_t>(version) + 1, pluginName, pluginVersion.string("."));

    return api;
}
