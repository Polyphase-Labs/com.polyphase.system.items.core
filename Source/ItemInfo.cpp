#include "ItemInfo.h"

#include "AssetManager.h"
#include "Assets/ParticleSystem.h"
#include "Assets/SoundWave.h"
#include "Assets/StaticMesh.h"
#include "Assets/Texture.h"
#include "Log.h"
#include "Property.h"
#include "Stream.h"

#include <algorithm>

FORCE_LINK_DEF(ItemInfo);
DEFINE_ASSET(ItemInfo);

namespace
{
    // Distinct trailer magic from WeaponInfo (0xC0DEF11Au) and LootTable
    // (0xC0DEF11Cu) so version-skew warnings name the right asset type.
    constexpr uint32_t kItemInfoTrailerMagic = 0xC0DEF11Bu;
    // v1: initial schema (identity + behavior + category + signals + FX).
    constexpr uint32_t kItemInfoVersion = 1;
}

ItemInfo::ItemInfo()
{
    mType = ItemInfo::GetStaticType();
}

ItemInfo::~ItemInfo() = default;

void ItemInfo::Create()
{
    Asset::Create();
    ItemInfoRegistry::Get().Add(this);
}

void ItemInfo::Destroy()
{
    ItemInfoRegistry::Get().Remove(this);
    Asset::Destroy();
}

const char* ItemInfo::GetTypeName()
{
    return "ItemInfo";
}

glm::vec4 ItemInfo::GetTypeColor()
{
    // Gold — distinct from WeaponInfo (orange) and LootTable (cyan).
    return glm::vec4(0.95f, 0.80f, 0.20f, 1.0f);
}

std::string ItemInfo::GetIconName() const
{
    if (Asset* a = mIcon.Get()) return a->GetName();
    return {};
}
std::string ItemInfo::GetWorldMeshName() const
{
    if (Asset* a = mWorldMesh.Get()) return a->GetName();
    return {};
}
std::string ItemInfo::GetPickupSoundName() const
{
    if (Asset* a = mPickupSound.Get()) return a->GetName();
    return {};
}
std::string ItemInfo::GetPickupVfxName() const
{
    if (Asset* a = mPickupVfx.Get()) return a->GetName();
    return {};
}
std::string ItemInfo::GetUseSoundName() const
{
    if (Asset* a = mUseSound.Get()) return a->GetName();
    return {};
}

void ItemInfo::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteUint32(kItemInfoVersion);

    // Identity
    stream.WriteString(mItemName);
    stream.WriteString(mDisplayName);
    stream.WriteString(mDescription);
    stream.WriteInt32(mCategory);
    stream.WriteUint32((uint32_t)mTagNames.size());
    for (const auto& s : mTagNames) stream.WriteString(s);

    // Behavior
    stream.WriteInt32(mMaxCount);
    stream.WriteBool(mAutoUse);
    stream.WriteBool(mEquipable);
    stream.WriteBool(mConsumable);
    stream.WriteBool(mThrowable);

    // Category data
    stream.WriteInt32(mAmount);
    stream.WriteString(mAmmoForWeapon);
    stream.WriteString(mWeaponArchetype);

    // Signals
    stream.WriteString(mOnPickupSignal);
    stream.WriteString(mOnUseSignal);
    stream.WriteString(mOnEquipSignal);
    stream.WriteString(mOnTossSignal);

    // FX
    stream.WriteAsset(mIcon);
    stream.WriteAsset(mWorldMesh);
    stream.WriteAsset(mPickupSound);
    stream.WriteAsset(mPickupVfx);
    stream.WriteAsset(mUseSound);

    stream.WriteUint32(kItemInfoTrailerMagic);
}

