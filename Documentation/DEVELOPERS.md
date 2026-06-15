# Developers' Guide: `com.polyphase.system.items.core`

For programmers integrating items into gameplay scripts, adding new
category effects, or extending the asset schema.

## 1. Architecture

```
+----------------------------------------------------------------------+
|  Native (C++ addon)                                                  |
|                                                                      |
|  ItemInfo : Asset                ItemInfoRegistry                    |
|    - Identity / Behavior /         - live-pointer table              |
|      Category / Signals / FX       - Find(name) -> ItemInfo*         |
|    - SaveStream / LoadStream       - ListNames()                     |
|    - GatherProperties (inspector)  - LoadAllFromAssetManager()       |
|                                                                      |
|  Items_Lua.cpp binds the `Items` global:                             |
|    Items.GetItemInfo(name)  -> table | nil   (snapshot)              |
|    Items.ListItems()        -> { names }                             |
|    Items.HasItem(name)      -> bool                                  |
|    Items.LoadAllItems()     -> int                                   |
+------------------------------+---------------------------------------+
                               |
+------------------------------v---------------------------------------+
|  Project Lua (FPS Demo). Scripts/FPS/Items/Items.lua                 |
|                                                                      |
|    Items.Category   = { Ammo=0, Health=1, Shield=2, Weapon=3,        |
|                         KeyItem=4, Currency=5, Generic=6 }           |
|    Items.Defaults   = { onPickup="FPS.Item.Pickup", ... }            |
|    Items.Handlers   = { [Category.Ammo]=fn, [Category.Health]=fn,    |
|                         [Category.Shield]=fn, [Category.Weapon]=fn,  |
|                         [Category.KeyItem]=noop, ... }               |
|    Items._Emit      (asset, kind, ...)                               |
|    Items.Apply      (player, itemName, count) -> bool                |
|    Items.EnsureLoaded()                                              |
+----------------------------------------------------------------------+
```

The C++ side is just the data store and the snapshot accessor. Every
gameplay decision (what an Ammo pickup does, whether a Weapon pickup
auto-swaps, how Health interacts with vitals) is Lua. Override the
handlers per project without touching the addon.

## 2. Lua API

### `Items.GetItemInfo(name) -> table | nil`

Returns a flat snapshot of the `ItemInfo` asset's fields. Snapshot,
not a live ref. Mutating the returned table does not write back. Call
again to see new values after an editor edit.

Table shape:

```lua
{
    name            = "I_Health_Small",
    displayName     = "Small Medkit",
    description     = "...",
    category        = 1,                 -- Items.Category.Health
    tags            = { "Healing", "Common" },

    maxCount        = 5,
    autoUse         = true,
    equipable       = false,
    consumable      = true,
    throwable       = false,

    amount          = 25,
    ammoForWeapon   = "",                -- empty unless category=Ammo
    weaponArchetype = "",                -- empty unless category=Weapon

    onPickupSignal  = "",                -- empty falls back to Items.Defaults
    onUseSignal     = "",
    onEquipSignal   = "",
    onTossSignal    = "",

    icon            = "T_HUD_Medkit",    -- asset name, not a ref
    worldMesh       = "SM_Medkit_World",
    pickupSound     = "SFX_Medkit_Pickup",
    pickupVfx       = "P_Medkit_Sparkle",
    useSound        = "SFX_Medkit_Use",
}
```

Returns `nil` if no asset is registered under that name. The lookup
checks the live-pointer registry first, then falls back to
`AssetManager::GetAsset`, so a stub that hasn't been touched yet still
resolves.

### `Items.ListItems() -> { string, ... }`

Array of every `ItemInfo` name the registry knows. Cheap. Call freely
in editor tooling. In a shipped build the registry is fully populated
by first tick.

### `Items.HasItem(name) -> bool`

Boolean form of `Find != nil`. Use when you don't need the snapshot.

### `Items.LoadAllItems() -> int`

Force-loads every `ItemInfo` asset stub the `AssetManager` has
indexed. Returns how many were newly loaded this call (zero if
everything was already hot).

* In the editor, the addon fires this from `OnEditorReady` after the
  project loads. You usually don't need to call it.
* In shipped builds, the addon also fires it on first tick.
* Call explicitly from your project's startup script if you want the
  registry hot *before* the first tick (e.g. before splash screen
  logic needs to query items).

