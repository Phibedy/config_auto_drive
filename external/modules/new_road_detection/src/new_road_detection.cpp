#include "new_road_detection.h"
#include <lms/imaging/warp.h>
#include <lms/math/curve.h>
#include <lms/imaging/graphics.h>
#include <local_course/local_course.h>
#include <street_environment/car.h>
#include <detection_utils.h>
#include <warp_service/warp_service.h>

bool NewRoadDetection::init() {

    image = readChannel<lms::imaging::Image>("IMAGE");
    road = writeChannel<street_environment::RoadLane>("ROAD");
    //output = writeChannel<street_environment::RoadLane>("ROAD_OUTPUT");
    debugImage = writeChannel<lms::imaging::Image>("DEBUG_IMAGE");
    debugAllPoints = writeChannel<lms::math::polyLine2f>("DEBUG_ALL_POINTS");
    debugValidPoints = writeChannel<lms::math::polyLine2f>("DEBUG_VALID_POINTS");
    debugTranslatedPoints = writeChannel<lms::math::polyLine2f>("DEBUG_VALID_TRANSLATED_POINTS");

    car = readChannel<street_environment::CarCommand>("CAR"); //TODO create ego-estimation service




    configsChanged();
    return true;
}

bool NewRoadDetection::cycle() {
    if(image->width()*image->height() == 0){
        logger.error("cycle")<<"image size is 0";
        return false;
    }
    //if we have a round, try to find the line
    if(road->points().size() > 1){
        find(); //TODO use bool from find
    }

    float dx = 0;
    float dy = 0;
    float dPhi = 0;
    //update the course
    if(config().get<bool>("translateEnvironment",false)){
        logger.info("translation")<<car->deltaPhi();
        dPhi = car->deltaPhi();
    }

    lms::ServiceHandle<local_course::LocalCourse> localCourse = getService<local_course::LocalCourse>("LOCAL_COURSE_SERVICE");
    if(localCourse.isValid()){
        //TODO detect if the current path is invalid!
        //check if we need to reset the environment
        lms::ServiceHandle<phoenix_CC2016_service::Phoenix_CC2016Service> phoenixService = getService<phoenix_CC2016_service::Phoenix_CC2016Service>("PHOENIX_SERVICE");
        if(!phoenixService.isValid()){
            logger.error("Phoenix_CC2016Service invalid");
            return false;
        }
        if(phoenixService->rcStateChanged() || phoenixService->driveModeChanged()){
            //phoenixService->logRcStates();
            localCourse->resetData();
            logger.warn("reset kalman")<<"rcState or driveMode changed";
        }

        logger.time("localCourse");
        localCourse->update(dx,dy,dPhi);
        *road = localCourse->getCourse();
        logger.timeEnd("localCourse");
        bool renderDebugImage = config().get<bool>("renderDebugImage",false);
        if(renderDebugImage){
                debugTranslatedPoints->points() = localCourse->getPointsAdded();
        }

    }else{
        logger.error("localCourse invalid!");
        return false;
    }
    return true;
}

std::vector<lms::math::vertex2f> NewRoadDetection::findBySobel(
        const bool renderDebugImage,
        const std::vector<int> &xv,
        const std::vector<int> &yv,
        const float minLineWidthMul,
        const float maxLineWidthMul,
        const float iDist,
        const float wDist,
        const int threshold) {
    return ::findBySobel(image.get(),debugImage.get(),renderDebugImage,xv,yv,minLineWidthMul,maxLineWidthMul,0.02,iDist,wDist,threshold,homo);
}
std::vector<lms::math::vertex2f> NewRoadDetection::findByBrightness(const bool renderDebugImage, const std::vector<int> &xv,const std::vector<int> &yv, const float minLineWidthMul, const float maxLineWidthMul,const float iDist,const float wDist, const int threshold){
    lms::imaging::BGRAImageGraphics graphics(*debugImage);
    std::vector<lms::math::vertex2f> foundPoints;
    std::vector<int> color;
    //get the color from the points
    if(renderDebugImage)
        graphics.setColor(lms::imaging::blue);
    for(int k = 0; k < (int)xv.size(); k++){
        const int x = xv[k];
        const int y = yv[k];
        if(!image->inside(x,y)){
            color.push_back(0);
            continue;
        }
        color.push_back(*(image->data()+x+y*image->width()));
        if(renderDebugImage)
            graphics.drawCross(x,y);
    }
    //detect peaks
    float pxlPeakWidth = iDist/wDist*0.02; //TODO to bad, calculate for each road line (how should we use them for searching?
    int tCounter = 0;
    if(renderDebugImage)
        graphics.setColor(lms::imaging::red);
    for(int k = 0; k < (int)color.size(); k++){
        if(color[k]>threshold){
            tCounter++;
        }else{
            if(tCounter - k != 0 && tCounter > pxlPeakWidth*minLineWidthMul && tCounter < pxlPeakWidth*maxLineWidthMul){
                if(renderDebugImage){
                    for(int j = 0; j<tCounter;j++){
                        graphics.drawCross(xv[k-j],yv[k-j]);
                    }
                }
                //we found a valid point
                //get the middle
                lms::math::vertex2i mid;
                lms::math::vertex2f wMid;
                homo.vt(mid,wMid);
                foundPoints.push_back(wMid);
            }
            tCounter = 0;
        }
    }
    return foundPoints;
}