void ItemInfo::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    const uint32_t version = stream.ReadUint32();
    (void)version; // reserved for future field gating

    // Identity
    stream.ReadString(mItemName);
    stream.ReadString(mDisplayName);
    stream.ReadString(mDescription);
    mCategory = stream.ReadInt32();
    {
        const uint32_t n = stream.ReadUint32();
        mTagNames.clear(); mTagNames.reserve(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            std::string s; stream.ReadString(s);
            mTagNames.emplace_back(std::move(s));
        }
    }

    // Behavior
    mMaxCount   = stream.ReadInt32();
    mAutoUse    = stream.ReadBool();
    mEquipable  = stream.ReadBool();
    mConsumable = stream.ReadBool();
    mThrowable  = stream.ReadBool();

    // Category data
    mAmount = stream.ReadInt32();
    stream.ReadString(mAmmoForWeapon);
    stream.ReadString(mWeaponArchetype);

    // Signals
    stream.ReadString(mOnPickupSignal);
    stream.ReadString(mOnUseSignal);
    stream.ReadString(mOnEquipSignal);
    stream.ReadString(mOnTossSignal);

    // FX
    stream.ReadAsset(mIcon);
    stream.ReadAsset(mWorldMesh);
    stream.ReadAsset(mPickupSound);
    stream.ReadAsset(mPickupVfx);
    stream.ReadAsset(mUseSound);

    // Trailer — older streams may end before this; presence-check to
    // stay forward compatible.
    if (stream.GetPos() + sizeof(uint32_t) <= stream.GetSize())
    {
        const uint32_t magic = stream.ReadUint32();
        if (magic != kItemInfoTrailerMagic)
        {
            LogWarning("ItemInfo '%s': trailer magic mismatch (got 0x%08X)",
                       GetName().c_str(), magic);
        }
    }

    // Re-register so live edits propagate to the registry's lookup
    // table without needing an addon reload.
    ItemInfoRegistry::Get().Add(this);
}

void ItemInfo::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    {
        SCOPED_CATEGORY("Identity");
        outProps.push_back(Property(DatumType::String,  "Item Name",    this, &mItemName));
        outProps.push_back(Property(DatumType::String,  "Display Name", this, &mDisplayName));
        outProps.push_back(Property(DatumType::String,  "Description",  this, &mDescription));
        // Category enum — exposed as Integer; designers reference the
        // Items.Category table on the Lua side for symbolic names.
        // 0 Ammo, 1 Health, 2 Shield, 3 Weapon, 4 KeyItem, 5 Currency, 6 Generic.
        outProps.push_back(Property(DatumType::Integer, "Category",     this, &mCategory));
        outProps.push_back(Property(DatumType::String,  "Tag Names",    this, &mTagNames).MakeVector());
    }
    {
        SCOPED_CATEGORY("Behavior");
        outProps.push_back(Property(DatumType::Integer, "Max Count",   this, &mMaxCount));
        outProps.push_back(Property(DatumType::Bool,    "Auto Use",    this, &mAutoUse));
        outProps.push_back(Property(DatumType::Bool,    "Equipable",   this, &mEquipable));
        outProps.push_back(Property(DatumType::Bool,    "Consumable",  this, &mConsumable));
        outProps.push_back(Property(DatumType::Bool,    "Throwable",   this, &mThrowable));
    }
    {
        SCOPED_CATEGORY("CategoryData");
        // Amount semantics depend on Category — heal HP, ammo rounds,
        // shield points, starter reserve for granted weapons, etc.
        outProps.push_back(Property(DatumType::Integer, "Amount",          this, &mAmount));
        // Ammo only: "" active weapon, "*" all slots, exact name otherwise.
        outProps.push_back(Property(DatumType::String,  "Ammo For Weapon", this, &mAmmoForWeapon));
        // Weapon only: archetype name passed to _EquipFromArchetype.
        outProps.push_back(Property(DatumType::String,  "Weapon Archetype",this, &mWeaponArchetype));
    }
    {
        SCOPED_CATEGORY("Signals");
        // Empty string falls back to Items.Defaults in Lua.
        outProps.push_back(Property(DatumType::String, "On Pickup Signal", this, &mOnPickupSignal));
        outProps.push_back(Property(DatumType::String, "On Use Signal",    this, &mOnUseSignal));
        outProps.push_back(Property(DatumType::String, "On Equip Signal",  this, &mOnEquipSignal));
        outProps.push_back(Property(DatumType::String, "On Toss Signal",   this, &mOnTossSignal));
    }
    {
        SCOPED_CATEGORY("FX");
        outProps.push_back(Property(DatumType::Asset, "Icon",         this, &mIcon,        1, nullptr, int32_t(Texture::GetStaticType())));
        outProps.push_back(Property(DatumType::Asset, "World Mesh",   this, &mWorldMesh,   1, nullptr, int32_t(StaticMesh::GetStaticType())));
        outProps.push_back(Property(DatumType::Asset, "Pickup Sound", this, &mPickupSound, 1, nullptr, int32_t(SoundWave::GetStaticType())));
        outProps.push_back(Property(DatumType::Asset, "Pickup VFX",   this, &mPickupVfx,   1, nullptr, int32_t(ParticleSystem::GetStaticType())));
        outProps.push_back(Property(DatumType::Asset, "Use Sound",    this, &mUseSound,    1, nullptr, int32_t(SoundWave::GetStaticType())));
    }
}

