#pragma once

// Centralized Minecraft JNI symbol table.
// For game updates, update the obfuscated names here instead of searching the whole project.
namespace mc_mappings {

struct MemberId {
    const char *name;
    const char *signature;
};

namespace classes {
inline constexpr const char *Minecraft = "gfj";
inline constexpr const char *Options = "gfo";
inline constexpr const char *OptionInstance = "gfn";
inline constexpr const char *Entity = "cgk";
inline constexpr const char *LocalPlayer = "hnh";
inline constexpr const char *Player = "ddm";
inline constexpr const char *GameMode = "hio";
inline constexpr const char *HitResult = "ftk";
inline constexpr const char *BlockHitResult = "fti";
inline constexpr const char *EntityHitResult = "ftj";
inline constexpr const char *Component = "yh";
inline constexpr const char *JavaDouble = "java/lang/Double";
}  // namespace classes

namespace minecraft {
inline constexpr MemberId Instance{"A", "Lgfj;"};
inline constexpr MemberId Options{"k", "Lgfo;"};
inline constexpr MemberId LocalPlayer{"s", "Lhnh;"};
inline constexpr MemberId HitResult{"u", "Lftk;"};
inline constexpr MemberId GameMode{"q", "Lhio;"};
inline constexpr MemberId RightClickDelay{"aR", "I"};
}  // namespace minecraft

namespace options {
inline constexpr MemberId Gamma{"di", "Lgfn;"};
}  // namespace options

namespace option_instance {
inline constexpr MemberId Value{"k", "Ljava/lang/Object;"};
}  // namespace option_instance

namespace local_player {
inline constexpr MemberId SetSprinting{"i", "(Z)V"};
inline constexpr MemberId AttackStrengthScale{"I", "(F)F"};
inline constexpr MemberId SetDeltaMovement{"m", "(DDD)V"};
inline constexpr MemberId HurtTime{"bu", "I"};
}  // namespace local_player

namespace game_mode {
inline constexpr MemberId DestroyDelay{"h", "I"};
inline constexpr MemberId DestroyProgress{"f", "F"};
}  // namespace game_mode

namespace entity_hit_result {
inline constexpr MemberId GetEntity{"a", "()Lcgk;"};
}  // namespace entity_hit_result

namespace entity {
inline constexpr MemberId GetName{"ap", "()Lyh;"};
inline constexpr MemberId IsAlive{"cb", "()Z"};
}  // namespace entity

namespace component {
inline constexpr MemberId GetString{"getString", "()Ljava/lang/String;"};
}  // namespace component

namespace java_double {
inline constexpr MemberId Constructor{"<init>", "(D)V"};
}  // namespace java_double

}  // namespace mc_mappings
