#include "street_obstacle_merger.h"
#include "street_environment/obstacle.h"
#include "street_environment/crossing.h"
#include "street_environment/start_line.h"

bool StreetObjectMerger::initialize() {
    envInput = readChannel<street_environment::EnvironmentObjects>("ENVIRONMENT_INPUT");
    envOutput = writeChannel<street_environment::EnvironmentObjects>("ENVIRONMENT_OUTPUT");

    //We should have the roadlane and the car from the current cycle
    car = readChannel<sensor_utils::Car>("CAR");
    middle = readChannel<street_environment::RoadLane>("MIDDLE_LANE");

    if(config().hasKey("visibleAreas")){
        std::vector<std::string> sRects = config().getArray<std::string>("visibleAreas");
        for(std::string &sRect: sRects){
            lms::math::Rect r;
            if(!config().hasKey(sRect+"_x") || !config().hasKey(sRect+"_y") ||!config().hasKey(sRect+"_w")||!config().hasKey(sRect+"_h")){
                logger.error("initialize")<<"visibleArea not valid: "<< sRect;
                return false;
            }
            r.x = config().get<float>(sRect+"_x");
            r.y = config().get<float>(sRect+"_y");
            r.width = config().get<float>(sRect+"_w");
            r.height = config().get<float>(sRect+"_h");
            visibleAreas.push_back(r);
        }
    }
    return true;
}

bool StreetObjectMerger::inVisibleArea(float x, float y){
    for(lms::math::Rect& r:visibleAreas){
        if(r.contains(x,y))
            return true;
    }
    return false;
}

bool StreetObjectMerger::deinitialize() {
    return true;
}

bool StreetObjectMerger::cycle() {

    //get vectors from environments
    street_environment::EnvironmentObstacles obstaclesNew;
    street_environment::EnvironmentObstacles obstaclesOld;


    getObstacles(*envInput,obstaclesNew);
    getObstacles(*envOutput,obstaclesOld);

    logger.debug("cycle")<<"number of old obstacles" << obstaclesOld.objects.size();

    //update old obstacles
    for(std::shared_ptr<street_environment::Obstacle> &obst:obstaclesOld.objects){
        obst->kalman(*middle,car->movedDistance());
    }

    //kalman new obstacles
    for(std::shared_ptr<street_environment::Obstacle> &obst:obstaclesNew.objects){
        obst->kalman(*middle,0);
    }

    logger.debug("cycle")<<"number of new obstacles (before merge)" << obstaclesNew.objects.size();
    //merge them
    merge(obstaclesNew,obstaclesOld);
    logger.debug("cycle")<<"number of obstacles (after merge)" << obstaclesOld.objects.size();

    //create new env output
    createOutput(obstaclesOld);
    logger.debug("cycle")<<"number of obstacles (output)" << envOutput->objects.size();

    return true;
}

void StreetObjectMerger::createOutput(street_environment::EnvironmentObstacles &obstaclesOld){
    //clear old obstacles
    envOutput->objects.clear();
    for(uint i = 0; i < obstaclesOld.objects.size(); i++){
        envOutput->objects.push_back(obstaclesOld.objects[i]);
    }
}


void StreetObjectMerger::merge(street_environment::EnvironmentObstacles &obstaclesNew,street_environment::EnvironmentObstacles &obstaclesOld){
    std::vector<int> verifiedOld;
    int oldSize = obstaclesOld.objects.size();
    //check if obstacles can be merged
    for(uint k = 0; k < obstaclesNew.objects.size();k++){
        bool merged = false;
        for(uint i = 0; i < obstaclesOld.objects.size();i++){
            if(obstaclesNew.objects[k]->getType() != obstaclesOld.objects[i]->getType()){
                continue;
            }
            merged = obstaclesOld.objects[i]->match(*obstaclesNew.objects[k].get());
            if(merged){
                lms::math::vertex2f pos = obstaclesNew.objects[k]->position()+obstaclesOld.objects[i]->position();
                pos = pos*0.5;
                obstaclesOld.objects[i]->updatePosition(pos);
                //TODO create merged object
                //TODO set new trust-value
                float newTrust = obstaclesOld.objects[i]->trust() + obstaclesNew.objects[k]->trust();
                if(newTrust < 0)
                    newTrust = 0;
                else if(newTrust > 1)
                    newTrust = 1;
                obstaclesOld.objects[i]->setTrust(newTrust);
                verifiedOld.push_back(i);
                break;
            }
        }
        if(!merged){
            //Basic value
            obstaclesOld.objects.push_back(obstaclesNew.objects[k]);
        }
    }

    //Decrease trust in obstacles that weren't found
    for(int i = 0; i < oldSize; i++){
        if(std::find(verifiedOld.begin(), verifiedOld.end(), i) == verifiedOld.end()){
           if(!inVisibleArea(obstaclesOld.objects[i]->position().x,
                        obstaclesOld.objects[i]->position().y))
               continue;//obstacle can't be found

            //old obstacle wasn't found
            float dt = obstaclesOld.objects[i]->getDeltaTrust();
            if(dt < 0){
                dt *= 2;
            }else{
                dt = -config().get<float>("trustReducer",0.1);
            }
            float newTrust = obstaclesOld.objects[i]->trust();
            if(newTrust < 0)
                newTrust = 0;
            else if(newTrust > 1)
                newTrust = 1;

            //TODO pos.x isn't nice at all
            if(obstaclesOld.objects[i]->trust() <= 0 || obstaclesOld.objects[i]->position().x < -0.35){
                obstaclesOld.objects.erase(obstaclesOld.objects.begin() + i);
            }else{
                obstaclesOld.objects[i]->setTrust(newTrust);
            }

        }
    }
}

void StreetObjectMerger::getObstacles(const street_environment::EnvironmentObjects &env,street_environment::EnvironmentObstacles &output){
    for(const std::shared_ptr<street_environment::EnvironmentObject> obj : env.objects){
        if(obj->getType() == street_environment::Obstacle::TYPE){
            //that cast ignores, that the obj was const
            std::shared_ptr<street_environment::Obstacle> obst = std::static_pointer_cast<street_environment::Obstacle>(obj);
            output.objects.push_back(obst);
        }else if(obj->getType() == street_environment::Crossing::TYPE){
            std::shared_ptr<street_environment::Obstacle> crossing = std::static_pointer_cast<street_environment::Crossing>(obj);
            output.objects.push_back(crossing);
        }else if(obj->getType() == street_environment::StartLine::TYPE){
            std::shared_ptr<street_environment::StartLine> startLine = std::static_pointer_cast<street_environment::StartLine>(obj);
            output.objects.push_back(startLine);
        }
    }
}
