draw an empty map
draw trees on the map
draw a worker and have him move randomly

map, tiles, trees on tiles
worker
designate trees for cutting
worker chops trees and leaves the wood where it stands

designate stockpile, worker returns wood to stockpile



designate resources for collection
stockpile
crafting station

Structure {
	
}

Tree {
	wood resource
	hitpoints
}

MoveTask {
	Pawn pawn
	Vec2 target
}

FellTreeTask {
	
}
Worker {
	position
}

for each move task
	
	

Tile {
	TerrainType
	Structure	// house, tree, bush, etc
}

Map {
	Tile[]
}