bool NewRoadDetection::find(){
    //clear old lines
    logger.time("create lines");
    lines.clear();
    linesToProcess = 0;

    //TODO rectangle for neglecting areas

    //(TODO calculate threshold for each line)
    if(renderDebugImage){
        //Clear debug image
        debugImage->resize(image->width(),image->height(),lms::imaging::Format::BGRA);
        debugImage->fill(0);
        debugAllPoints->points().clear();
        debugValidPoints->points().clear();
        debugTranslatedPoints->points().clear();
    }

    //create left/mid/right lane
    lms::math::polyLine2f mid = road->getWithDistanceBetweenPoints(config().get<float>("distanceBetweenSearchlines",0.2));
    lms::math::polyLine2f left = mid.moveOrthogonal(-0.4);
    lms::math::polyLine2f right = mid.moveOrthogonal(0.4);
    if(mid.points().size() != left.points().size() || mid.points().size() != right.points().size()){
        logger.error("invalid midlane given!");
        return false;
    }
    //get all lines
    for(int i = 0; i< (int)mid.points().size(); i++){
        SearchLine l;
        l.w_start = left.points()[i];
        l.w_end = right.points()[i];
        l.w_left = left.points()[i];
        l.w_mid = mid.points()[i];
        l.w_right = right.points()[i];
        //check if the part is valid (middle should be always visible)
        if(!image->inside(l.w_start.x,l.w_start.y)){
            l.w_start = mid.points()[i];
            if(!image->inside(l.w_end.x,l.w_end.y)){
                continue;
            }
        }else if(!image->inside(l.w_end.x,l.w_end.y)){
            l.w_end = mid.points()[i];
        }

        //transform them in image-coordinates
        homo.vti(l.i_start,l.w_start);
        homo.vti(l.i_end,l.w_end);
        {
            std::unique_lock<std::mutex> lock(mutex);
            lines.push_back(l);
            linesToProcess ++;
            conditionNewLine.notify_one();
        }
    }
    logger.timeEnd("create lines");

    logger.time("search");
    if(numThreads == 0) {
        // single threaded
        for(SearchLine &l:lines){
            processSearchLine(l);
        }
    } else {
        // multi threaded

        // initialize and start threads if not yet there
        if(threads.size() == 0) {
            threadsRunning = true;
            for(int i = 0; i < numThreads; i++) {
                threads.emplace_back([this] () {
                    threadFunction();
                });
            }
        }

        // wait till every search line was processed
        {
            std::unique_lock<std::mutex> lock(mutex);
            while(linesToProcess > 0) conditionLineProcessed.wait(lock);
        }
    }
    logger.timeEnd("search");
    return true;
}

void NewRoadDetection::threadFunction() {
    while(threadsRunning) {
        SearchLine line;
        {
            std::unique_lock<std::mutex> lock(mutex);
            while(threadsRunning && lines.empty()) conditionNewLine.wait(lock);
            if(lines.size() > 0) {
                line = lines.front();
                lines.pop_front();
            }
            if(! threadsRunning) {
                break;
            }
        }
        processSearchLine(line);
    }
}

