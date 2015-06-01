#include <string>

#include "environment_predictor.h"
#include "lms/datamanager.h"
#include "lms/imaging/converter.h"
#include <lms/imaging/image_factory.h>
#include "lms/imaging/warp.h"
#include "image_objects/environment.h"
#include "cmath"
extern "C"{
#include "kalman_filter_lr.h"
#include "projectPoints.h"
}
bool EnvironmentPredictor::initialize() {
    envInput = datamanager()->readChannel<Environment>(this,"ENVIRONMENT_INPUT");
    envOutput = datamanager()->writeChannel<Environment>(this,"ENVIRONMENT_OUTPUT");

    n = 10;
    delta = 0.2;
    r_fakt=20;
    zustandsVector = emxCreate_real_T(n,1);
    for(int i = 0; i < 10; i++){
        r[i]=0;
    }
    clearMatrix(zustandsVector);

    stateTransitionMatrix = emxCreate_real_T(n,n);
    asEinheitsMatrix(stateTransitionMatrix);
    kovarianzMatrixDesZustandes = emxCreate_real_T(n,n);
    asEinheitsMatrix(kovarianzMatrixDesZustandes);
    kovarianzMatrixDesZustandUebergangs = emxCreate_real_T(n,n);
    clearMatrix(kovarianzMatrixDesZustandUebergangs);

    for(int x = 0; x < n; x++){
        for(int y = 0; y < n; y++){
            kovarianzMatrixDesZustandUebergangs->data[y*n+x]=15*(1-pow(0.2,1/fabs(x-y)));
        }
    }
    return true;
}

bool EnvironmentPredictor::deinitialize() {
    return true;
}

bool EnvironmentPredictor::cycle() {
    //länge der später zu berechnenden Abschnitten
    //convert data to lines
    emxArray_real_T *rx = nullptr;
    emxArray_real_T *ry = nullptr;
    emxArray_real_T *lx = nullptr;
    emxArray_real_T *ly = nullptr;
    emxArray_real_T *mx = nullptr;
    emxArray_real_T *my = nullptr;
    for(const Environment::RoadLane &rl :envInput->lanes){
        if(rl.points().size() == 0)
            continue;
        if(rl.type() == Environment::RoadLaneType::LEFT){
            logger.debug("cycle") << "found left lane: " << rl.points().size();
            convertToKalmanArray(rl,&lx,&ly);
        }else if(rl.type() == Environment::RoadLaneType::RIGHT){
            logger.debug("cycle") << "found right lane: " << rl.points().size();
            convertToKalmanArray(rl,&rx,&ry);
        }else if(rl.type() == Environment::RoadLaneType::MIDDLE){
            logger.debug("cycle") << "found middle lane: " << rl.points().size();
            convertToKalmanArray(rl,&mx,&my);
        }
    }
    if(rx == nullptr){
        logger.debug("right has zero points");
        rx = emxCreate_real_T(0,0);
        ry = emxCreate_real_T(0,0);
    }
    if(lx == nullptr){
        logger.debug("left has zero points");
        lx = emxCreate_real_T(0,0);
        ly = emxCreate_real_T(0,0);
    }
    if(mx == nullptr){
        logger.debug("middle has zero points");
        mx = emxCreate_real_T(0,0);
        my = emxCreate_real_T(0,0);
    }
    //Kalman with middle-lane
    kalman_filter_lr(zustandsVector,stateTransitionMatrix,kovarianzMatrixDesZustandes,
                     kovarianzMatrixDesZustandUebergangs,
                     r_fakt,delta,lx,ly,rx,ry,mx,my);

    createOutput();
    //destroy stuff
    emxDestroyArray_real_T(rx);
    emxDestroyArray_real_T(ry);
    emxDestroyArray_real_T(lx);
    emxDestroyArray_real_T(ly);
    emxDestroyArray_real_T(mx);
    emxDestroyArray_real_T(my);
    return true;
}

void EnvironmentPredictor::createOutput(){
    envOutput->lanes.clear();
    //create middle
    Environment::RoadLane middle;
    middle.type(Environment::RoadLaneType::MIDDLE);
    convertZustandToLane(middle);
    envOutput->lanes.push_back(middle);
}

void EnvironmentPredictor::convertZustandToLane(Environment::RoadLane &output){
    lms::math::vertex2f p1;
    p1.x = 0;
    p1.y = zustandsVector->data[0];
    lms::math::vertex2f p2;
    p2.x = delta*cos(zustandsVector->data[1]);
    p2.y = p1.y + delta*sin(zustandsVector->data[1]);
    double phi = zustandsVector->data[1];
    //add points to lane
    output.points().push_back(p1);
    output.points().push_back(p2);
    for(int i = 2; i < n; i++){
        lms::math::vertex2f pi;
        double dw = 2*acos(delta*zustandsVector->data[i]/2);
        phi = phi -dw-M_PI;
        pi.x = output.points()[i-1].x + delta*cos(phi);
        pi.y = output.points()[i-1].y + delta*sin(phi);
        output.points().push_back(pi);
    }
}

void EnvironmentPredictor::clearMatrix(emxArray_real_T *mat){
    memset(mat->data,0,mat->size[0]*mat->size[1]*sizeof(double));

}

void EnvironmentPredictor::asEinheitsMatrix(emxArray_real_T *mat){
    clearMatrix(mat);
    for(int i = 0; i < mat->size[0]; i++){
        mat->data[i*(mat->size[0]+1)] = 1;
    }
}

void EnvironmentPredictor::convertToKalmanArray(const Environment::RoadLane &lane,emxArray_real_T **x,emxArray_real_T **y){
    int dim = lane.points().size();
    emxArray_real_T *vx = emxCreate_real_T(dim,1);
    emxArray_real_T *vy = emxCreate_real_T(dim,1);
    for(uint i=0;i < lane.points().size(); i++){
        vx->data[i] = lane.points()[i].x;
        vy->data[i] = lane.points()[i].y;
    }
    *x = vx;
    *y = vy;
}

void EnvironmentPredictor::printMat(emxArray_real_T *mat){
    std::cout<<"mat: "<<std::endl;
    for(int x = 0; x < mat->size[0];x++){
        for(int y = 0; y < mat->size[1]; y++){
            std::cout << mat->data[x*mat->size[1]+y];
            std::cout <<",";
        }
        std::cout <<std::endl;
    }
}