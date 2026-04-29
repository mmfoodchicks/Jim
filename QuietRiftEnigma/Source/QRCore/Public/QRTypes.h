#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QRTypes.generated.h"

// ─────────────────────────────────────────────
//  ITEM / CATEGORY ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRItemCategory : uint8
{
	None            UMETA(DisplayName = "None"),
	Food            UMETA(DisplayName = "Food"),
	Tool            UMETA(DisplayName = "Tool"),
	Weapon          UMETA(DisplayName = "Weapon"),
	Ammo            UMETA(DisplayName = "Ammo"),
	Attachment      UMETA(DisplayName = "Attachment"),
	Component       UMETA(DisplayName = "Component"),
	ReferenceComp   UMETA(DisplayName = "Reference Component"),
	Resource        UMETA(DisplayName = "Resource"),
	Fuel            UMETA(DisplayName = "Fuel"),
	Medicine        UMETA(DisplayName = "Medicine"),
	Clothing        UMETA(DisplayName = "Clothing"),
	Blueprint_Item  UMETA(DisplayName = "Blueprint"),
	Seed            UMETA(DisplayName = "Seed"),
	Flora           UMETA(DisplayName = "Flora"),
	Wildlife        UMETA(DisplayName = "Wildlife Product"),
};

UENUM(BlueprintType)
enum class EQREdibilityState : uint8
{
	Unknown         UMETA(DisplayName = "Unknown"),
	Safe            UMETA(DisplayName = "Safe"),
	RiskyRaw        UMETA(DisplayName = "Risky Raw"),
	Toxic           UMETA(DisplayName = "Toxic"),
	Inedible        UMETA(DisplayName = "Inedible"),
	MustCook        UMETA(DisplayName = "Must Cook"),
	Researched      UMETA(DisplayName = "Researched"),
};

UENUM(BlueprintType)
enum class EQRSpoilState : uint8
{
	Fresh           UMETA(DisplayName = "Fresh"),
	Aging           UMETA(DisplayName = "Aging"),
	Spoiled         UMETA(DisplayName = "Spoiled"),
	Rotten          UMETA(DisplayName = "Rotten"),
	Preserved       UMETA(DisplayName = "Preserved"),
};

// ─────────────────────────────────────────────
//  SURVIVAL ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRInjuryType : uint8
{
	None            UMETA(DisplayName = "None"),
	Bleeding        UMETA(DisplayName = "Bleeding"),
	Fracture        UMETA(DisplayName = "Fracture"),
	Infection       UMETA(DisplayName = "Infection"),
	Concussion      UMETA(DisplayName = "Concussion"),
	Burn            UMETA(DisplayName = "Burn"),
	Toxin           UMETA(DisplayName = "Toxin"),
	Exhaustion      UMETA(DisplayName = "Exhaustion"),
	Hypothermia     UMETA(DisplayName = "Hypothermia"),
};

UENUM(BlueprintType)
enum class EQRInjurySeverity : uint8
{
	Minor           UMETA(DisplayName = "Minor"),
	Moderate        UMETA(DisplayName = "Moderate"),
	Severe          UMETA(DisplayName = "Severe"),
	Critical        UMETA(DisplayName = "Critical"),
};

// ─────────────────────────────────────────────
//  RESEARCH / TECH ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRResearchFamily : uint8
{
	Food            UMETA(DisplayName = "Food"),
	Water           UMETA(DisplayName = "Water"),
	Medicine        UMETA(DisplayName = "Medicine"),
	Materials       UMETA(DisplayName = "Materials"),
	Metalwork       UMETA(DisplayName = "Metalwork"),
	Electronics     UMETA(DisplayName = "Electronics"),
	Agriculture     UMETA(DisplayName = "Agriculture"),
	Ecology         UMETA(DisplayName = "Ecology"),
	Military        UMETA(DisplayName = "Military"),
	Remnant         UMETA(DisplayName = "Remnant"),
};

UENUM(BlueprintType)
enum class EQRTechTier : uint8
{
	T0_Primitive    UMETA(DisplayName = "T0 Primitive"),
	T1_Basic        UMETA(DisplayName = "T1 Basic"),
	T2_Intermediate UMETA(DisplayName = "T2 Intermediate"),
	T3_Advanced     UMETA(DisplayName = "T3 Advanced"),
	T4_Remnant      UMETA(DisplayName = "T4 Remnant"),
};

