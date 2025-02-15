# Tactical Battle

# Overview
When one army attacks another, the game transitions to a tactical combat map. The map is a hex grid where each unit occupies one hex on the grid.
Units have facing, and so have a front, back, front right/left and back right/left. We can use these for flanking calculations (ie unit takes more damage if flanked in back than if flanked in back left/right).
Terrain affects combat stats (forests add defense, mountains add ranged dmg, swamp decreases movement, etc)
Units have stamina which affects their combat performance. All combat actions reduce stamina. Zero stamina means the unit must rest and pass and is vulnerable. This makes stamina draining a valid strategy.
Units also have morale and is affected by unit losses, whole-army losses, fear, terror, etc.
Combat is "side-at-a-time", where all units on one side go then all units on another side go.
When a unit attacks another unit in melee, both units become "engaged" where withdrawing gives the enemy an opportunity attack
melee units also project zones of control where an enemy unit must stop if adjacent to front/left/right

# Combat Stats
melee attack:
damage, reduction, health
no chance to hit, but damage range

Attack
Counterattack
Defense (flat and percent)
Magic Defense (flat and percent)
Model Count
HP
Stamina
Morale
Movement
Abilities
Traits

Multiple units in a model, each unit attacks separately
Some attacks "trample over", if they kill one model in a unit then extra damage applies to the next one
Other attacks are "single target"

# Humans
Archers
Swords
Spears
Battlemages
Knights
Paladins
Templars



Battle {
	PlayerArmy
		Units[]
	EnemyArmy
		Units[]
	Tiles[]
}

render a terrain grid and move units around it
