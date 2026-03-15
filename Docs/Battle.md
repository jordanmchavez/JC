"shortest path" = lowest total movement cost with ties broken by fewest hexes

If no unit is selected
	Hover friendly unit
		white 100% border
		show moveable hexes green 50% fill
		show attackable hexes green 50% fill and green 100% border
	Hover enemy unit
		white 100% border
		show moveable hexes red 50% fill
		show attackable hexes red 50% fill and red 100% border
	Hover else
		white 50% border
	LClick friendly
		select that unit
Selected
	show moveable hexes green 50% fill
	show attackable hexes green 50% fill and green 100% border
	Hover friendly unit
		white 100% border
	Hover empty moveable
		hoverMovePath = shortest path yellow 50% 
		set lastEmptyMoveable = hover hex
	Hover enemy attackable
		if lastEmptyMoveable and hover enemy in range of lastEmptyMoveable
			show yellow 50% shortest path ending on lastEmptyMoveable in range of enemy
		else
			show yellow 50% shortest path in range of enemy
	Hover else
		if lastEmptyMoveable
			show path to lastEmptyMoveable yellow 50%
		else
			white 50% border
	LClick empty moveable
		targetMovePath = hoverMovePath
	LClick friendly
		selected = unit
	LClick enemy attackable
		target enemy for attack
	RClick
		selected = nullptr
moveTarget
attackTarget
Move or Attack Target
	show path yellow 100%

Alt enemy threat map
