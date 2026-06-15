# com.polyphase.system.items.core

Polyphase Engine addon that adds the `ItemInfo` asset type and an `Items`
Lua global. Designers author pickup and inventory items in the editor
inspector (icon, mesh, description, tags, behavior flags, category
effect parameters, signal names). Lua dispatches the effect through
`Items.Apply`, or hands the item to the player inventory when the
asset's `autoUse` flag is off.

Version 1.0.0. Schema v1 covers Identity, Behavior, Category, Signals,
and FX. Ships with the Polyphase FPS Demo. Pairs with
`com.polyphase.system.rpg.loot` for loot tables and with
`com.polyphase.engine.combat.core` for the weapon archetypes that the
Weapon category references.

## What you get

| Surface | Where | Use it for |
|---|---|---|
| `ItemInfo` asset type | Editor: Create Asset > Items > ItemInfo | Authoring item archetypes |
| `Items` Lua global | Any Lua script after engine boot | Looking items up at runtime |
| `Items.Category` enum and `Items.Apply` dispatch | `Scripts/FPS/Items/Items.lua` (project side) | Applying an item's effect to the player |
| Signal emission | Project's `SignalBus` | Hooking HUD, VFX, achievements |

The C++ side only exposes raw queries. Per-category effect handlers
(Heal, Ammo, Shield, Weapon) live in the project's `Items.lua` sugar
layer so they stay overridable per game.

## Quick start

### 1. Confirm the addon is loaded

Drop the built `com.polyphase.system.items.core.dll` (or the source
tree) into your project's `Packages/` folder. The editor loads it
automatically. Shipped builds eager-load the registry on first tick.

Check the log on editor startup:

```
com.polyphase.system.items.core loaded (ItemInfo / Items Lua)
```

### 2. Create an item asset

In the Content Browser, right-click a folder and choose
**Create Asset > Items > ItemInfo**. Give it a stable name like
`I_Health_Small`. That name is the key other systems (loot tables,
pickup scripts) will reference.

Fill in the inspector. A minimal item needs:

* Identity > Item Name: blank to inherit the asset name, or set
  explicitly if you want to rename the asset without breaking refs.
* Identity > Category: `1` for Health (full list in the
  [artists guide](Documentation/ARTISTS.md#category-cheat-sheet)).
* Behavior > Auto Use: `true` so it heals on pickup.
* CategoryData > Amount: `25` (HP healed).
* FX > Icon, World Mesh, Pickup Sound, Pickup VFX: any cosmetic refs
  you want.

Save the asset. Press Play. Drop an `ItemPickup` actor into the world
that references the asset and run over it. The player heals 25 HP and
`FPS.Item.Pickup` fires on the SignalBus.

### 3. Read it from Lua

```lua
-- Anywhere after engine boot:
local asset = Items.GetItemInfo("I_Health_Small")
print(asset.displayName, asset.category, asset.amount)
-- "Small Medkit", 1, 25

-- Apply the effect directly:
Items.Apply(player, "I_Health_Small", 1)
```

## Read next

* [Documentation/ARTISTS.md](Documentation/ARTISTS.md). Every inspector
  field explained, the category cheat-sheet, asset naming conventions,
  signal routing, and a worked example for each category (ammo box,
  medkit, shield cell, weapon pickup, key item, currency).
* [Documentation/DEVELOPERS.md](Documentation/DEVELOPERS.md). Lua API
  reference, the `Items.Apply` dispatch model, how to add a custom
  category handler, C++ registry internals, asset stream format, and
  build instructions.

## How it fits together

```
                    +--------------------------+
   Editor --------->|  ItemInfo asset (.bin)   |
                    |  Identity / Behavior /   |
                    |  Category / Signals / FX |
                    +------------+-------------+
                                 | LoadAsset
                                 v
                    +--------------------------+
                    |   ItemInfoRegistry (C++) |
                    |   live-pointer lookup    |
                    +------------+-------------+
                                 | Items.GetItemInfo / .ListItems
                                 v
                    +--------------------------+
                    |   Items.Apply (Lua)      |
                    |   per-category handler   |  <-- overrideable per project
                    +----+-------+-------+-----+
                         |       |       |
                Vitals:Heal  Weapon:AddReserve  Equip...
                         |       |       |
                         v       v       v
                    +--------------------------+
                    |   SignalBus.Emit         |
                    |   FPS.Item.Pickup/Used.. |
                    +--------------------------+
```

Pickup scripts call `Items.Apply` (or route to inventory if `autoUse`
is false). Mission rewards and console grants funnel through the same
entry point, so a heal applied from a cheat command runs the same code
path as a heal applied from a world pickup.

## Building from source

The package builds as a shared library (`.dll` on Windows). Run the
included `build.bat` or `build.sh`, or invoke CMake directly:

```bash
cmake -B build -S . -DPOLYPHASE_PATH=/path/to/Polyphase
cmake --build build --config Release
```

`POLYPHASE_PATH` auto-resolves by walking up from this directory
looking for `PolyphaseConfig.cmake` (set by the editor on project open)
or `Polyphase.sln` (engine source root). Set it explicitly if your
layout is unusual.

Full toolchain notes:
[Documentation/DEVELOPERS.md](Documentation/DEVELOPERS.md#building).

## License and support

Authored by Justin Jaro / Polyphase. Part of the Polyphase Engine
example suite. File issues and feature requests against the main
Polyphase repository.