UENUM(BlueprintType)
enum class EQRCodexDiscoveryState : uint8
{
	Undiscovered    UMETA(DisplayName = "Undiscovered"),
	Observed        UMETA(DisplayName = "Observed"),
	Sampled         UMETA(DisplayName = "Sampled"),
	Pending         UMETA(DisplayName = "Pending Research"),
	Known           UMETA(DisplayName = "Known"),
	Mastered        UMETA(DisplayName = "Mastered"),
};

// ─────────────────────────────────────────────
//  NPC / COLONY ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRNPCRole : uint8
{
	Unassigned      UMETA(DisplayName = "Unassigned"),
	Gatherer        UMETA(DisplayName = "Gatherer"),
	Hauler          UMETA(DisplayName = "Hauler"),
	Cook            UMETA(DisplayName = "Cook"),
	Builder         UMETA(DisplayName = "Builder"),
	Miner           UMETA(DisplayName = "Miner"),
	Guard           UMETA(DisplayName = "Guard"),
	Researcher      UMETA(DisplayName = "Researcher"),
	Medic           UMETA(DisplayName = "Medic"),
	Scout           UMETA(DisplayName = "Scout"),
	Farmer          UMETA(DisplayName = "Farmer"),
	Engineer        UMETA(DisplayName = "Engineer"),
	Leader          UMETA(DisplayName = "Leader"),
};

UENUM(BlueprintType)
enum class EQRLeaderType : uint8
{
	None            UMETA(DisplayName = "None"),
	Security        UMETA(DisplayName = "Security"),
	Engineering     UMETA(DisplayName = "Engineering"),
	Medical         UMETA(DisplayName = "Medical"),
	Logistics       UMETA(DisplayName = "Logistics"),
	Research        UMETA(DisplayName = "Research"),
	Agriculture     UMETA(DisplayName = "Agriculture"),
	Diplomacy       UMETA(DisplayName = "Diplomacy"),
	Military        UMETA(DisplayName = "Military"),
	Morale          UMETA(DisplayName = "Morale"),
	Survival        UMETA(DisplayName = "Survival"),
};

UENUM(BlueprintType)
enum class EQRNPCMoodState : uint8
{
	Stable          UMETA(DisplayName = "Stable"),
	Anxious         UMETA(DisplayName = "Anxious"),
	Panicked        UMETA(DisplayName = "Panicked"),
	Exhausted       UMETA(DisplayName = "Exhausted"),
	Motivated       UMETA(DisplayName = "Motivated"),
	Defiant         UMETA(DisplayName = "Defiant"),
	Resigned        UMETA(DisplayName = "Resigned"),
};

UENUM(BlueprintType)
enum class EQRCivilianRaidState : uint8
{
	Normal          UMETA(DisplayName = "Normal"),
	Alert           UMETA(DisplayName = "Alert"),
	Defending       UMETA(DisplayName = "Defending"),
	Hiding          UMETA(DisplayName = "Hiding"),
	Fleeing         UMETA(DisplayName = "Fleeing"),
	Evacuated       UMETA(DisplayName = "Evacuated"),
};

// ─────────────────────────────────────────────
//  FACTION / RAID ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRFactionStance : uint8
{
	Unknown         UMETA(DisplayName = "Unknown"),
	Neutral         UMETA(DisplayName = "Neutral"),
	Friendly        UMETA(DisplayName = "Friendly"),
	Allied          UMETA(DisplayName = "Allied"),
	Hostile         UMETA(DisplayName = "Hostile"),
	AtWar           UMETA(DisplayName = "At War"),
};

UENUM(BlueprintType)
enum class EQRRaidExperienceTier : uint8
{
	Inexperienced   UMETA(DisplayName = "Inexperienced"),
	Competent       UMETA(DisplayName = "Competent"),
	Veteran         UMETA(DisplayName = "Veteran"),
	Fanatic         UMETA(DisplayName = "Fanatic"),
};

UENUM(BlueprintType)
enum class EQRRemnantWakeState : uint8
{
	Dormant         UMETA(DisplayName = "Dormant"),
	Stirring        UMETA(DisplayName = "Stirring"),
	Active          UMETA(DisplayName = "Active"),
	Hostile         UMETA(DisplayName = "Hostile"),
	Subsiding       UMETA(DisplayName = "Subsiding"),
};