### `Items.EnsureLoaded()` (project-side helper)

Idempotent wrapper around `Items.LoadAllItems` defined in the
project's `Items.lua`. Safe to call from multiple Register nodes or
startup paths.

## 3. The dispatch model: `Items.Apply`

`Items.Apply(player, itemName, count) -> bool` is the single entry
point every code path uses to apply an item's effect:

* Pickup actors (`ItemPickup.lua`) call it on touch when
  `autoUse=true`.
* `FrameworkPlayer:UseItem` calls it when the player triggers an item
  from inventory.
* Mission and quest reward grants call it.
* Cheat and console commands call it.

Pseudocode of the dispatch (real impl in
`Scripts/FPS/Items/Items.lua:196`):

```lua
function Items.Apply(player, itemName, count)
    local asset = Items.GetItemInfo(itemName)
    if not asset then log_warn(); return false end
    local handler = Items.Handlers[asset.category]
    if not handler then log_warn(); return false end
    return handler(player, asset, count or 1) and true or false
end
```

A handler returns false when the effect couldn't apply: full ammo,
no matching weapon, full weapon slots, no shield headroom. The caller
(`ItemPickup`) treats false as "leave the pickup on the ground" so
the player can come back for it.

`Items.Apply` does NOT emit `onUseSignal`. That's a `UseItem`-time
concern. Callers emit the use signal themselves *before* calling
`Items.Apply`. Auto-pickup paths emit `onPickupSignal` instead. This
separation lets one item effect (heal) be triggered by multiple entry
points without double-firing signals.

### Built-in handlers (from `Scripts/FPS/Items/Items.lua`)

| Category | What the handler does | Fails when |
|---|---|---|
| Ammo | Adds `amount * count` rounds to a weapon's reserve based on `ammoForWeapon` (`""` for active, `"*"` for split thirds, exact name for archetype match). | Player has no matching weapon. |
| Health | `vitals:Heal(amount * count)`. | No `vitals` or no `Heal` method. |
| Shield | Reads `shield` and `maxShield`, clamps, `vitals:Set("shield", new)`. | Already at max shield (returns false, pickup stays). |
| Weapon | Tops up reserve if already carrying. Else finds a free slot, calls `player:_EquipFromArchetype`, grants `amount` reserve, emits `onEquipSignal`, swaps to it. | All three slots full and player doesn't already have the archetype. |
| KeyItem, Currency, Generic | No-op (returns true). The signal and inventory plumbing in `ItemPickup` handles everything else. |

## 4. Adding a custom category handler

The handlers table is just a Lua table. Override an entry from any
script that loads after `Items.lua`.

### Example: replace Health with a "blessed heal" that also restores shield

```lua
local _baseHealth = Items.Handlers[Items.Category.Health]

Items.Handlers[Items.Category.Health] = function(player, asset, count)
    local ok = _baseHealth(player, asset, count)
    if ok then
        local vitals = player:GetVitals()
        local bonus = math.floor((asset.amount or 0) * 0.25)
        local cur = vitals:Get("shield") or 0
        vitals:Set("shield", math.min(cur + bonus, vitals:Get("maxShield") or 0))
    end
    return ok
end
```

### Example: add a new "Poison" category at index 7

```lua
Items.Category.Poison = 7

Items.Handlers[Items.Category.Poison] = function(player, asset, count)
    if not player.ApplyStatus then return false end
    player:ApplyStatus("poison", {
        damage   = asset.amount * count,
        duration = 5.0,
    })
    return true
end
```

Then in the editor inspector, set the new item's Category to `7`. The
inspector lets you type any integer. The `Items.Category` table just
defines symbolic constants on the script side.

### Patching, not replacing

If you want surgical changes without forking the file, do the
override in a separate script that runs *after* `Items.lua`. The
registration order is up to your project's `Register*.lua` setup.

## 5. Signal emission: `Items._Emit`

```lua
Items._Emit(asset, kind, ...)
```

* `kind` is one of `"onPickup"`, `"onUse"`, `"onEquip"`, `"onToss"`.
* Reads `asset[kind .. "Signal"]`, e.g. `asset.onPickupSignal`.
* If that field is non-empty, uses it. Otherwise falls back to
  `Items.Defaults[kind]` (`"FPS.Item.Pickup"`, etc.).
