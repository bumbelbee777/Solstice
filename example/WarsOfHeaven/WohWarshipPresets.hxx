#pragma once

#include "WohShipTypes.hxx"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Solstice::WarsOfHeaven {

struct WarshipClassDef {
    WarshipClass Class{WarshipClass::Corvette};
    const char* DisplayName{""};
    const char* Doctrine{""};

    float HullPoints{1.0f};
    float LinearAccelScale{1.0f};
    float TurnRateScale{1.0f};
    float SignatureScale{1.0f};

    WarshipSignatureAbility Signature{WarshipSignatureAbility::Afterburn};
    float SignatureAbilityBaseCooldownSec{45.0f};

    std::uint8_t HardpointCount{2};

    const char* PlanNote{""};
};

[[nodiscard]] inline const WarshipClassDef& GetWarshipClassDef(WarshipClass c) {
    static constexpr std::array<WarshipClassDef, static_cast<std::size_t>(WarshipClass::Count)> kTable = {{
        {WarshipClass::Corvette, "Corvette", "Rapid skirmish and commerce escort: high thrust, low mass budget.",
         0.32f, 1.18f, 1.20f, 0.75f, WarshipSignatureAbility::Afterburn, 32.0f, 2,
         "2× mixed S (coil/PD) + 1 S missile; strike-and-run, avoid sustained beam."},
        {WarshipClass::Frigate, "Frigate", "Screen and escort: PDS-saturated, screens strike craft, torp intercept.",
         0.55f, 0.88f, 0.86f, 0.90f, WarshipSignatureAbility::CIWSFlood, 40.0f, 4,
         "2× M + 2× S + light VLS: defensive envelope over DPS; anchor picket line."},
        {WarshipClass::Destroyer, "Destroyer", "Strike and ASuW: heavy coil / rail, alpha missile windows.",
         0.78f, 0.70f, 0.64f, 1.0f, WarshipSignatureAbility::TorpedoSalvo, 55.0f, 5,
         "1× L + 2× M + VLS: timed alpha with thermal discipline; not a brawler."},
        {WarshipClass::Cruiser, "Cruiser", "Fleet action: long sustain, magazine depth, C2, area denial options.",
         1.0f, 0.55f, 0.45f, 1.2f, WarshipSignatureAbility::SpinalLance, 70.0f, 6,
         "2× L + 4–6 M: beam hold, missile tempo; trades agility for standoff power."},
        {WarshipClass::Carrier, "Carrier", "Aerospace and strike package: keep distance; fighters do contact.",
         0.95f, 0.40f, 0.32f, 1.4f, WarshipSignatureAbility::AirWing, 50.0f, 4,
         "4× S PD + hangar / launch (embarked craft in sim): flak when pressed, wings at range."},
    }};
    const auto i = static_cast<std::size_t>(c);
    if (i >= kTable.size()) {
        return kTable[0];
    }
    return kTable[i];
}

[[nodiscard]] inline WarshipWeaponLoadout DefaultLoadoutForClass(WarshipClass c) {
    WarshipWeaponLoadout L{};
    const WarshipClassDef& def = GetWarshipClassDef(c);
    L.HardpointCount = def.HardpointCount;
    for (std::size_t n = 0; n < kWarshipMaxHardpoints; ++n) {
        L.Kinds[n] = WarshipWeaponKind::Coilgun;
    }
    switch (c) {
    case WarshipClass::Corvette:
        L.Kinds[0] = WarshipWeaponKind::Coilgun;
        L.Kinds[1] = WarshipWeaponKind::Missile;
        break;
    case WarshipClass::Frigate:
        L.Kinds[0] = WarshipWeaponKind::PointDefense;
        L.Kinds[1] = WarshipWeaponKind::PointDefense;
        L.Kinds[2] = WarshipWeaponKind::Coilgun;
        L.Kinds[3] = WarshipWeaponKind::Missile;
        break;
    case WarshipClass::Destroyer:
        L.Kinds[0] = WarshipWeaponKind::Railgun;
        L.Kinds[1] = WarshipWeaponKind::Coilgun;
        L.Kinds[2] = WarshipWeaponKind::Coilgun;
        L.Kinds[3] = WarshipWeaponKind::VLS;
        L.Kinds[4] = WarshipWeaponKind::Missile;
        break;
    case WarshipClass::Cruiser:
        L.Kinds[0] = WarshipWeaponKind::Railgun;
        L.Kinds[1] = WarshipWeaponKind::Railgun;
        for (std::size_t j = 2; j < 6; ++j) {
            L.Kinds[j] = WarshipWeaponKind::Coilgun;
        }
        break;
    case WarshipClass::Carrier:
        L.Kinds[0] = WarshipWeaponKind::PointDefense;
        L.Kinds[1] = WarshipWeaponKind::PointDefense;
        L.Kinds[2] = WarshipWeaponKind::PointDefense;
        L.Kinds[3] = WarshipWeaponKind::PointDefense;
        break;
    case WarshipClass::Count:
    default:
        break;
    }
    L.Primary = L.Kinds[0];
    L.Secondary = (L.HardpointCount > 1) ? L.Kinds[1] : L.Kinds[0];
    return L;
}

[[nodiscard]] inline WarshipHullState DefaultHullStateForClass(WarshipClass c) {
    const WarshipClassDef& d = GetWarshipClassDef(c);
    WarshipHullState h{};
    h.Class = c;
    h.LastSignature = d.Signature;
    h.SignatureCooldownRemaining = 0.0f;
    return h;
}

[[nodiscard]] inline ShipHullHealth DefaultHullHealthForClass(WarshipClass c) {
    const WarshipClassDef& d = GetWarshipClassDef(c);
    const float m = 100.0f * d.HullPoints; // game-side scale: Corvette ≈ 32, Cruiser ≈ 100
    ShipHullHealth h{};
    h.Max = m;
    h.Current = m;
    h.ArzachelFragmentation01 = 0.0f;
    return h;
}

} // namespace Solstice::WarsOfHeaven
