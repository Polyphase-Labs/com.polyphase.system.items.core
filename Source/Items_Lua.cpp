#include "Items_Lua.h"

#include "ItemInfo.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <string>
#include <vector>

namespace polyphase
{
namespace items
{

namespace
{

// Items.GetItemInfo(name) -> table | nil
//
// Returns a flat snapshot of the ItemInfo asset's fields so Lua can
// render HUDs, drive Items.Apply dispatch, and spawn world pickups
// without holding a C++ pointer. Mirror of Combat.GetWeaponInfo
// (Combat_Lua.cpp:557) — keep the same shape conventions so script
// authors familiar with weapons can read items immediately.
int ItemsGetItemInfo(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    ItemInfo* info = ItemInfoRegistry::Get().Find(name);
    if (info == nullptr) { lua_pushnil(L); return 1; }

    lua_createtable(L, 0, 22);

    // Identity
    lua_pushstring(L,  info->GetItemName().c_str());        lua_setfield(L, -2, "name");
    lua_pushstring(L,  info->GetDisplayName().c_str());     lua_setfield(L, -2, "displayName");
    lua_pushstring(L,  info->GetDescription().c_str());     lua_setfield(L, -2, "description");
    lua_pushinteger(L, info->GetCategory());                lua_setfield(L, -2, "category");

    // Tag list as a Lua array.
    {
        const auto& tags = info->GetTagNames();
        lua_createtable(L, (int)tags.size(), 0);
        for (size_t i = 0; i < tags.size(); ++i)
        {
            lua_pushstring(L, tags[i].c_str());
            lua_rawseti(L, -2, (int)(i + 1));
        }
        lua_setfield(L, -2, "tags");
    }

    // Behavior flags
    lua_pushinteger(L, info->GetMaxCount());                lua_setfield(L, -2, "maxCount");
    lua_pushboolean(L, info->GetAutoUse() ? 1 : 0);         lua_setfield(L, -2, "autoUse");
    lua_pushboolean(L, info->GetEquipable() ? 1 : 0);       lua_setfield(L, -2, "equipable");
    lua_pushboolean(L, info->GetConsumable() ? 1 : 0);      lua_setfield(L, -2, "consumable");
    lua_pushboolean(L, info->GetThrowable() ? 1 : 0);       lua_setfield(L, -2, "throwable");

    // Category data
    lua_pushinteger(L, info->GetAmount());                  lua_setfield(L, -2, "amount");
    lua_pushstring(L,  info->GetAmmoForWeapon().c_str());   lua_setfield(L, -2, "ammoForWeapon");
    lua_pushstring(L,  info->GetWeaponArchetype().c_str()); lua_setfield(L, -2, "weaponArchetype");

    // Signals (empty string = use Items.Defaults fallback)
    lua_pushstring(L,  info->GetOnPickupSignal().c_str());  lua_setfield(L, -2, "onPickupSignal");
    lua_pushstring(L,  info->GetOnUseSignal().c_str());     lua_setfield(L, -2, "onUseSignal");
    lua_pushstring(L,  info->GetOnEquipSignal().c_str());   lua_setfield(L, -2, "onEquipSignal");
    lua_pushstring(L,  info->GetOnTossSignal().c_str());    lua_setfield(L, -2, "onTossSignal");

    // FX asset names — Lua hands these to LoadAsset / Audio / Particles.
    lua_pushstring(L,  info->GetIconName().c_str());        lua_setfield(L, -2, "icon");
    lua_pushstring(L,  info->GetWorldMeshName().c_str());   lua_setfield(L, -2, "worldMesh");
    lua_pushstring(L,  info->GetPickupSoundName().c_str()); lua_setfield(L, -2, "pickupSound");
    lua_pushstring(L,  info->GetPickupVfxName().c_str());   lua_setfield(L, -2, "pickupVfx");
    lua_pushstring(L,  info->GetUseSoundName().c_str());    lua_setfield(L, -2, "useSound");

    return 1;
}

// Items.ListItems() -> { "I_Ammo_Pistol", "I_Health_Small", ... }
int ItemsListItems(lua_State* L)
{
    std::vector<std::string> names = ItemInfoRegistry::Get().ListNames();
    lua_createtable(L, (int)names.size(), 0);
    for (size_t i = 0; i < names.size(); ++i)
    {
        lua_pushstring(L, names[i].c_str());
        lua_rawseti(L, -2, (int)(i + 1));
    }
    return 1;
}

// Items.HasItem(name) -> bool
int ItemsHasItem(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    ItemInfo* info = ItemInfoRegistry::Get().Find(name);
    lua_pushboolean(L, info != nullptr ? 1 : 0);
    return 1;
}

// Items.LoadAllItems() -> integer (count of newly loaded stubs)
//
// Mirror of Combat.LoadAllWeapons — projects call this from Startup.lua
// after the engine reports AssetManager ready, to populate the registry
// in shipped builds where OnEditorReady never fires.
int ItemsLoadAllItems(lua_State* L)
{
    int n = ItemInfoRegistry::Get().LoadAllFromAssetManager();
    lua_pushinteger(L, n);
    return 1;
}

} // namespace

void BindItemsLua(lua_State* L)
{
    if (L == nullptr) return;

    lua_newtable(L);
    int t = lua_gettop(L);

    lua_pushcfunction(L, ItemsGetItemInfo);   lua_setfield(L, t, "GetItemInfo");
    lua_pushcfunction(L, ItemsListItems);     lua_setfield(L, t, "ListItems");
    lua_pushcfunction(L, ItemsHasItem);       lua_setfield(L, t, "HasItem");
    lua_pushcfunction(L, ItemsLoadAllItems);  lua_setfield(L, t, "LoadAllItems");

    lua_setglobal(L, "Items");
}

void UnbindItemsLua()
{
    // No userdata metatables yet — registry teardown lives in OnUnload
    // through ItemInfoRegistry::Get().Clear(). This stub exists so the
    // entry point mirrors the Combat addon shape; future Item userdata
    // would unregister metatables here.
}

} // namespace items
} // namespace polyphase
