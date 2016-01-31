#include "street_obstacle_ir_detector.h"

bool StreetObstacleIRDetector::initialize() {

    sensors = readChannel<sensor_utils::SensorContainer>("SENSORS");
    env = writeChannel<street_environment::EnvironmentObjects>("ENVIRONMENT");
    return true;
}

bool StreetObstacleIRDetector::deinitialize() {
    return true;
}

bool StreetObstacleIRDetector::cycle() {
    if(sensors->hasSensor("LIDAR")){
        std::shared_ptr<sensor_utils::DistanceSensor> lidar = sensors->sensor<sensor_utils::DistanceSensor>("LIDAR");
        float totalDistance = lidar->distance;
        float obstacleMinDistance = config().get<float>("obstacleMinDistance",0.01);
        float obstacleTriggerDistance = config().get<float>("obstacleTriggerDistance",0.20);
        float obstacleWidth = config().get<float>("obstacleWidth",0.1);

        if(totalDistance > obstacleMinDistance &&totalDistance < obstacleTriggerDistance){
            //found some obstacles in reach
            std::shared_ptr<street_environment::Obstacle> obstacle(new street_environment::Obstacle());
            obstacle->updatePosition(lms::math::vertex2f(lidar->totalX(),lidar->totalY()+obstacleWidth/2));
            obstacle->setTrust(config().get<float>("obstacleInitTrust",0.5));
            env->objects.push_back(obstacle);
        }else{
            //no obstacle in reach
        }
    }else{
        logger.warn("cycle")<<"no valid sensor given";
    }

    return true;
}