// ─────────────────────────────────────────────
//  WORLD / BIOME ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRBiomeType : uint8
{
	CrashField      UMETA(DisplayName = "Crash Field"),
	Forest          UMETA(DisplayName = "Forest"),
	ForestEdge      UMETA(DisplayName = "Forest Edge"),
	Grassland       UMETA(DisplayName = "Grassland"),
	BurnZone        UMETA(DisplayName = "Burn Zone"),
	Scrubland       UMETA(DisplayName = "Scrubland"),
	Swamp           UMETA(DisplayName = "Swamp"),
	RiverFlat       UMETA(DisplayName = "River Flat"),
	RockyHighland   UMETA(DisplayName = "Rocky Highland"),
	Cave            UMETA(DisplayName = "Cave"),
	AbandonedSite   UMETA(DisplayName = "Abandoned Site"),
	RemnantSite     UMETA(DisplayName = "Remnant Site"),
	MineInterior    UMETA(DisplayName = "Mine Interior"),
	OpenField       UMETA(DisplayName = "Open Field"),
	Ravine          UMETA(DisplayName = "Ravine"),
};

UENUM(BlueprintType)
enum class EQRPOIType : uint8
{
	CrashWreck      UMETA(DisplayName = "Crash Wreck"),
	LuggageWreck    UMETA(DisplayName = "Luggage Wreck"),
	EngineeringWreck UMETA(DisplayName = "Engineering Wreck"),
	FoodCache       UMETA(DisplayName = "Food Cache"),
	ArmoryCache     UMETA(DisplayName = "Armory Cache"),
	ResearchCache   UMETA(DisplayName = "Research Cache"),
	RemnantSite     UMETA(DisplayName = "Remnant Site"),
	AbandonedFarm   UMETA(DisplayName = "Abandoned Farm"),
	AbandonedTown   UMETA(DisplayName = "Abandoned Town"),
	MineEntrance    UMETA(DisplayName = "Mine Entrance"),
	FactionCamp     UMETA(DisplayName = "Faction Camp"),
	RadioTower      UMETA(DisplayName = "Radio Tower"),
};

UENUM(BlueprintType)
enum class EQRControlState : uint8
{
	Uncontrolled    UMETA(DisplayName = "Uncontrolled"),
	PlayerControlled UMETA(DisplayName = "Player Controlled"),
	FactionControlled UMETA(DisplayName = "Faction Controlled"),
	Contested       UMETA(DisplayName = "Contested"),
	RemnantActive   UMETA(DisplayName = "Remnant Active"),
};

// ─────────────────────────────────────────────
//  POWER SYSTEM ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRPowerQuality : uint8
{
	None            UMETA(DisplayName = "None"),
	Unstable        UMETA(DisplayName = "Unstable"),
	Low             UMETA(DisplayName = "Low"),
	Nominal         UMETA(DisplayName = "Nominal"),
	High            UMETA(DisplayName = "High"),
};

// ─────────────────────────────────────────────
//  WILDLIFE BEHAVIOR ENUMS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQRWildlifeAIState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	Grazing         UMETA(DisplayName = "Grazing"),
	Wandering       UMETA(DisplayName = "Wandering"),
	Alert           UMETA(DisplayName = "Alert"),
	Fleeing         UMETA(DisplayName = "Fleeing"),
	Stalking        UMETA(DisplayName = "Stalking"),
	Charging        UMETA(DisplayName = "Charging"),
	Attacking       UMETA(DisplayName = "Attacking"),
	Scavenging      UMETA(DisplayName = "Scavenging"),
	Fleeing_Injured UMETA(DisplayName = "Fleeing Injured"),
	Dead            UMETA(DisplayName = "Dead"),
};

UENUM(BlueprintType)
enum class EQRWildlifeBehaviorRole : uint8
{
	Prey            UMETA(DisplayName = "Prey"),
	Predator        UMETA(DisplayName = "Predator"),
	Scavenger       UMETA(DisplayName = "Scavenger"),
	Ambient         UMETA(DisplayName = "Ambient"),
	Hazard          UMETA(DisplayName = "Hazard"),
};