* Calls `SignalBus.Emit(name, ...)`.

Pickup and inventory scripts call this. You usually don't call it
from gameplay code unless you're writing a new pickup variant.

If `SignalBus` isn't loaded (early boot, headless test harness),
`Items._Emit` is a silent no-op. Don't guard at the call site.

## 6. C++ surface

### `ItemInfo : Asset` (`Source/ItemInfo.h`)

* `DECLARE_ASSET(ItemInfo, Asset)` registers the type.
* Schema versioned. Writer emits `kItemInfoVersion = 1`, trailer
  magic `0xC0DEF11Bu`. Reader presence-checks the trailer so older
  streams (without it) still load with a single warning rather than
  corruption.
* `GatherProperties` builds the inspector layout. Properties are
  grouped under Identity, Behavior, CategoryData, Signals, and FX via
  `SCOPED_CATEGORY`. Asset slots are typed via the last argument to
  `Property(...)` so only matching asset types are droppable.
* Asset refs (`TextureRef`, `StaticMeshRef`, etc.) are stored as hard
  refs and saved/loaded via `Stream::WriteAsset` and `ReadAsset`.
* `Get*Name()` accessors return the underlying asset's name (or
  empty string if unset). Lua hands those strings to `LoadAsset`,
  `Audio.PlaySound3D`, and `world:SpawnParticle` directly.

### `ItemInfoRegistry` (singleton)

* `Get()` returns the global instance.
* `Add(info)` and `Remove(info)` are called from `Create` and
  `Destroy`, and from `LoadStream` so live editor edits propagate.
* `Find(name)` checks the live-pointer list first (so renames are
  picked up), then falls back to `AssetManager::GetAsset`. `name` is
  the `mItemName` field, or `Asset::GetName()` if `mItemName` is
  empty.
* `ListNames()` returns every registered item's resolved name.
* `LoadAllFromAssetManager()` walks the AssetManager's map and loads
  every `ItemInfo`-typed stub that hasn't been hydrated. Used by both
  `OnEditorReady` and the first-tick eager load in shipped builds.
* Thread-safe via a `std::recursive_mutex`. Re-entrancy matters
  because `LoadAllFromAssetManager` calls `mgr->LoadAsset`, which
  triggers `LoadStream`, which calls `Add()` again.

### Addon entry point (`ComPolyphaseSystemItemsCore.cpp`)

Stock Polyphase plugin shape:

* `OnLoad` calls `FORCE_LINK_CALL(ItemInfo)` to keep the static
  initializer alive.
* `RegisterScriptFuncs` calls `BindItemsLua(L)`.
* `RegisterEditorUI` (editor only) adds the `Items/ItemInfo`
  Create-Asset menu entry.
* `OnEditorReady` (editor only) eager-loads the registry.
* `Tick` (shipped) one-shot eager-loads on the first frame.
* `OnUnload` calls `ItemInfoRegistry::Get().Clear()` *before*
  `UnbindItemsLua` so no Lua callback can see a half-torn-down
  registry.

The addon mirrors the structural conventions of
`com.polyphase.engine.combat.core`: symbols, lifecycle order, type
color choice. If you're extending one, the patterns in the other will
look familiar.

## 7. Asset stream format (v1)

For tooling, data-mining, and save migrations. Read in this exact
order:

```
uint32 version  (= 1)

// Identity
string itemName
string displayName
string description
int32  category
uint32 tagCount
{ string tag }[tagCount]

// Behavior
int32 maxCount
bool  autoUse
bool  equipable
bool  consumable
bool  throwable

// Category data
int32  amount
string ammoForWeapon
string weaponArchetype

// Signals
string onPickupSignal
string onUseSignal
string onEquipSignal
string onTossSignal

// FX (Stream::WriteAsset format. Opaque ref blobs.)
asset  icon
asset  worldMesh
asset  pickupSound
asset  pickupVfx
asset  useSound

uint32 trailerMagic  (= 0xC0DEF11B)   // presence-checked, optional in v0-era streams
```

