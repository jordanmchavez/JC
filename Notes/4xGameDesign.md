Eador: dmg = [atk-20%, atk+20%] - def
	< 50% HP: every 10% = -10% attack
	Every Sta < 5 = -10% attack
	Every morale < 5 = -10% atk
	negative dmg equation means 50%...0% chance of doing 1 dmg [0..-10]

MoM/Heroquest: atk vs def are just d10s with 30% success rates

=========================================================================================================



Combat System

mirror eador for now
hex grid
set of units
each unit has a set of stats
player turn, ai turn
wait for command
execute command
end turn command

eador:
	hp
	stam
	morale
	atk
	ctr atk
	mov
	def
	r.def
	m.def
	r.atk
	ammo
	range
	special abilities like immunities, etc


phalanx here
supported by archers and cavalry
mages casting spells
huge monsters
fliers
artillery

what made eador fun?
	keeping your dudes alive with healers while enemy was coming
	surviving the onslaught
	stamina mechanics / rotating out
unit has facing, can attack any of the three "front" tiles
withdrawing from melee, or any zone of control, needs a cost, like auto AOO

notion of orders and limited control

attack stat
offensive skill vs defensive skill
power vs toughness

to-hit: d6 <= a.skl - d.skl
to-wound: d6 <= a.pow - d.tgh

charging:             +2/+0/+0
pinching:             +0/+1/+1
attacking their side: +0/+1/+0
attacking their rear: +0/+1/+1
attacking your side:  -1/-0/-0
attacking your rear:  -0/-1/-2
in the yellow:        -1/-0/-0
in the red:           -2/-0/-0
long range:           -0/-1/-0
move and shoot:       -0/-1/-0

yellow: -1 morale
red:    -2 morale

rout checks occur:
	green->yellow
	yellow->red
	pinched (
rout check: 3d6 <= morale (incl modifiers)
rout: turn around, all engaged units free action, moves to edge, keeps going until off table
spend cmd to rally

cmd:
	change order
	direct control
	draw cmd card
	use bravery ability
	rally

standing orders:
	close closest enemy
	range nearest enemy
	hold

movement: rotating/sliding uses movement








# Design Doc
I want to make a game
either a caravan/trading/mercenary/mobile base game
or a "roguelike" 4x game where you conquer a series of shard worlds, getting stronger/unlocking more things each time

what is "fun"? fun is exploring new thinbgs, unlocking new mechanics/items/units/enemies/regions, growing more powerful, confronting challenges, making *visible* progress/being *visibly* stronger than you were before (what was once hard is now easy)

what makes vagrus fun? Upgrading is very obstruse and nonobvious. The thrill is "what comes next and am I prepared for it"

build your leader
select your race
select your lords
select your initial army

survival in a hostile world with a mercenary company is very different than a conquer-the-world 4x

eador is fun because the tactical combat is amazing
everything is less interesting: the economics only drive the army and it's just money. even special resources only exist to reduce cost so they're not essential
to me this is less interesting than having actual resources

organic beings require food and possibly money
archers require wood
soldiers require iron
enchanted beings require magic

let's try to model it exactly
lords are basically heroes who can level up, have powers, add special abilities to the units their a part of, can go into dungeons and such
units have 1 or more figure
each figure


Figure {
	hp
	active abilities
	passive abilities
	stats: strength, toughness, mental, stamina, morale, etc
	abilities are influenced by stats
}
Unit {
	Figure[] figures
	Items: weapons, armor
}
items require upkeep
upkeep supplied by items carried with the army
army has a wagon train/beasts of burden/carrying capacity to carry food/material
army has camp followers like smiths, healers, etc that give various benefits: reduced upkeep, produciton of materials, faster healing, morale, etc

cities produce resources: food, gold, metal, wood, exotic resources, mana, research, etc
cities have a transport capacity where their production can be moved to another location via caravans the player must manage

should this even be turn-based? what about real time with pause?

armies can draw supplies from a depot that's close by automatically
army can build depots from its supplies, or pack up a depot and recover some resources
depots can be upgraded into forts that provide additional defenses in a surrounding area
the fort's garrison can assist with any attack in that radius
production would then be continuous, or on a very short tick loop (1/second or something)

cities have control radii based on size
cities can build production buildings either in the city center itself (commerce), or on surrounding tiles (mines, farms, etc)

we want the distances between cities to be large
with realtime, do we still have hexes? How do cities build external things? or is it more virtual, where there's some number of "terrain slots" and you can choose what to build on each?
how do players found cities? we need a limit here, or a tremendous cost. perhaps explicitly limit the player by some sort of "founding stone" that's required to build a new city based on some planar limit
islands of fertility in seas of wasteland. i like this idea, it basically hardcaps the # cities per shard, or requires the player to amass magic
perhaps the fertility spell requires maintenance

cities are fortresses with farms/mines/lumbermills/buroughs around them
how is the world map represented? how does a city determine what resources are available to it to exploit? what exactly is a "resource" if you don't have a discrete map?
There must be some level of discreteness, it's just very small
rectangular grid let's say
each square has a type: forest, water, plans, hills, etc
sum the total of each type to provide the potential for exploitation
then individual buildings, like farms or mills, can be built and expanded to meet that potential
mines and mills are more discrete than farms, however
going to a hex grid simplifies this considerably, however it makes the realtime aspect more problematic
controlling multiple armies in realtime is also problematic, you still have to switch back and forth
the total wara approach of moving all your armies then ending your turn, then production occuring at the end of the turn

the initial colonization is almost like landing on a new planet
you need to bring enough raw resources to bootstrap, metal in particular

how do armies interact with each other without hexes? must be an interaction radius

there are also random encounters on the world map ala mom: ruins, dungeons, caves, etc