// ─────────────────────────────────────────────
//  ENDING PATHS
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class EQREndingPath : uint8
{
	None            UMETA(DisplayName = "None"),
	RescueWindow    UMETA(DisplayName = "Rescue Window"),
	EscapeHardware  UMETA(DisplayName = "Escape Hardware"),
	PermanentFoothold UMETA(DisplayName = "Permanent Foothold"),
};

// ─────────────────────────────────────────────
//  v1.17 INTEGRATED MERGE PATCH ENUMS
// ─────────────────────────────────────────────

// Hands slot occupancy — separates bulk-hand carry from grid inventory.
// Source: Quiet_Rift_GDD_Integrated_Merge_Patch_v1_17.docx
UENUM(BlueprintType)
enum class EQRHandsSlotState : uint8
{
	Empty           UMETA(DisplayName = "Empty"),
	Occupied        UMETA(DisplayName = "Occupied"),
	LockedByAction  UMETA(DisplayName = "Locked By Action"),
};

// Origin class for any food item. Separates crash-safe Earth food from native
// alien food that requires discovery before being marked safe.
// Source: Quiet_Rift_GDD_Integrated_Merge_Patch_v1_17.docx
UENUM(BlueprintType)
enum class EQRFoodOriginClass : uint8
{
	Unknown         UMETA(DisplayName = "Unknown"),
	Native          UMETA(DisplayName = "Native (Alien)"),
	EarthCrop       UMETA(DisplayName = "Earth Crop"),
	EarthSealed     UMETA(DisplayName = "Earth Sealed Ration"),
	ShipRation      UMETA(DisplayName = "Ship Ration"),
	ProcessedLocal  UMETA(DisplayName = "Processed Local"),
};

// Tracks the imported crop safety-to-mutation pipeline.
// Source: Quiet_Rift_GDD_Integrated_Merge_Patch_v1_17.docx
UENUM(BlueprintType)
enum class EQREarthCropBaseState : uint8
{
	Stable          UMETA(DisplayName = "Stable"),
	Contaminated    UMETA(DisplayName = "Contaminated"),
	Mutated         UMETA(DisplayName = "Mutated"),
};

// Leader issue/blocker tracking state machine.
// Source: Mission & Leadership Bible v1.4 / Leader_Parameters
UENUM(BlueprintType)
enum class EQRLeaderIssueState : uint8
{
	None            UMETA(DisplayName = "None"),
	Reported        UMETA(DisplayName = "Reported"),
	Escalating      UMETA(DisplayName = "Escalating"),
	QuestIssued     UMETA(DisplayName = "Quest Issued"),
	Resolved        UMETA(DisplayName = "Resolved"),
};

// Generic mission/quest state used for main, side, leader, and faction missions.
UENUM(BlueprintType)
enum class EQRMissionStatus : uint8
{
	Locked          UMETA(DisplayName = "Locked"),
	Available       UMETA(DisplayName = "Available"),
	Active          UMETA(DisplayName = "Active"),
	Completed       UMETA(DisplayName = "Completed"),
	Failed          UMETA(DisplayName = "Failed"),
	Expired         UMETA(DisplayName = "Expired"),
};

// Source family for a mission row in DataTables.
UENUM(BlueprintType)
enum class EQRMissionSource : uint8
{
	MainQuestline       UMETA(DisplayName = "Main Questline"),
	LeaderDirective     UMETA(DisplayName = "Leader Directive"),
	SideQuest           UMETA(DisplayName = "Side Quest"),
	FactionContract     UMETA(DisplayName = "Faction Contract"),
	Procedural          UMETA(DisplayName = "Procedural"),
};

// Ideological pull/push axis for the leader moral compass vector.
// Source: Leader_Moral_Compass v1.4
UENUM(BlueprintType)
enum class EQRMoralCompassAxis : uint8
{
	Compassion          UMETA(DisplayName = "Compassion"),
	Security            UMETA(DisplayName = "Security"),
	Science             UMETA(DisplayName = "Science"),
	Autonomy            UMETA(DisplayName = "Autonomy"),
	Infrastructure      UMETA(DisplayName = "Infrastructure"),
	Isolation           UMETA(DisplayName = "Isolation"),
	Extraction          UMETA(DisplayName = "Extraction"),
	RemnantCuriosity    UMETA(DisplayName = "Remnant Curiosity"),
};
