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
	ChestRig        UMETA(DisplayName = "Chest Rig"),
	Backpack        UMETA(DisplayName = "Backpack"),
	Cosmetic        UMETA(DisplayName = "Cosmetic"),
};

// Wearable container slot. Equipping a container of this type extends the
// owning inventory's grid + weight + volume capacity. None = not a container.
UENUM(BlueprintType)
enum class EQRContainerSlotType : uint8
{
	None            UMETA(DisplayName = "None"),
	ChestRig        UMETA(DisplayName = "Chest Rig"),
	Backpack        UMETA(DisplayName = "Backpack"),
};

// Which logical grid an item lives in. Body is the player's base inventory
// (always present); ChestRig and Backpack are only valid when the matching
// container is equipped via UQRInventoryComponent.
UENUM(BlueprintType)
enum class EQRContainerKind : uint8
{
	None            UMETA(DisplayName = "None / Unplaced"),
	Body            UMETA(DisplayName = "Body"),
	ChestRig        UMETA(DisplayName = "Chest Rig"),
	Backpack        UMETA(DisplayName = "Backpack"),
};

// ─────────────────────────────────────────────
//  PLAYER IDENTITY (Batch A — pronoun + dialogue substitution layer)
// ─────────────────────────────────────────────

// Player-selected pronoun set. Drives third-person dialogue substitution
// performed by UQRPronounLibrary. Three options are intentionally all that
// exist — the dropdown is a neutral selector, not a politicized feature.
UENUM(BlueprintType)
enum class EQRPronouns : uint8
{
	He      UMETA(DisplayName = "He / Him"),
	She     UMETA(DisplayName = "She / Her"),
	They    UMETA(DisplayName = "They / Them"),
};

// The player's persistent character identity — name, pronouns, voice line set.
// Stored on the player save state and read by every dialogue line / NPC barks
// system through UQRPronounLibrary::Substitute.
USTRUCT(BlueprintType)
struct QRCORE_API FQRPlayerIdentity
{
	GENERATED_BODY()

	// Display name the player chose at character creation. Used for the
	// {name} dialogue token. Empty string is a valid sentinel meaning
	// "fall back to the default 'Survivor' label".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString DisplayName;

	// Player-selected pronoun set. Defaults to They so dialogue stays
	// gender-neutral until the player makes a choice — the same reason
	// "Customer" is the default form-letter salutation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	EQRPronouns Pronouns = EQRPronouns::They;

	// Voice profile asset the player's character speaks with. Soft-pointer
	// so unselected slots don't pull audio packages. Authored later by the
	// voice-acting pass; this field just reserves the hook.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	TSoftObjectPtr<class USoundBase> VoiceProfile;
};

// ─────────────────────────────────────────────
//  CHARACTER APPEARANCE / BODY CUSTOMIZATION (Batch B)
// ─────────────────────────────────────────────

// Body mesh template the character creator starts from. INDEPENDENT of
// EQRPronouns — a Feminine-bodied character can use He pronouns, a
// Masculine-bodied character can use She or They pronouns, etc. The two
// fields are decoupled on purpose so player choice stays orthogonal.
UENUM(BlueprintType)
enum class EQRBodyType : uint8
{
	Masculine UMETA(DisplayName = "Masculine"),
	Feminine  UMETA(DisplayName = "Feminine"),
};

// Body proportions / silhouette parameters. All normalized sliders (0..1)
// where the morph target authoring sets the 0/1 endpoints to realistic
// extremes — no caricature. Height + weight are explicit physical units.
//
// Realism note: every slider is hard-clamped to 0..1 in code AND the
// morph target endpoints in Blender are authored within plausible adult
// human range. There is no exposed "cartoon mode" — keeping the player
// inside a realistic envelope is enforced at both the data layer (these
// clamps) and the art layer (the actual blend-shape extremes).
USTRUCT(BlueprintType)
struct QRCORE_API FQRBodyCustomization
{
	GENERATED_BODY()