// ===========================================================================
// ItemInfoRegistry
// ===========================================================================

ItemInfoRegistry& ItemInfoRegistry::Get()
{
    static ItemInfoRegistry sInstance;
    return sInstance;
}

static std::string ResolveItemName(const ItemInfo* info)
{
    if (info == nullptr) return {};
    return info->GetItemName().empty() ? info->GetName() : info->GetItemName();
}

void ItemInfoRegistry::Add(ItemInfo* info)
{
    if (info == nullptr) return;
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    if (std::find(mAll.begin(), mAll.end(), info) == mAll.end())
    {
        mAll.push_back(info);
    }
}

void ItemInfoRegistry::Remove(ItemInfo* info)
{
    if (info == nullptr) return;
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    auto it = std::find(mAll.begin(), mAll.end(), info);
    if (it != mAll.end()) mAll.erase(it);
}

ItemInfo* ItemInfoRegistry::Find(const std::string& name) const
{
    if (name.empty()) return nullptr;
    std::lock_guard<std::recursive_mutex> lock(mMutex);

    // Walk the live-pointer list first so editor renames are picked up
    // (ResolveItemName re-reads each asset's current fields).
    for (ItemInfo* info : mAll)
    {
        if (ResolveItemName(info) == name) return info;
    }

    // AssetManager fallback for assets that haven't been Add()'d yet.
    if (AssetManager* mgr = AssetManager::Get())
    {
        if (Asset* a = mgr->GetAsset(name))
        {
            if (a->GetType() == ItemInfo::GetStaticType())
            {
                return static_cast<ItemInfo*>(a);
            }
        }
    }
    return nullptr;
}

std::vector<std::string> ItemInfoRegistry::ListNames() const
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    std::vector<std::string> out;
    out.reserve(mAll.size());
    for (ItemInfo* info : mAll)
    {
        std::string n = ResolveItemName(info);
        if (!n.empty()) out.push_back(std::move(n));
    }
    return out;
}

void ItemInfoRegistry::Clear()
{
    std::lock_guard<std::recursive_mutex> lock(mMutex);
    mAll.clear();
}

int ItemInfoRegistry::LoadAllFromAssetManager()
{
    AssetManager* mgr = AssetManager::Get();
    if (mgr == nullptr) return 0;

    const TypeId wantedType = ItemInfo::GetStaticType();
    int loaded = 0;

    std::vector<std::string> namesToLoad;
    {
        auto& map = mgr->GetAssetMap();
        namesToLoad.reserve(map.size());
        for (auto& kv : map)
        {
            AssetStub* stub = kv.second;
            if (stub == nullptr) continue;
            if (stub->mType != wantedType) continue;
            if (stub->mAsset != nullptr) continue;
            namesToLoad.push_back(kv.first);
        }
    }

    for (const std::string& name : namesToLoad)
    {
        if (Asset* a = mgr->LoadAsset(name))
        {
            (void)a;
            ++loaded;
        }
    }
    return loaded;
}
