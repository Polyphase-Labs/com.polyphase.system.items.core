#pragma once

#include "Asset.h"
#include "AssetRef.h"

#include <mutex>
#include <string>
#include <vector>

// ItemInfo
//
// Engine-authored item archetype asset. Holds every tunable a designer
// needs to author a pickup/inventory item — display strings, world mesh
// / HUD icon, behavior flags (autoUse, equipable, consumable, throwable,
// maxCount), per-category effect parameters (heal amount, ammo rounds,
// shield points, weapon archetype) and authored signal names for hooks.
//
// The Lua layer (Scripts/FPS/Items/Items.lua) dispatches effects through
// a per-category handler table:
//   Ammo     → weapon:AddReserve(amount)        (active or named weapon)
//   Health   → vitals:Heal(amount)
//   Shield   → vitals:Set("shield", current+amount, clamped)
//   Weapon   → player:_EquipFromArchetype(slot, mWeaponArchetype)
//   KeyItem, Currency, Generic → no built-in; pure signal + inventory
//
// Every pickup also emits onPickupSignal; later UseItem / Equip / Toss
// fire onUseSignal / onEquipSignal / onTossSignal. Empty asset fields
// fall back to default names ("FPS.Item.Pickup" / ".Used" / etc.).
//
// Lifecycle:
//   • Create() registers the instance with ItemInfoRegistry.
//   • Destroy() unregisters.
//   • LoadStream re-registers so editor edits propagate live.

class ItemInfo : public Asset
{
public:
    DECLARE_ASSET(ItemInfo, Asset);

    ItemInfo();
    ~ItemInfo();

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    // ----- Read-only accessors used by the Lua snapshot builder -----
    const std::string& GetItemName() const        { return mItemName; }
    const std::string& GetDisplayName() const     { return mDisplayName; }
    const std::string& GetDescription() const     { return mDescription; }
    int32_t            GetCategory() const        { return mCategory; }
    int32_t            GetMaxCount() const        { return mMaxCount; }
    bool               GetAutoUse() const         { return mAutoUse; }
    bool               GetEquipable() const       { return mEquipable; }
    bool               GetConsumable() const      { return mConsumable; }
    bool               GetThrowable() const       { return mThrowable; }
    int32_t            GetAmount() const          { return mAmount; }
    const std::string& GetAmmoForWeapon() const   { return mAmmoForWeapon; }
    const std::string& GetWeaponArchetype() const { return mWeaponArchetype; }
    const std::string& GetOnPickupSignal() const  { return mOnPickupSignal; }
    const std::string& GetOnUseSignal() const     { return mOnUseSignal; }
    const std::string& GetOnEquipSignal() const   { return mOnEquipSignal; }
    const std::string& GetOnTossSignal() const    { return mOnTossSignal; }
    const std::vector<std::string>& GetTagNames() const { return mTagNames; }

    // Asset-name view of cosmetic refs; the Lua side hands these to
    // LoadAsset / Audio.PlaySound3D / world:SpawnParticle directly.
    std::string GetIconName() const;
    std::string GetWorldMeshName() const;
    std::string GetPickupSoundName() const;
    std::string GetPickupVfxName() const;
    std::string GetUseSoundName() const;

protected:
    // ----- Identity --------------------------------------------------
    // mItemName falls back to GetName() when empty so renames don't
    // break LootTable string references.
    std::string mItemName;
    std::string mDisplayName;
    std::string mDescription;
    // 0=Ammo, 1=Health, 2=Shield, 3=Weapon, 4=KeyItem, 5=Currency, 6=Generic
    int32_t     mCategory = 6;
    // Free-form tag list (e.g. "Rare", "Quest", "Healing") — used by
    // LootTable conditionTag filtering and HUD coloring.
    std::vector<std::string> mTagNames;

    // ----- Behavior --------------------------------------------------
    int32_t mMaxCount = 99;
    bool mAutoUse    = true;
    bool mEquipable  = false;
    bool mConsumable = false;
    bool mThrowable  = false;

    // ----- Category data --------------------------------------------
    // Meaning depends on mCategory:
    //   Ammo     → rounds added (per stack count)
    //   Health   → HP healed
    //   Shield   → shield points restored
    //   Weapon   → starter reserve ammo when granted
    //   Currency → unit value
    //   Other    → arbitrary metadata
    int32_t mAmount = 0;
    // Ammo only: "" = active weapon, "*" = topUpAll, else exact name.
    std::string mAmmoForWeapon;
    // Weapon only: archetype name passed to player:_EquipFromArchetype.
    std::string mWeaponArchetype;

    // ----- Signals --------------------------------------------------
    // Empty strings resolve to defaults in Items._Emit:
    //   FPS.Item.Pickup / .Used / .Equipped / .Tossed
    std::string mOnPickupSignal;
    std::string mOnUseSignal;
    std::string mOnEquipSignal;
    std::string mOnTossSignal;

    // ----- FX asset refs --------------------------------------------
    TextureRef         mIcon;
    StaticMeshRef      mWorldMesh;
    SoundWaveRef       mPickupSound;
    ParticleSystemRef  mPickupVfx;
    SoundWaveRef       mUseSound;
};

// ---------------------------------------------------------------------------

// Process-wide registry of every loaded ItemInfo asset. Auto-populated
// by ItemInfo lifecycle hooks. Mirrors WeaponInfoRegistry's live-pointer
// design so editor renames Just Work.
class ItemInfoRegistry
{
public:
    static ItemInfoRegistry& Get();

    void Add(ItemInfo* info);
    void Remove(ItemInfo* info);

    ItemInfo* Find(const std::string& name) const;
    std::vector<std::string> ListNames() const;

    void Clear();

    // Force-load every ItemInfo stub the AssetManager has indexed.
    // Called from OnEditorReady (editor) and first Tick (shipped).
    int LoadAllFromAssetManager();

private:
    ItemInfoRegistry() = default;
    ItemInfoRegistry(const ItemInfoRegistry&) = delete;
    ItemInfoRegistry& operator=(const ItemInfoRegistry&) = delete;

    mutable std::recursive_mutex mMutex;
    std::vector<ItemInfo*> mAll;
};