	// Mesh base template. Drives which skeletal mesh asset the character
	// creator instantiates. Pronouns are a separate field (FQRPlayerIdentity).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	EQRBodyType BodyType = EQRBodyType::Masculine;

	// Height in centimeters. 145–205cm covers ~99% of adult human range.
	// Anything outside that is fantasy — clamped at the type level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "145", ClampMax = "205", UIMin = "145", UIMax = "205"))
	float HeightCm = 170.0f;

	// Body weight in kilograms. 40–130kg covers the realistic adult range.
	// Independent of muscularity / body fat sliders so a heavy frame can
	// be either muscular or soft.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "40", ClampMax = "130", UIMin = "40", UIMax = "130"))
	float WeightKg = 70.0f;

	// Muscularity (0 = lean / unconditioned, 1 = highly conditioned but
	// still anatomically realistic — think trained soldier, not bodybuilder).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float Muscularity = 0.4f;

	// Body fat (0 = very lean, 1 = heavy build). Drives soft-tissue morphs
	// across the body — distinct from WeightKg (which scales bone+muscle
	// mass too).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float BodyFat = 0.4f;

	// Shoulder width (0 = narrow, 1 = broad). Drives upper-torso silhouette.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float ShoulderWidth = 0.5f;

	// Waist size (0 = slim, 1 = wide). Combines with HipWidth to drive
	// hip-to-waist ratio.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float WaistSize = 0.5f;

	// Hip width (0 = narrow, 1 = broad).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float HipWidth = 0.5f;

	// Bust / chest fullness. 0 = flat (default for Masculine bodies and
	// some Feminine bodies). 1 = realistic large — the morph target's 1.0
	// endpoint is authored at a clearly-busty but still-anatomically-plausible
	// extent. There is no "monstrous" mode and the data slider can't
	// exceed 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float BustFullness = 0.0f;

	// Glute fullness (0 = flat, 1 = full).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float GluteFullness = 0.5f;
};

// Face shape parameters. All normalized 0..1 with 0.5 as neutral baseline.
// Each one drives a single morph target on the head mesh. Authoring rule:
// the 0 and 1 endpoints are within real-human variance — no exaggerated
// cartoon faces. Variation is achieved by combining many subtle morphs.
USTRUCT(BlueprintType)
struct QRCORE_API FQRFaceCustomization
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float JawWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float JawLength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float ChinProminence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float CheekboneHeight = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float CheekboneWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float BrowRidge = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float EyeSpacing = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float EyeSize = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float NoseLength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float NoseWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float NoseTipShape = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float LipFullness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float MouthWidth = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face",
		meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float ForeheadHeight = 0.5f;
};

// Top-level appearance bundle — body + face + skin/hair colors + style
// indices into authored hair/facial-hair catalogs. Saved alongside
// FQRPlayerIdentity in FQRGameSaveData.
USTRUCT(BlueprintType)
struct QRCORE_API FQRCharacterAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FQRBodyCustomization Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FQRFaceCustomization Face;

	// Skin tone. The character creator UI offers a curated palette of
	// realistic human skin tones; this is the resolved color. Anything
	// outside the ClampAppearanceToRealism check (e.g. green skin from
	// modding) snaps back into the realistic range on load.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor SkinTone = FLinearColor(0.78f, 0.62f, 0.50f, 1.0f);

	// Hair color (RGB — eumelanin / pheomelanin axis covers black through
	// blonde / red; bleached / dyed colors are out-of-range and clamped).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor HairColor = FLinearColor(0.20f, 0.10f, 0.05f, 1.0f);

	// Eye color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor EyeColor = FLinearColor(0.30f, 0.40f, 0.50f, 1.0f);

	// Index into the authored hair-style catalog. 0 = "buzz / short" base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (ClampMin = "0"))
	int32 HairStyleIndex = 0;

	// Index into the authored facial-hair catalog. 0 = clean shaven.
	// Authoring lives on the character creator data asset, not here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (ClampMin = "0"))
	int32 FacialHairIndex = 0;
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

