#pragma once

#include "DataHandler.hpp"
#include "Events.h"
#include "JSONHandler.h"
#include "SettingsIni.hpp"

#include "Core/Init.hpp"

#include "API/Mod-API.h"

namespace ModData
{
	class DataHandler
	{
	public:
		bool preLoaded = false;
		bool postLoaded = false;
		bool postLoadedAlternate = false;

		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		std::future<void> loadFuture;

		void WaitUntilReady()
		{
			if (SettingsIni::bGeneral_AsynchronousStartup && loadFuture.valid()) {
				loadFuture.get();
			}
		}

		void PreLoadData()
		{
			if (preLoaded) return;
			preLoaded = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();
			
			ExtractGameAssets();
			LoadPluginsForms();
			CreateBloodpoolKeyword();

			Events::ModEventSink::LoadEvents();

			auto loadAndInitialize = []() {
				SKSE::GetTaskInterface()->AddTask([]() {
					JSONHandler::Main::LoadMappings();
				});
			};

			if (SettingsIni::bGeneral_AsynchronousStartup) loadFuture = std::async(std::launch::async, loadAndInitialize);
			else loadAndInitialize();
		}

		void PostLoadData()
		{
			if (postLoaded) return;
			postLoaded = true;

			WaitUntilReady();
		}

		void PostLoadDataAlternate()
		{
			if (postLoadedAlternate) return;
			postLoadedAlternate = true;

			TimeUtils::DoWhile(100ms, [](TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (TimeUtils::IsEnd(result)) return true;

				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
					GetSingleton()->PostLoadData();
					return false;
				}

				return true;
			}, true);
		}

	private:
		static inline void LoadPluginsForms()
		{
			logger::info("Loading Plugins Froms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}

			logger::info("Loading Plugins Froms Data: DONE");
		}

		static inline void ExtractGameAssets()
		{
			constexpr unsigned char PscBytes[] = {
				#include "DynamicBloodpoolFramework.psc.h"
			};

			constexpr unsigned char PexBytes[] = {
				#include "DynamicBloodpoolFramework.pex.h"
			};

			const std::string_view PscData{ reinterpret_cast<const char*>(PscBytes), sizeof(PscBytes) - 1 };

			const std::string_view PexData{ reinterpret_cast<const char*>(PexBytes), sizeof(PexBytes) - 1 };

			struct AssetEntry
			{
				std::string_view data;
				std::string_view dest;
				bool isSource;
			};

			const std::array<AssetEntry, 2> assets{{
				{ PscData, "Data/Source/Scripts/DynamicBloodpoolFramework.psc", true },
				{ PexData, "Data/Scripts/DynamicBloodpoolFramework.pex", false }
			}};

			for (const auto& asset : assets) {
				if (asset.isSource && !SettingsIni::bGeneral_ExtractScriptSources) {
					TRACE("ExtractGameAssets: Skipping source script '{}' (ExtractScriptSources disabled).", asset.dest);
					continue;
				}
				try {
					const std::size_t srcHash = std::hash<std::string_view>{}(asset.data);
					const std::filesystem::path destPath(asset.dest);
					if (std::filesystem::exists(destPath)) {
						std::ifstream existing(destPath, std::ios::binary);
						if (existing) {
							const std::string destData{
								std::istreambuf_iterator<char>{existing},
								std::istreambuf_iterator<char>{}
							};
							const std::size_t destHash = std::hash<std::string>{}(destData);
							if (srcHash == destHash) {
								TRACE("ExtractGameAssets: Asset '{}' is up-to-date, skipping.", asset.dest);
								continue;
							}
							if (!SettingsIni::bGeneral_OverwriteInvalidScripts) {
								TRACE("ExtractGameAssets: Asset '{}' differs but overwrite is disabled, skipping.", asset.dest);
								continue;
							}
							TRACE("ExtractGameAssets: Asset '{}' differs, replacing.", asset.dest);
						}
					}
					std::filesystem::create_directories(destPath.parent_path());

					std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
					if (!out) {
						logger::error("ExtractGameAssets: Failed to open output stream for '{}'.", asset.dest);
						continue;
					}

					out.write(asset.data.data(), static_cast<std::streamsize>(asset.data.size()));
					if (!out) {
						std::filesystem::remove(destPath);
						logger::error("ExtractGameAssets: Failed to write '{}'.", asset.dest);
						continue;
					}
					TRACE("ExtractGameAssets: Asset '{}' extracted successfully.", asset.dest);
				} catch (const std::exception& e) {
					logger::error("ExtractGameAssets: Exception extracting '{}': {}", asset.dest, e.what());
				}
			}
		}

		static inline void CreateBloodpoolKeyword()
		{
			logger::info("Creating Bloodpool Keyword...");

			const auto keywordFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSKeyword>();
			auto* createdKeyword = keywordFactory ? keywordFactory->Create() : nullptr;
			if (!createdKeyword) REPORT_AND_FAIL("Failed to initialize Keyword Factory.");

			bloodpoolKeyword = createdKeyword;

			logger::info("Creating Bloodpool Keyword: DONE");
		}
	};
}
