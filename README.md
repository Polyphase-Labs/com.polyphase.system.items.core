# com.polyphase.system.items.core

Native Polyphase Engine addon that adds the `ItemInfo` asset type and the
`Items` Lua global. Designers author every pickup/inventory item — icon,
world mesh, description, tags, behavior flags, category effect parameters,
and signal names — directly in the editor inspector. Lua dispatches the
effect through `Items.Apply` or hands the item to the player inventory
based on the asset's `autoUse` flag.

> **Status:** v1.0.0 — schema v1 (Identity, Behavior, Category, Signals, FX).
> Ships with the Polyphase FPS Demo. Pairs with
> [`com.polyphase.system.rpg.loot`](../com.polyphase.system.rpg.loot) for
> loot tables and with `com.polyphase.engine.combat.core` for the weapon
> archetypes the `Weapon` category references.

---

## What this package gives you

| Surface | Where | Use it for |
|---|---|---|
| `ItemInfo` asset type | Editor → `Create Asset → Items → ItemInfo` | Authoring item archetypes (the data) |
| `Items` Lua global | Any Lua script after engine boot | Looking items up at runtime |
| `Items.Category` enum + `Items.Apply` dispatch | `Scripts/FPS/Items/Items.lua` (project side) | Applying an item's effect to the player |
| Signal emission | Project's `SignalBus` | Hooking HUD / VFX / achievements |

The C++ side only exposes raw queries — the per-category effect handlers
(Heal, Ammo, Shield, Weapon, etc.) live in the project's `Items.lua`
sugar layer so they stay overridable per game.

---

## Quick start

### 1. Make sure the addon is loaded

Drop the built `com.polyphase.system.items.core.dll` (or the source tree)
into your project's `Packages/` folder. The editor loads it automatically;
shipped builds eager-load the registry on first tick.

You should see this in the log on editor startup:

```
com.polyphase.system.items.core loaded (ItemInfo / Items Lua)
```

### 2. Create an item asset

In the Content Browser, right-click a folder and choose
**Create Asset → Items → ItemInfo**. Name it something stable like
`I_Health_Small` — that name is what other systems (loot tables,
pickup scripts) will reference.

Fill in the inspector. The minimum viable item is:

- **Identity → Item Name**: leave blank to use the asset name, or set
  explicitly if you want to rename the asset without breaking references.
- **Identity → Category**: `1` for Health (see the table in the
  [artists guide](Documentation/ARTISTS.md#category-cheat-sheet)).
- **Behavior → Auto Use**: `true` so it heals on pickup.
- **CategoryData → Amount**: `25` (HP healed).
- **FX → Icon / World Mesh / Pickup Sound / Pickup VFX**: assign any
  cosmetic refs you want.

Save the asset. Press Play. Drop an `ItemPickup` actor into the world
that references the asset and run over it — the player heals 25 HP and
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

---

## Read next

- **[Documentation/ARTISTS.md](Documentation/ARTISTS.md)** — every field
  on the inspector explained, category cheat-sheet, asset naming
  conventions, signal routing, and a walk-through for each category
  (ammo box, medkit, shield cell, weapon pickup, key item, currency).
- **[Documentation/DEVELOPERS.md](Documentation/DEVELOPERS.md)** —
  Lua API reference, `Items.Apply` dispatch model, how to add a custom
  category handler, C++ registry internals, asset stream format,
  build instructions.

---

## How it fits together

```
                    ┌──────────────────────────┐
   Editor ─────────►│  ItemInfo asset (.bin)   │
                    │  Identity / Behavior /   │
                    │  Category / Signals / FX │
                    └────────────┬─────────────┘
                                 │ LoadAsset
                                 ▼
                    ┌──────────────────────────┐
                    │   ItemInfoRegistry (C++) │
                    │   live-pointer lookup    │
                    └────────────┬─────────────┘
                                 │ Items.GetItemInfo / .ListItems
                                 ▼
                    ┌──────────────────────────┐
                    │   Items.Apply (Lua)      │
                    │   per-category handler   │◄── overrideable per project
                    └────┬───────┬───────┬─────┘
                         │       │       │
                Vitals:Heal  Weapon:AddReserve  Equip…
                         │       │       │
                         ▼       ▼       ▼
                    ┌──────────────────────────┐
                    │   SignalBus.Emit         │
                    │   FPS.Item.Pickup/Used…  │
                    └──────────────────────────┘
```

Pickup scripts call `Items.Apply` (or route to inventory if
`autoUse=false`). Mission rewards and console grants funnel through the
same entry point, so a heal applied from a cheat command is the same
code path as a heal applied from a world pickup.

---

## Building from source

The package builds as a shared library (`.dll` on Windows). Either run
the included `build.bat` / `build.sh`, or generate from CMake:

```bash
cmake -B build -S . -DPOLYPHASE_PATH=/path/to/Polyphase
cmake --build build --config Release
```

`POLYPHASE_PATH` auto-resolves by walking up from this directory looking
for `PolyphaseConfig.cmake` (set by the editor on project open) or
`Polyphase.sln` (engine source root). Set it explicitly only if your
layout is unusual.

See [Documentation/DEVELOPERS.md](Documentation/DEVELOPERS.md#building)
for the full toolchain notes.

---

## License & support

Authored by Justin Jaro / Polyphase. Part of the Polyphase Engine
example suite. Issues and feature requests via the main Polyphase
repository.