Trailer magic values used across sibling addons (don't reuse):

* `0xC0DEF11Au` for `WeaponInfo` (combat.core)
* `0xC0DEF11Bu` for `ItemInfo` (this addon)
* `0xC0DEF11Cu` for `LootTable` (rpg.loot)

When you add fields, bump `kItemInfoVersion` and gate them with
`if (version >= 2) { ... }` in `LoadStream` so older saved assets
keep loading.

## 8. Building

### Dependencies

* Polyphase Engine sources (headers and `Polyphase` import lib).
* Lua (vendored under `External/Lua` in the engine repo).
* Vulkan SDK (the engine's render headers transitively pull it).

### Resolving `POLYPHASE_PATH`

`CMakeLists.txt` resolves the engine root in this priority order:

1. `-DPOLYPHASE_PATH=...` on the command line.
2. `POLYPHASE_PATH` env var.
3. Walk up from `CMakeLists.txt` looking for `PolyphaseConfig.cmake`
   (written by the editor on project open. Canonical for addons
   living under `<project>/Packages`).
4. Walk up looking for `Polyphase.sln` (engine source root. Works
   when the addon is checked out next to the engine repo).
5. `C:/Polyphase` or `/opt/Polyphase` (default install).
6. A baked fallback path. Only useful on the original author's
   machine.

### Commands

```bash
# From the package root:
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the included scripts:

```bash
./build.bat        # Windows
./build.sh         # Linux or macOS
```

Output is `com.polyphase.system.items.core.dll` (or `.so` / `.dylib`).
Drop it into your project's `Packages/com.polyphase.system.items.core/`
folder. The editor picks it up on the next launch.

### Debug vs Release

The CMake script auto-points at `Standalone/Build/Windows/x64/DebugEditor`
or `ReleaseEditor` based on `CMAKE_BUILD_TYPE`. Matching the engine's
build flavor matters. Mixing a Debug addon with a Release engine will
either fail to link or crash at runtime on STL ABI mismatches.

## 9. Things this addon deliberately does NOT do

* No inventory implementation. Stack tracking, slot UI, "Use" verbs
  are all project-side. This addon only describes what an item *is*.
* No pickup actor. The `ItemPickup.lua` script in the FPS Demo is one
  example consumer. You'd replace it for a different game.
* No loot tables. See `com.polyphase.system.rpg.loot` for that.
* No userdata wrapping. `Items.GetItemInfo` returns a plain table
  snapshot. `UnbindItemsLua` is a stub today because there are no Lua
  metatables to tear down. It exists so the addon's lifecycle shape
  matches the combat addon's, ready for future per-item userdata.

These are intentional separations of concern. Adding any of them is a
fork or a sibling addon, not an extension of this one.

## 10. Test recipes

Quick smoke tests once the addon is built and a project has loaded:

```lua
-- Registry hot?
print(#Items.ListItems())            -- > 0

-- Round-trip an item:
local i = Items.GetItemInfo("I_Health_Small")
assert(i and i.category == Items.Category.Health)

-- Force a reload (returns newly-loaded count, should be 0 second time):
print(Items.LoadAllItems())
print(Items.LoadAllItems())

-- Apply against a real player:
print(Items.Apply(Player.Get(), "I_Health_Small", 1))
```

Stress tests and fuzz cases worth running once when changing the C++
side:

* Save, re-open, save again. Round-trip stability on every field.
* Editor rename of asset file. `Items.GetItemInfo(oldName)` should
  return `nil`, `(newName)` should succeed.
* Set `Item Name` field explicitly, then rename the asset. Lookups by
  `Item Name` keep working.
* Set Category to an unknown integer (e.g. `99`). Asset loads, but
  `Items.Apply` returns false and warns.
* Unload the addon (Edit > Reload Addons). `Items` global goes away.
  Reload restores it. No dangling pointers in the registry.

## 11. Related packages

| Package | Why it's relevant |
|---|---|
| `com.polyphase.engine.combat.core` | Provides `WeaponInfo` archetypes. `Items.Category.Weapon` items reference them by name via `Weapon Archetype`. |
| `com.polyphase.system.rpg.loot` | `LootTable` assets reference `ItemInfo` names via the same registry. Tag-based filtering uses `tagNames`. |
| `com.polyphase.engine.fps` | FPS player and inventory glue. Consumes `Items.Apply` from pickup and inventory paths. |
| `com.polyphase.system.character.core` | Vitals (`Heal`, shield `Get` and `Set`). The Health and Shield handlers call into this. |

The conventions across these packages (Asset color, trailer magic,
addon lifecycle, registry shape) deliberately match. If you're
writing a new sibling, copy this addon as a template.
