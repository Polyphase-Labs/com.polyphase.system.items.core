#pragma once

struct lua_State;

namespace polyphase
{
namespace items
{

// Bind the `Items` global into Lua. Idempotent.
//   Items.GetItemInfo(name) -> snapshot table | nil
//   Items.ListItems()       -> array of strings
//   Items.LoadAllItems()    -> integer count loaded this call
//   Items.HasItem(name)     -> bool
void BindItemsLua(lua_State* L);

// Tear down side state. Called from addon OnUnload before the
// ItemInfo registry is cleared.
void UnbindItemsLua();

} // namespace items
} // namespace polyphase
