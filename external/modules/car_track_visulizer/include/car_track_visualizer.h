#ifndef CAR_TRACK_VISULIZER_H
#define CAR_TRACK_VISULIZER_H

#include <lms/module.h>
#include "main_window.h"
#include <street_environment/car.h>
/**
 * @brief LMS module car_track_visulizer
 **/
class CarTrackVisualizer : public lms::Module {
    MainWindow* window;
    lms::ReadDataChannel<street_environment::CarCommand> car,car_ground_truth;
public:
    bool initialize() override;
    bool deinitialize() override;
    bool cycle() override;
};

#endif // CAR_TRACK_VISULIZER_H
