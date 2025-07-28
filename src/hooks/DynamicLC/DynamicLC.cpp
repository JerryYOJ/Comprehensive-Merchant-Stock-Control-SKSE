#include "DynamicLC.h"

#include "../../configmanager.h"

#include <xbyak.h>
#include <random>

extern "C" __declspec(dllexport) float MinNumMult = 1, MaxNumMult = 1, MinLevelMult = 1, MaxLevelMult = 1;

void DynamicLC::Install()
{
	REL::Relocation<std::uintptr_t> hookPoint{ RELOCATION_ID(16038, 16038), REL::VariantOffset(0x229, 0x229, 0x0) };
	
	_InitLeveledItems = SKSE::GetTrampoline().write_call<5>(hookPoint.address(), InitLeveledItems);
}

void DynamicLC::InitLeveledItems(RE::InventoryChanges* inv)
{
	_InitLeveledItems(inv);

	using _GetFormEditorID = const char* (*)(std::uint32_t);
	static auto tweaks = GetModuleHandle(L"po3_Tweaks");
	static auto GetFormEditorID = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));

	RE::FormID id = inv->owner->formID;
	if (inv->owner->Is(RE::FormType::Reference)) id = inv->owner->GetBaseObject()->formID;

	if (std::string(GetFormEditorID(id)).contains("Merchant")) {
		if (RE::BarterMenu::GetTargetRefHandle()) {
			auto&& cfg = ConfigManager::getInstance();
			RE::TESObjectREFRPtr trader;
			if (RE::TESObjectREFR::LookupByHandle(RE::BarterMenu::GetTargetRefHandle(), trader)) {
				for (auto&& item : *inv->entryList) {
					item->countDelta *= cfg.GetCountMultiplier(trader->As<RE::Actor>(), item, RE::PlayerCharacter::GetSingleton());
				}
			}
			else {
				logger::warn("trader handle lookup failed");
			}
		}
	}
	
	return;
}


// extern "C" __declspec(dllexport) creates an dll exported function that others can use
// RE::Actor* trader is the merchant's reference
// RE::InventoryEntryData* objDesc is the item player is looking at
// uint16_t a_level is the item's generated level form leveledlists
// RE::GFxValue& a_updateObj is the item's gfx object
// bool is_buying is whether the player is buying item from merchant (true) or selling to the merchant (false)
extern "C" __declspec(dllexport) float MerchantPriceCallback(RE::Actor* trader, RE::InventoryEntryData* objDesc, uint16_t a_level, RE::GFxValue& a_updateObj, bool is_buying) {
	if (is_buying) {
		auto&& cfg = ConfigManager::getInstance();
		return cfg.GetPriceMultiplier(trader, objDesc, RE::PlayerCharacter::GetSingleton());
	}
	
	return 1.0f;
}