void NewRoadDetection::processSearchLine(const SearchLine &l) {
    std::vector<int> xv;
    std::vector<int> yv;

    //calculate the offset
    float iDist = l.i_start.distance(l.i_end);
    float wDist = l.w_start.distance(l.w_end);
    float pxlPerDist = iDist/wDist*searchOffset;
    lms::math::vertex2f iDiff = lms::math::vertex2f(l.i_start-l.i_end).normalize();
    lms::math::vertex2f startLine = lms::math::vertex2f(l.i_start)+iDiff*pxlPerDist;
    lms::math::vertex2f endLine = lms::math::vertex2f(l.i_end)-iDiff*pxlPerDist;
    //get all points in between
    lms::math::bresenhamLine(startLine.x,startLine.y,endLine.x,endLine.y,xv,yv); //wir suchen von links nach rechts!

    //find points
    std::vector<lms::math::vertex2f> foundPoints;
    if(findPointsBySobel){
       foundPoints = findBySobel(renderDebugImage,xv,yv,minLineWidthMul,maxLineWidthMul,iDist,wDist,sobelThreshold);
    }else{
       foundPoints = findByBrightness(renderDebugImage,xv,yv,minLineWidthMul,maxLineWidthMul,iDist,wDist,threshold);
    }

    if(renderDebugImage) {
        std::unique_lock<std::mutex> lock(debugAllPointsMutex);
        for(lms::math::vertex2f &v:foundPoints)
            debugAllPoints->points().push_back(v);
    }

    //filter
    //TODO filter points that are in poluted regions (for example the car itself)
    //remove points with bad distances
    if(foundPoints.size() > 2){
        std::vector<int> foundCounter;
        foundCounter.resize(foundPoints.size(),0);
        for(std::size_t fp = 0; fp < foundPoints.size(); fp++){
            const lms::math::vertex2f &s = foundPoints[fp];
            for(std::size_t test = fp+1; test <foundPoints.size(); test++){
                const lms::math::vertex2f &toTest  = foundPoints[test];
                float distance = toTest.distance(s);
                if((distance > 0.4-laneWidthOffsetInMeter && distance < 0.4 + laneWidthOffsetInMeter)|| (distance > 0.8-laneWidthOffsetInMeter && distance < 0.8+laneWidthOffsetInMeter)){
                    foundCounter[test]++;
                    foundCounter[fp]++;
                }
            }
        }
        //filter, all valid points should have the same counter and have the highest number
        int max = 0;
        for(int c:foundCounter){
            if(c > max){
                max = c;
            }
        }
        std::vector<lms::math::vertex2f> validPoints;
        for(int i = 0; i < (int)foundPoints.size(); i++){
            if(foundCounter[i] >= max){
                validPoints.push_back(foundPoints[i]);
            }
        }
        //TODO if we have more than 3 points we know that there is an error!
        foundPoints = validPoints;
    }

    if(renderDebugImage){
        std::unique_lock<std::mutex> lock(debugValidPointsMutex);
        for(lms::math::vertex2f &v:foundPoints)
            debugValidPoints->points().push_back(v);
    }

    //Handle found points
    lms::math::vertex2f diff;
    std::vector<float> distances;
    for(int i = 0; i < (int)foundPoints.size(); i++){
        lms::math::vertex2f &wMind = foundPoints[i];
        float distanceToLeft = wMind.distance(l.w_left);
        float distanceToRight= wMind.distance(l.w_right);
        float distanceToMid= wMind.distance(l.w_mid);
        diff = (l.w_left-l.w_right).normalize();
        if(distanceToLeft < distanceToMid && distanceToLeft < distanceToRight){
            //left point
            wMind-=diff*0.4;
            distances.push_back(distanceToLeft);
        }else if(distanceToRight < distanceToMid ){
            wMind+=diff*0.4;
            distances.push_back(distanceToRight);
        }else{
            distances.push_back(distanceToMid);
        }
    }

    std::vector<float> weights;
    for(const float &dist:distances){
        //as distance is in meter, we multiply it by 100
        if(useWeights)
            weights.push_back(1/(dist*100+0.001)); //TODO hier etwas sinnvolles überlegen
        else
            weights.push_back(1);
    }
    /*
    if(renderDebugImage){
        for(lms::math::vertex2f &v:foundPoints)
            debugTranslatedPoints->points().push_back(v);
    }
    */
    lms::ServiceHandle<local_course::LocalCourse> localCourse = getService<local_course::LocalCourse>("LOCAL_COURSE_SERVICE");
    if(localCourse.isValid()){
        localCourse->addPoints(foundPoints,weights);
    }else{
        logger.error("localCourse invalid!");
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        linesToProcess --;
        conditionLineProcessed.notify_all();
    }
}

void NewRoadDetection::configsChanged(){
    lms::ServiceHandle<warp_service::WarpService> warp = getService<warp_service::WarpService>("WARP_SERVICE");
    if(!warp.isValid()){
        logger.error("WARP SERVICE is invalid!")<<"shutting down!";
        exit(0); //TODO shut down runtime
    }else{
        homo = warp->getHomography();
    }
    // read config
    searchOffset = config().get<float>("searchOffset",0.1);
    findPointsBySobel = config().get<bool>("findBySobel",true);
    renderDebugImage = config().get<bool>("renderDebugImage",false);
    minLineWidthMul = config().get<float>("minLineWidthMul",0.5);
    maxLineWidthMul = config().get<float>("maxLineWidthMul",1.5);
    threshold = config().get<int>("threshold",200);
    laneWidthOffsetInMeter = config().get<float>("laneWidthOffsetInMeter",0.1);
    useWeights = config().get<bool>("useWeights",false);
    sobelThreshold = config().get<int>("sobelThreshold",200);
    numThreads = config().get<int>("threads",0);
}

void NewRoadDetection::destroy() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        threadsRunning = false;
        conditionNewLine.notify_all();
    }
    for(auto &t : threads) {
        if(t.joinable()) {
            t.join();
        }
    }
}
