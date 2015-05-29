#ifndef IMAGE_HINT_TRANSFORMER_H
#define IMAGE_HINT_TRANSFORMER_H

#include "lms/module.h"
#include "lms/datamanager.h"
#include "lms/imaging_detection/image_hint.h"
#include "image_objects/environment.h"
#include "lms/imaging_detection/line_point.h"
#include <vector>

class ImageHintGenerator : public lms::Module {
public:
    bool initialize() override;
    bool deinitialize() override;
    bool cycle() override;
private:
    void initialHints();
    void createHintsFromMiddleLane();
    const Environment *middleEnv;
    lms::imaging::find::HintContainer *hintContainer;
    lms::imaging::find::HintContainer *hintContainer_Left;
    lms::imaging::find::HintContainer *hintContainer_Right;
    lms::imaging::find::HintContainer *hintContainer_Middle;
    lms::imaging::Image *gaussBuffer;
    /**
     * @brief getTargetImage Dont use that method yet!
     * @param name
     * @return
     */
    const lms::imaging::Image* getTargetImage(std::string name);
    const lms::imaging::Image* target;
    //that's not that nice...
    std::map<std::string,const lms::imaging::Image*> targets;
    lms::imaging::find::LinePoint::LinePointParam defaultLinePointParameter;
};

#endif /* IMAGE_HINT_TRANSFORMER_H */
