#include "BuildingPlacer.h"
#include "InformationManager.h"
#include "Debug.h"
#include "ResourceManager.h"

using namespace BWAPI;
using namespace BWTA;

bool BuildingPlacer::supplyProviderIsBeingBuild = false; 

bool BuildingPlacer::xpandIsBeingBuild = false;


void BuildingPlacer::onStart(){

}

void BuildingPlacer::onFrame(){
	static int lastChecked = 0;

	if (InformationManager::firstCenter->isIdle() && ResourceManager::wrkUnits.size() < 15 &&
		!InformationManager::firstCenter->train(InformationManager::firstCenter->getType().getRace().getWorker())){
		// If that fails, draw the error at the location so that you can visibly see what went wrong!
		// However, drawing the error once will only appear for a single frame
		// so create an event that keeps it on the screen for some frames
		Position pos = InformationManager::firstCenter->getPosition();
		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
			nullptr,    // condition
			Broodwar->getLatencyFrames());  // frames to run
	}

	if (Broodwar->self()->minerals() >= 500 && !xpandIsBeingBuild){
		for (ResourceManager::workerUnit &myUnit : ResourceManager::wrkUnits) {
			if (myUnit.status == "Idle" || myUnit.status == "Returning Cargo") {
				//get a nice place to build a supply depot
				UnitType townHall = Broodwar->self()->getRace().getCenter();
				TilePosition buildTile =
					naturalExpantion();
				//and, if found, send the worker to build it (and leave others alone - break;)
				if (buildTile != TilePositions::None) {
					// Register an event that draws the target build location
					Broodwar->registerEvent([buildTile, townHall](Game*)
					{
						Broodwar->drawBoxMap(Position(buildTile),
							Position(buildTile + townHall.tileSize()),
							Colors::Green);
					},
						nullptr,  // condition
						townHall.buildTime() + 100);  // frames to run
					myUnit.unit->build(townHall, buildTile);
					xpandIsBeingBuild = true;
					myUnit.status = "Building";
					Broodwar << "worker is building expantion" << std::endl;
					break;
				}
				else{
					Broodwar << "Expantion BuildTile = none" << std::endl;
					break;
				}
			}
		}
	}

	//if we're running out of supply and have enough minerals ...
	if (Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() < 6 && Broodwar->self()->minerals() >= 100 &&
		Broodwar->self()->incompleteUnitCount(Broodwar->self()->getRace().getSupplyProvider()) == 0) {
		//iterate over units to find a worker
		for (ResourceManager::workerUnit &myUnit : ResourceManager::wrkUnits) {
			if (myUnit.status == "Idle" || myUnit.status == "Returning Cargo") {
				//get a nice place to build a supply depot
				UnitType supplyDepot = Broodwar->self()->getRace().getSupplyProvider();
				TilePosition buildTile =
					getBuildTile(myUnit.unit, supplyDepot, InformationManager::firstCenter->getTilePosition());
				//and, if found, send the worker to build it (and leave others alone - break;)
				if (buildTile != TilePositions::None) {
					// Register an event that draws the target build location
					Broodwar->registerEvent([buildTile, supplyDepot](Game*)
					{
						Broodwar->drawBoxMap(Position(buildTile),
							Position(buildTile + supplyDepot.tileSize()),
							Colors::Blue);
					},
						nullptr,  // condition
						supplyDepot.buildTime() + 100);  // frames to run

					myUnit.unit->build(supplyDepot, buildTile);
					myUnit.status = "Building";
					Broodwar << "worker is building supplyprovider" << std::endl;
					supplyProviderIsBeingBuild = true;
					break;
				}
			}
		}
	}
}


// Returns a suitable TilePosition to build a given building type near
// specified TilePosition aroundTile, or null if not found. (builder parameter is our worker)
TilePosition BuildingPlacer::getBuildTile(Unit builder, UnitType buildingType, TilePosition aroundTile) {
	TilePosition ret = TilePositions::None;
	int maxDist = 3;
	int stopDist = 40;
	
	// Refinery, Assimilator, Extractor
	if (buildingType.isRefinery()) {
		for (Unit n : Broodwar->neutral()->getUnits()) {
			if ((n->getType() == UnitTypes::Resource_Vespene_Geyser) &&
				(std::abs(n->getTilePosition().x - aroundTile.x) < stopDist) &&
				(std::abs(n->getTilePosition().y - aroundTile.y) < stopDist)
				) return n->getTilePosition();
		}
	}
	if (buildingType.isResourceDepot()){
		return naturalExpantion();
	}


	while ((maxDist < stopDist) && (ret == TilePositions::None)) {
		for (int i = aroundTile.x - maxDist; i <= aroundTile.x + maxDist; i++) {
			for (int j = aroundTile.y - maxDist; j <= aroundTile.y + maxDist; j++) {
				if (Broodwar->canBuildHere(TilePosition(i, j), buildingType, builder, false)) {
					// units that are blocking the tile
					bool unitsInWay = false;
					for (Unit u : Broodwar->getAllUnits()) {
						if (u->getID() == builder->getID()) { continue; }
						if ((std::abs(u->getTilePosition().x - i) < 4) && (std::abs(u->getTilePosition().y - j) < 4)) { unitsInWay = true; }
					}
					if (!unitsInWay) {
						return TilePosition(i, j);
					}
					// creep for Zerg
					if (buildingType.requiresCreep()) {
						bool creepMissing = false;
						for (int k = i; k <= i + buildingType.tileWidth(); k++) {
							for (int l = j; l <= j + buildingType.tileHeight(); l++) {
								if (!Broodwar->hasCreep(k, l)) { creepMissing = true; }
								break;
							}
						}
						if (creepMissing) { continue; }
					}
				}
			}
		}
		maxDist += 2;
	}

	if (ret == TilePositions::None) { Broodwar << "Unable to find suitable build position for " << buildingType.toString() << std::endl; }
	return ret;
}

TilePosition BuildingPlacer::naturalExpantion(){
	double groundDist = 10000;
	int i = 0;
	int eras = 0;
	TilePosition buildTile = TilePositions::None;
	for (auto &b : InformationManager::baseLocations){
		if (b != ResourceManager::mainBase && b->getGroundDistance(ResourceManager::mainBase) < groundDist){
			groundDist = b->getGroundDistance(ResourceManager::mainBase);
			buildTile = b->getTilePosition();
			eras = i;
		}
		i++;
	}
	InformationManager::baseLocations.erase(InformationManager::baseLocations.begin() + eras);
	return buildTile;
	

	//TODO
	//Search neighbouring regions for baselocation to find the base location for natural expantion
	//Then find a worker to go and build the new center and connect him with the new center.
}