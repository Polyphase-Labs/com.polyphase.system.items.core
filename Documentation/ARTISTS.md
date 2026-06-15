# Artists' Guide: Authoring `ItemInfo` Assets

For designers, level artists, and tech artists authoring items in the
editor. No code required.

To wire a new *kind* of item effect (poison status, deployable shield,
thrown grenade), that's a code change. See
[DEVELOPERS.md](DEVELOPERS.md#adding-a-custom-category-handler). This
doc covers everything you can do without touching code.

## 1. Creating a new item

1. In the Content Browser, navigate to where you want the asset to
   live, e.g. `Assets/Items/Health/`.
2. Right-click, then **Create Asset > Items > ItemInfo**.
3. Name it. Prefix conventions used in the FPS Demo:
   * `I_Ammo_*` for ammo packs
   * `I_Health_*` for medkits and stim packs
   * `I_Shield_*` for shield cells
   * `I_Weapon_*` for weapon pickups
   * `I_Key_*` for quest and key items
   * `I_Coin_*` for currency or pickups with score value
4. Double-click to open the inspector.

The asset's chip in the Content Browser is gold. That's how you spot
an `ItemInfo` versus a `WeaponInfo` (orange) or `LootTable` (cyan).

## 2. The inspector, field by field

### Identity

| Field | What it does | Tips |
|---|---|---|
| Item Name | The string other systems (loot tables, pickup actors, Lua) use to find this item. | Leave blank to use the asset name. Set it explicitly when you need to rename the asset file without breaking references. |
| Display Name | The pretty name shown in HUD pickups, inventory grids, tooltips. | "Small Medkit", not "I_Health_Small". |
| Description | One-paragraph flavor or mechanical text for tooltips and inspect screens. | Designers can rewrite this without touching code. |
| Category | Integer that picks the effect handler. See the cheat-sheet below. | Change this and the inspector "CategoryData" fields take on new meaning. Re-read the per-category sections. |
| Tag Names | Free-form list of tags (e.g. `Rare`, `Quest`, `Healing`, `Holiday2024`). | Loot tables can filter on these (`conditionTag`). HUDs use them for rarity coloring. Tags are pure metadata. No engine behavior is hard-coded against any specific tag. |

#### Category cheat-sheet

| Value | Category | Effect |
|---|---|---|
| 0 | Ammo | Adds rounds to a weapon's reserve |
| 1 | Health | Heals the player |
| 2 | Shield | Restores shield points |
| 3 | Weapon | Grants or tops up a weapon archetype |
| 4 | KeyItem | No built-in effect. Pure signal plus inventory entry. |
| 5 | Currency | No built-in effect. Pure signal plus inventory entry. |
| 6 | Generic | No built-in effect. Pure signal plus inventory entry. |

If you set Category to a number not in this list, the asset loads
fine, but `Items.Apply` will warn and refuse to apply it.

### Behavior

| Field | What it does |
|---|---|
| Max Count | Upper bound for how many of this item the inventory will hold. Once at max, additional pickups either drop or are ignored. That's the inventory's call, not this asset's. |
| Auto Use | If `true`, picking the item up applies its effect immediately (a medkit heals on touch). If `false`, the pickup adds it to inventory and the player triggers the effect manually with `UseItem`. |
| Equipable | Marks the item as something the player can equip versus just consume. The inventory UI uses this to show an "Equip" verb. Weapons usually leave this `false` because the Weapon category handles equipping itself. |
| Consumable | Marks the item as consumable for the inventory UI and loot filters. |
| Throwable | Marks the item as throwable. The pickup script's "toss" path fires `onTossSignal`. |

### CategoryData

These fields change meaning depending on the Category. The inspector
shows all three fields all the time. Ignore the ones that don't apply
to your category.

| Field | When category is... | Meaning |
|---|---|---|
| Amount | Ammo | Rounds added (per stack count) |
| | Health | HP healed |
| | Shield | Shield points restored |
| | Weapon | Starter reserve ammo granted with the weapon |
| | Currency | Unit value (1 coin = 1, 1 gem = 50, etc.) |
| | KeyItem or Generic | Free-form metadata. No engine reads it. |
| Ammo For Weapon | Ammo | Which weapon archetype this ammo feeds. See routing rules below. |
| | anything else | Ignored |
| Weapon Archetype | Weapon | The Combat archetype name to grant (e.g. `WI_Pistol`). |
| | anything else | Ignored |

#### `Ammo For Weapon` routing

* Empty string (`""`). Refills the player's *active* weapon. Generic
  "ammo box" pickups use this.
* `"*"`. Splits the rounds across every weapon the player is
  carrying (rough thirds). For universal ammo crate pickups.
* Exact name (e.g. `WI_Pistol`). Only refills that specific weapon
  archetype. If the player isn't carrying it, the pickup does nothing
  and the handler returns false. The pickup script will leave it on
  the ground.

### Signals

Empty falls back to the project defaults:

| Field | Default if blank | When it fires |
|---|---|---|
| On Pickup Signal | `FPS.Item.Pickup` | World pickup touches the player |
| On Use Signal | `FPS.Item.Used` | Player triggers the item from inventory |
| On Equip Signal | `FPS.Item.Equipped` | A Weapon-category item is equipped to a slot |
| On Toss Signal | `FPS.Item.Tossed` | Player tosses the item from inventory |

Set a custom name when you want this specific item to trigger a unique
hook. For example, `FPS.Quest.AncientKey.Found` for a story item that
should trigger a cinematic. Anything subscribed to that signal name
will fire.

### FX

| Slot | Type | Used for |
|---|---|---|
| Icon | Texture | HUD pickup popup, inventory grid, tooltip thumbnail |
| World Mesh | StaticMesh | The 3D model shown when the item is sitting on the floor |
| Pickup Sound | SoundWave | One-shot played at the pickup point on touch |
| Pickup VFX | ParticleSystem | One-shot spawned at the pickup point on touch |
| Use Sound | SoundWave | One-shot played when the item is consumed (heal sip, ammo click) |

All five are optional. Leave a slot blank and that step is skipped.
Slots are typed: dropping a Sound into the Icon slot won't take.

## 3. Worked examples

### Small medkit (heal-on-touch)

| Field | Value |
|---|---|
| Item Name | `I_Health_Small` |
| Display Name | "Small Medkit" |
| Description | "Restores 25 HP. Field stim. Works through armor." |
| Category | `1` (Health) |
| Tag Names | `Healing`, `Common` |
| Max Count | `5` |
| Auto Use | `true` |
| Amount | `25` |
| Icon, World Mesh, Pickup Sound, Pickup VFX, Use Sound | as needed |

Player walks over it, heals 25 HP, `FPS.Item.Pickup` emits.

### Pistol ammo box (active-weapon ammo)

| Field | Value |
|---|---|
| Item Name | `I_Ammo_Pistol` |
| Category | `0` (Ammo) |
| Auto Use | `true` |
| Amount | `30` |
| Ammo For Weapon | `WI_Pistol` |

Player walks over it. If they're carrying a `WI_Pistol`, its reserve
gains 30 rounds. If not, the pickup stays on the ground (returns
false).

### Universal ammo crate

Same as above but:

| Field | Value |
|---|---|
| Item Name | `I_Ammo_Universal` |
| Amount | `90` |
| Ammo For Weapon | `*` |

90 rounds get split across every weapon the player carries (~30 each
across three slots).

### Weapon pickup (shotgun)

| Field | Value |
|---|---|
| Item Name | `I_Weapon_Shotgun` |
| Category | `3` (Weapon) |
| Auto Use | `true` |
| Amount | `12` |
| Weapon Archetype | `WI_Shotgun` |

* If the player isn't carrying a shotgun and has a free slot: equip
  the shotgun, give it 12 reserve, auto-swap to it, fire
  `FPS.Item.Equipped`.
* If the player is already carrying a shotgun: top up reserve by 12.
* If all weapon slots are full and they don't have a shotgun: the
  pickup stays on the ground.

### Key item (story / quest)

| Field | Value |
|---|---|
| Item Name | `I_Key_AncientRune` |
| Display Name | "Ancient Rune" |
| Category | `4` (KeyItem) |
| Tag Names | `Quest`, `MainStory` |
| Auto Use | `false` |
| On Pickup Signal | `FPS.Quest.AncientRune.Found` |

Doesn't auto-use, doesn't stack. Picking it up adds it to inventory
and emits the custom signal. Quest scripts subscribed to that signal
handle the actual story progression.

### Coin (currency)

| Field | Value |
|---|---|
| Item Name | `I_Coin_Gold` |
| Category | `5` (Currency) |
| Auto Use | `true` |
| Amount | `1` |

Touch to collect. Score and wallet systems listening on
`FPS.Item.Pickup` read `amount` from the asset to add value.

## 4. Asset hygiene

* Don't rename `Item Name` for an item that's already in saves or
  loot tables. The name is the key. Change it and references break.
  Rename the asset file freely (the `Item Name` field overrides), but
  treat `Item Name` itself as a hard contract once shipped.
* Tags are case-sensitive. `rare` and `Rare` are different tags to
  the loot filter.
* Category changes are destructive. Flipping Health to Weapon
  silently reinterprets the `Amount`, `Ammo For Weapon`, and
  `Weapon Archetype` fields. Re-validate the asset after any category
  change.
* Live edits propagate. While the editor is running, edits to an
  `ItemInfo` re-register it in the registry on save. Lua callers that
  pick the asset up after the save see the new values without a
  reload. Shipped builds load once and stay.

## 5. Troubleshooting

| Symptom | Likely cause |
|---|---|
| `Items.Apply: no ItemInfo registered for 'X'` in log | The `Item Name` field doesn't match what the caller asked for. Check spelling, or the asset wasn't loaded. Try `Items.LoadAllItems()` from the Lua console. |
| Pickup touches the player but nothing happens | `Auto Use` is `false` (so it just enters inventory), or the category handler returned false (e.g. ammo type the player isn't carrying, weapon slots all full). |
| Heal or ammo gives the wrong amount | `Amount` is per-stack and gets multiplied by the pickup's `count`. A medkit with `Amount=25` picked up as a stack-of-2 heals 50. |
| Trailer magic mismatch warning in log | Asset was saved by a future version of the addon, or got corrupted. Re-save from the editor. |
| Custom signal doesn't fire | The asset's signal field is blank, so it falls back to the default. Fill it in. Also check that whatever's listening on the SignalBus subscribed to the right name. |

## 6. Where to look next

* Inventory UI behavior (icon rendering, stacking, "Use" verbs).
  Project-side code, not this addon.
* Loot tables (chance-based grants from chests or enemies). See the
  `com.polyphase.system.rpg.loot` package.
* Weapon archetypes (what `Weapon Archetype` points at). See the
  `com.polyphase.engine.combat.core` package.
* Scripting your own item effect. See [DEVELOPERS.md](DEVELOPERS.md).
