/*
	spearmenUnitDef = &unitDefs[unitDefsLen++];
	TryTo(Draw::GetSprite("Unit_Spearmen"), spearmenUnitDef->sprite);

	Draw::Sprite attackSprite; TryTo(Draw::GetSprite("Unit_Spearmen_Attack"), attackSprite);
	spearmenUnitDef->maxHp = 10;
	spearmenUnitDef->size = Draw::GetSpriteSize(spearmenUnitDef->sprite);
	U32 frameLen = 0;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = attackSprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameSprites[frameLen] = spearmenUnitDef->sprite;
	spearmenUnitDef->attackAnimationDef.frameDurSecs[frameLen] = 0.2f;
	frameLen++;
	spearmenUnitDef->attackAnimationDef.frameLen = frameLen;
*/