// Tier classification for Vanguard Concordat satellite outposts.
// Determined at runtime from distance to the Concordat; drives AI, attacker count, and equipment.
UENUM(BlueprintType)
enum class EQRVanguardHardpointTier : uint8
{
	ListeningPost   UMETA(DisplayName = "Listening Post"),   // Outer ring — scouts, watchers, no fortification
	ForwardPost     UMETA(DisplayName = "Forward Post"),     // Mid-outer — light infantry, basic cover
	Hardpoint       UMETA(DisplayName = "Hardpoint"),        // Mid-inner — fortified, coordinated patrols
	InnerSanctum    UMETA(DisplayName = "Inner Sanctum"),    // Near Concordat — elite, heavy weapons
	Concordat       UMETA(DisplayName = "Concordat"),        // The main colony — Fanatic-tier, never relocated
};

// Physical artifact left behind by Progenitor automated systems.
// Found near active Remnant sites; each type has distinct research and crafting uses.
UENUM(BlueprintType)
enum class EQRRemnantArtifactType : uint8
{
	DataShard       UMETA(DisplayName = "Data Shard"),      // Crystalline memory sliver — yields Remnant research on study
	PowerCell       UMETA(DisplayName = "Power Cell"),      // Spent or partial energy store — T4 power ingredient
	SignalFragment  UMETA(DisplayName = "Signal Fragment"), // Broken Spire component — contains Progenitor language data
	MemoryCore      UMETA(DisplayName = "Memory Core"),     // Dense data cluster — rare; unlocks codex lore directly
};

// Classification of Progenitor (Remnant) structure types found on Tharsis IV.
UENUM(BlueprintType)
enum class EQRRemnantStructureType : uint8
{
	SignalSpire         UMETA(DisplayName = "Signal Spire"),        // Emits the beacon signal; studying unlocks lore
	PowerCore           UMETA(DisplayName = "Power Core"),          // Still-active energy source; harvestable for T4 power
	DataArchive         UMETA(DisplayName = "Data Archive"),        // Crystalline storage; yields Remnant research on study
	ResonanceChamber    UMETA(DisplayName = "Resonance Chamber"),   // Deepest sites; responds to Rift research progress
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
//  WEATHER EVENT ENUMS
// ─────────────────────────────────────────────

// Active atmospheric hazard. Stored on UQRWeatherComponent and broadcast to all systems.
UENUM(BlueprintType)
enum class EQRWeatherEvent : uint8
{
	None            UMETA(DisplayName = "None"),
	DustStorm       UMETA(DisplayName = "Dust Storm"),       // visibility ↓, machinery fouling ↑
	AcidRain        UMETA(DisplayName = "Acid Rain"),        // ongoing armor/structure damage
	VentEruption    UMETA(DisplayName = "Vent Eruption"),    // heat + toxin zones
	HeatWave        UMETA(DisplayName = "Heat Wave"),        // core temp ↑, dehydration x2
	IceFog          UMETA(DisplayName = "Ice Fog"),          // hypothermia risk, vision 0
	MagneticStorm   UMETA(DisplayName = "Magnetic Storm"),   // electronics disabled
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

// ─────────────────────────────────────────────
//  REUSABLE STAT CONTAINER
// ─────────────────────────────────────────────

// Generic clamped stat (Health, Hunger, Thirst, Fatigue, Morale, etc.).
// Replaces ad-hoc float + MaxFloat pairs when a stat needs portable serialization or blueprints.
USTRUCT(BlueprintType)
struct FQRClampedStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Current = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Max = 100.0f;

	// Returns normalized [0..1] position within the [Min..Max] range.
	float GetNormalized() const
	{
		return (Max > Min) ? FMath::Clamp((Current - Min) / (Max - Min), 0.0f, 1.0f) : 0.0f;
	}

	void Set(float Value)   { Current = FMath::Clamp(Value, Min, Max); }
	void Add(float Delta)   { Set(Current + Delta); }
	bool IsAtMin() const    { return Current <= Min + KINDA_SMALL_NUMBER; }
	bool IsAtMax() const    { return Current >= Max - KINDA_SMALL_NUMBER; }
};
