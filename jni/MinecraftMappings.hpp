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
        inline constexpr const char *ClientLevel = "hif";
        inline constexpr const char *DeltaTracker = "gez";
        inline constexpr const char *DeltaTrackerTimer = "gez$b";
        inline constexpr const char *DeltaTrackerDefaultValue = "gez$a";
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
        inline constexpr const char *JavaList = "java/util/List";
        inline constexpr const char *JavaNumber = "java/lang/Number";
        inline constexpr const char *JavaDouble = "java/lang/Double";
    } // namespace classes

    namespace minecraft {
        inline constexpr MemberId Instance{"A", "Lgfj;"};
        inline constexpr MemberId Level{"r", "Lhif;"};
        inline constexpr MemberId DeltaTracker{"P", "Lgez$b;"};
        inline constexpr MemberId GetDeltaTracker{"aD", "()Lgez;"};
        inline constexpr MemberId Options{"k", "Lgfo;"};
        inline constexpr MemberId LocalPlayer{"s", "Lhnh;"};
        inline constexpr MemberId HitResult{"u", "Lftk;"};
        inline constexpr MemberId GameMode{"q", "Lhio;"};
        inline constexpr MemberId RightClickDelay{"aR", "I"};
        inline constexpr MemberId StartAttack{"bu", "()Z"};
    } // namespace minecraft

    namespace client_level {
        inline constexpr MemberId Players{"E", "()Ljava/util/List;"};
    } // namespace client_level

    namespace options {
        inline constexpr MemberId Gamma{"di", "Lgfn;"};
        inline constexpr MemberId Fov{"cT", "Lgfn;"};
    } // namespace options

    namespace option_instance {
        inline constexpr MemberId Value{"k", "Ljava/lang/Object;"};
    } // namespace option_instance

    namespace local_player {
        inline constexpr MemberId SetSprinting{"i", "(Z)V"};
        inline constexpr MemberId AttackStrengthScale{"I", "(F)F"};
        inline constexpr MemberId SetDeltaMovement{"m", "(DDD)V"};
        inline constexpr MemberId HurtTime{"bu", "I"};
    } // namespace local_player

    namespace game_mode {
        inline constexpr MemberId DestroyDelay{"h", "I"};
        inline constexpr MemberId DestroyProgress{"f", "F"};
    } // namespace game_mode

    namespace entity_hit_result {
        inline constexpr MemberId GetEntity{"a", "()Lcgk;"};
    } // namespace entity_hit_result

    namespace entity {
        inline constexpr MemberId GetName{"ap", "()Lyh;"};
        inline constexpr MemberId IsAlive{"cb", "()Z"};
        inline constexpr MemberId GetX{"dP", "()D"};        
        inline constexpr MemberId GetY{"dR", "()D"};
        inline constexpr MemberId GetEyeY{"dT", "()D"};
        inline constexpr MemberId GetZ{"dV", "()D"};
        inline constexpr MemberId GetYaw{"ec", "()F"};
        inline constexpr MemberId GetPitch{"ee", "()F"};
        inline constexpr MemberId OldX{"ao", "D"};
        inline constexpr MemberId OldY{"ap", "D"};
        inline constexpr MemberId OldZ{"aq", "D"};
    } // namespace entity

    namespace delta_tracker {
        inline constexpr MemberId GetGameTimeDeltaPartialTick{"a", "(Z)F"};
    } // namespace delta_tracker

    namespace delta_tracker_timer {
        inline constexpr MemberId GetGameTimeDeltaPartialTick{"a", "(Z)F"};
    } // namespace delta_tracker_timer

    namespace delta_tracker_default_value {
        inline constexpr MemberId GetGameTimeDeltaPartialTick{"a", "(Z)F"};
    } // namespace delta_tracker_default_value

    namespace component {
        inline constexpr MemberId GetString{"getString", "()Ljava/lang/String;"};
    } // namespace component

    namespace java_list {
        inline constexpr MemberId Size{"size", "()I"};
        inline constexpr MemberId Get{"get", "(I)Ljava/lang/Object;"};
    } // namespace java_list

    namespace java_number {
        inline constexpr MemberId DoubleValue{"doubleValue", "()D"};
    } // namespace java_number

    namespace java_double {
        inline constexpr MemberId Constructor{"<init>", "(D)V"};
    } // namespace java_double
} // namespace mc_mappings
