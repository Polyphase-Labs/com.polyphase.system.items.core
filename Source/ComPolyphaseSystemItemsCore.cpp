/**
 * @file ComPolyphaseSystemItemsCore.cpp
 * @brief Native addon: com.polyphase.system.items.core
 *
 * Exposes an Items global to Lua and registers the ItemInfo asset type
 * so designers can author items in the editor. Pairs with the FPS
 * project's Scripts/FPS/Items/Items.lua glue layer (per-category
 * effect handlers) and Scripts/FPS/Framework/ItemPickup.lua (the
 * unified pickup script). See com.polyphase.system.rpg.loot for the
 * matching LootTable asset.
 */

#include "Plugins/PolyphasePluginAPI.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "Plugins/EditorUIHooks.h"

#include "ItemInfo.h"
#include "Items_Lua.h"

#include "AssetManager.h"
#include "ScriptUtils.h"

static PolyphaseEngineAPI* sEngineAPI = nullptr;

static int OnLoad(PolyphaseEngineAPI* api)
{
    sEngineAPI = api;
    // Pin the ItemInfo TU so DECLARE_ASSET / DEFINE_ASSET static
    // initializer doesn't get DCE'd by the linker.
    FORCE_LINK_CALL(ItemInfo);
    if (api != nullptr && api->LogDebug != nullptr)
    {
        api->LogDebug("com.polyphase.system.items.core loaded (ItemInfo / Items Lua)");
    }
    return 0;
}

static void OnUnload()
{
    // Drop registry FIRST so nothing tries to look up an ItemInfo with
    // dangling AssetRef backing during asset-manager teardown.
    ItemInfoRegistry::Get().Clear();
    polyphase::items::UnbindItemsLua();
    if (sEngineAPI != nullptr && sEngineAPI->LogDebug != nullptr)
    {
        sEngineAPI->LogDebug("com.polyphase.system.items.core unloaded.");
    }
    sEngineAPI = nullptr;
}

static void RegisterTypes(void* /*nodeFactory*/)
{
    // ItemInfo registers via DECLARE_ASSET / DEFINE_ASSET static
    // initializers; FORCE_LINK_CALL in OnLoad pins the TU.
}

static void RegisterScriptFuncs(lua_State* L)
{
    polyphase::items::BindItemsLua(L);

    // Project-side Scripts/FPS/Registers/ItemsRegister.lua loads the
    // Lua sugar layer (Items.Apply, Items._Emit, category handlers).
    // We don't RunScript it here for the same reason combat.core
    // doesn't (the engine's class-name extractor mangles dotted
    // package paths).
}

#if EDITOR
static void CreateItemInfoAsset(void* /*userData*/)
{
    AssetManager* mgr = AssetManager::Get();
    if (mgr == nullptr) return;
    mgr->CreateAndRegisterAsset(
        ItemInfo::GetStaticType(),
        GetCurrentAssetDir(),
        "NewItem",
        false);
}

static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    if (hooks == nullptr) return;
    if (hooks->AddCreateAssetItem != nullptr)
    {
        hooks->AddCreateAssetItem(hookId, "Items/ItemInfo",
                                  &CreateItemInfoAsset, nullptr);
    }
}

// Force-load every ItemInfo stub the AssetManager has indexed. Editor
// fires this once after the project has loaded so the registry is hot
// before the user hits Play.
static void OnEditorReady()
{
    ItemInfoRegistry::Get().LoadAllFromAssetManager();
}
#endif

// Shipped builds: deferred eager-load on first Tick (AssetManager isn't
// ready in OnLoad). Combat.core uses the same pattern.
static bool sEagerLoadDone = false;
static void Tick(float /*dt*/)
{
    if (!sEagerLoadDone)
    {
        sEagerLoadDone = true;
        ItemInfoRegistry::Get().LoadAllFromAssetManager();
    }
}

static int FillDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "com.polyphase.system.items.core";
    desc->pluginVersion = "1.0.0";
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;
    desc->RegisterTypes = RegisterTypes;
    desc->RegisterScriptFuncs = RegisterScriptFuncs;
#if EDITOR
    desc->RegisterEditorUI = RegisterEditorUI;
#else
    desc->RegisterEditorUI = nullptr;
#endif
    desc->OnEditorPreInit = nullptr;
#if EDITOR
    desc->OnEditorReady = OnEditorReady;
#else
    desc->OnEditorReady = nullptr;
#endif
    desc->Tick = Tick;
    desc->TickEditor = nullptr;
    return 0;
}

#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#else
extern "C" int PolyphasePlugin_GetDesc_com_polyphase_system_items_core(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#endif
