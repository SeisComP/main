//- ****************************************************************************
//-
//- Copyright 2009 National Technology & Engineering Solutions of Sandia, LLC
//- (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
//- Government retains certain rights in this software.
//-
//- BSD Open Source License
//- All rights reserved.
//-
//- Redistribution and use in source and binary forms, with or without
//- modification, are permitted provided that the following conditions are met:
//-
//-   1. Redistributions of source code must retain the above copyright notice,
//-      this list of conditions and the following disclaimer.
//-
//-   2. Redistributions in binary form must reproduce the above copyright
//-      notice, this list of conditions and the following disclaimer in the
//-      documentation and/or other materials provided with the distribution.
//-
//-   3. Neither the name of the copyright holder nor the names of its
//-      contributors may be used to endorse or promote products derived from
//-      this software without specific prior written permission.
//-
//- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//- POSSIBILITY OF SUCH DAMAGE.
//-
//- ****************************************************************************

#include "SlbmInterface.h"
#include "SLBMGlobals.h"
#include "Grid.h"
#include "GridSLBM.h"

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

//using namespace std;

// **** _BEGIN SLBM NAMESPACE_ **************************************************

namespace slbm {

double SlbmInterface::CH_MAX = 0.2;

SlbmInterface::SlbmInterface() : 
    grid(NULL), 
    greatCircle(NULL),
    valid(false),
    sphase(""), iphase(-1),
    srcLat(NaN_DOUBLE), srcLon(NaN_DOUBLE), srcDep(NaN_DOUBLE),
    rcvLat(NaN_DOUBLE), rcvLon(NaN_DOUBLE), rcvDep(NaN_DOUBLE)
{
    // DEBUG_MSG(__FILE__, __FUNCTION__, this, "Created SlbmInterface.");
}  // END SlbmInterface Default Constructor

// SlbmInterface::SlbmInterface(const SlbmInterface &other) :
//     grid(other.grid->clone()), greatCircle(other.greatCircle->clone()),
//     valid(other.valid), sphase(other.sphase), iphase(other.iphase),
//     srcLat(other.srcLat), srcLon(other.srcLon), srcDep(other.srcDep),
//     rcvLat(other.rcvLat), rcvLon(other.rcvLon), rcvDep(other.rcvDep)
// {
//     // DEBUG_MSG(__FILE__, __FUNCTION__, this, "Copied SlbmInterface.");
// }

SlbmInterface::SlbmInterface(const double& earthRadius) : 
    grid(NULL), 
    greatCircle(NULL), 
    valid(false),
    sphase(""), iphase(-1),
    srcLat(NaN_DOUBLE), srcLon(NaN_DOUBLE), srcDep(NaN_DOUBLE),
    rcvLat(NaN_DOUBLE), rcvLon(NaN_DOUBLE), rcvDep(NaN_DOUBLE)
{
    Location::EARTH_RADIUS = earthRadius;
}  // END SlbmInterface Default Constructor

SlbmInterface::~SlbmInterface()
{
    // DEBUG_MSG(__FILE__, __FUNCTION__, this, "Deleting SlbmInterface...");
    clear();
    if (grid)
        delete grid;

    valid = false;
    // DEBUG_MSG(__FILE__, __FUNCTION__, this, "Deleted SlbmInterface.");
}  // END SlbmInterface Destructor

void SlbmInterface::clear()
{
    // DEBUG_MSG(__FILE__, __FUNCTION__, this, "Clearing SlbmInterface...");
    clearGreatCircles();
    if (grid)
        grid->clearCrustalProfiles();

    // crustalProfiles will accumulate for every cloned SlbmInterface, but
    // that's not a big deal, and profiles need to be periodically cleared for
    // performance, anyways. Calls to clear() should be controlled by the
    // non-cloned SlbmInterface.
    valid = false;
    // DEBUG_MSG(__FILE__, __FUNCTION__, this, "SlbmInterface cleared.");
}

void SlbmInterface::clearGreatCircles()
{
    if (greatCircle)
    {
        delete greatCircle;
        greatCircle = NULL;
    }
    sphase=""; iphase=-1;
    srcLat=srcLon=srcDep=rcvLat=rcvLon=rcvDep=NaN_DOUBLE;
}

void  SlbmInterface::loadVelocityModel(const string& modelFileName)
{
    if (grid)
        delete grid;

    grid = NULL;
    grid = Grid::getGrid(modelFileName);
}

void SlbmInterface::saveVelocityModel(const string& fname, const int& format)
{
    if (!grid)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::saveVelocityModel" << endl
            << "There is no grid in memory to save." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }
    grid->saveVelocityModel(fname, format);
}

void SlbmInterface::saveVelocityModelBinary()
{
    if (!grid)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::saveVelocityModelBinary" << endl
            << "There is no grid in memory to save." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }
    grid->saveVelocityModel(grid->getOutputDirectory(), 3);
}

void SlbmInterface::specifyOutputDirectory(const string& directoryName)
{
    if (!grid)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::specifyOutputDirectory" << endl
            << "There is no grid in memory to save." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }
    grid->specifyOutputDirectory(directoryName);
}

void  SlbmInterface::loadVelocityModelBinary(util::DataBuffer& buffer)
{
    if (grid)
        delete grid;

    grid = Grid::getGrid(buffer);

}

void SlbmInterface::saveVelocityModelBinary(util::DataBuffer& buffer)
{
    if (!grid)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::saveVelocityModelBinary" << endl
            << "There is no grid in memory to save." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }

    grid->saveVelocityModel(buffer);
}

int SlbmInterface::getBufferSize() const
{
    if (!grid)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::getBufferSize()" << endl
            << "There is no grid in memory." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }

    return grid->getBufferSize();
}

void SlbmInterface::createGreatCircle(
            const int& phase,
            const double& sourceLat,
            const double& sourceLon,
            const double& sourceDepth,
            const double& receiverLat,
            const double& receiverLon,
            const double& receiverDepth)
{
    clearGreatCircles();

    // save copies of source and receiver position (radians)
    sphase = (phase==Pn ? "Pn" : (phase==Sn ? "Sn" : (phase==Pg ? "Pg" : (phase==Lg ? "Lg" : "unknown phase"))));
    iphase = phase;
    srcLat = sourceLat;
    srcLon = sourceLon;
    srcDep = sourceDepth;
    rcvLat = receiverLat;
    rcvLon = receiverLon;
    rcvDep = receiverDepth;

    valid = false;
    
    greatCircle = GreatCircleFactory::create(
        phase, grid, sourceLat, sourceLon, sourceDepth,
        receiverLat, receiverLon, receiverDepth, CH_MAX);

    valid = true;
}

void SlbmInterface::getGridData(
        const int& nodeId,
        double& latitude,
        double& longitude,
        double depth[NLAYERS],
        double pvelocity[NLAYERS],
        double svelocity[NLAYERS],
        double gradient[2])
{
    if (nodeId < 0 || nodeId >= grid->getNNodes())
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::getGridData" << endl
            << "Specified grid nodeId, " << nodeId <<", "
            << " is out of range.  Must be less than " 
            << grid->getNNodes() << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),110);
    }
    GridProfile* profile = grid->getProfile(nodeId);
    latitude = profile->getLat();
    longitude = profile->getLon();
    profile->getData(depth, pvelocity, svelocity, gradient);
}

void SlbmInterface::getActiveNodeData(
        const int& nodeId,
        double& latitude,
        double& longitude,
        double depth[NLAYERS],
        double pvelocity[NLAYERS],
        double svelocity[NLAYERS],
        double gradient[2])
{
    getGridData(grid->getGridNodeId(nodeId), latitude, longitude, depth, 
        pvelocity, svelocity, gradient);
}

void SlbmInterface::setGridData(
        const int& nodeId,
        double depths[NLAYERS], 
        double pvelocity[NLAYERS], 
        double svelocity[NLAYERS],
        double gradient[2])
{
    if (nodeId < 0 || nodeId >= grid->getNNodes())
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::setGridData" << endl
            << "Specified grid nodeId, " << nodeId <<", "
            << " is out of range.  Must be less than " 
            << grid->getNNodes() << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),111);
    }
    GridProfile* gridProfile = grid->getProfile(nodeId);
    gridProfile->setData(depths, pvelocity, svelocity, gradient);
}

void SlbmInterface::setActiveNodeData(
        const int& nodeId,
        double depths[NLAYERS], 
        double pvelocity[NLAYERS], 
        double svelocity[NLAYERS],
        double gradient[2])
{
    setGridData(grid->getGridNodeId(nodeId), depths, pvelocity, svelocity, gradient);
}

void SlbmInterface::setMaxDistance(const double& maxDistance)
{
  GreatCircle::MAX_DISTANCE = maxDistance;
}

void SlbmInterface::getMaxDistance(double& maxDistance)
{
  maxDistance = GreatCircle::MAX_DISTANCE;
}

void SlbmInterface::setMaxDepth(const double& maxDepth)
{
  GreatCircle_Xn::MAX_DEPTH = maxDepth;
}

void SlbmInterface::getMaxDepth(double& maxDepth)
{
  maxDepth = GreatCircle_Xn::MAX_DEPTH;
}

void SlbmInterface::setCHMax(const double& chMax)
{
  CH_MAX = chMax;
}

void SlbmInterface::getCHMax(double& chMax)
{
  chMax = CH_MAX;
}

void SlbmInterface::setDelDistance(const double& del_distance)
{ GreatCircle::setDelDistance(del_distance); }

void SlbmInterface::getDelDistance(double& del_distance)
{ del_distance = GreatCircle::getDelDistance(); }

void SlbmInterface::setDelDepth(const double& del_depth)
{ GreatCircle::setDelDepth(del_depth); }

void SlbmInterface::getDelDepth(double& del_depth)
{ del_depth = GreatCircle::getDelDepth(); }

void SlbmInterface::getRayParameter(double& ray_parameter)
{ ray_parameter = greatCircle->getRayParameter(); }

void SlbmInterface::getTurningRadius(double& turning_radius)
{ turning_radius = greatCircle->getTurningRadius(); }

void SlbmInterface::setPathIncrement(const double& path_increment)
{ GreatCircle::setPathIncrement(path_increment); }

void SlbmInterface::getPathIncrement(double& path_increment)
{ path_increment = GreatCircle::getPathIncrement(); }

double SlbmInterface::getPathIncrement()
{ return GreatCircle::getPathIncrement(); }

void SlbmInterface::getTravelTimeUncertainty(const int& phase, const double& distance, double& uncert)
{
    if (grid->getUncertainty()[phase][TT] == NULL)
    {
        ostringstream os;
        os << setiosflags(ios::fixed) << setiosflags(ios::showpoint) << setprecision(9);
        os << endl << "ERROR in SlbmInterface::getTravelTimeUncertainty" << endl
            << "Uncertainty object is invalid." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(), 602);
    }
    else
    {
        uncert = grid->getUncertainty()[phase][TT]->getUncertainty(distance);
    }
}

void SlbmInterface::getTravelTimeUncertainty(double& travelTimeUncertainty, bool calcRandomError)
{
    if (!grid)
    {
        ostringstream os;
        os << setiosflags(ios::fixed) << setiosflags(ios::showpoint) << setprecision(9);
        os << endl << "ERROR in SlbmInterface::getTravelTimeUncertainty" << endl
            << "Grid is invalid.  Has the earth model been loaded with call to loadVelocityModel()?" << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(), 114);
    }
    if (!isValid())
    {
        ostringstream os;
        os << setiosflags(ios::fixed) << setiosflags(ios::showpoint) << setprecision(9);
        os << endl << "ERROR in SlbmInterface::getTravelTimeUncertainty" << endl
            << "GreatCircle is invalid." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(), 113);
    }

    // test for path dependent error definition

    if ((grid->getUncertaintyPDU().size() > 0) &&
        (grid->getUncertaintyPDU()[greatCircle->getPhase()] != NULL))
    {
        // path dependent uncertainty is defined ... get crust and head wave influence node ids and weights

        vector<int> crustNodeIds;
        vector<double> crustWeights;
        vector<int> headWaveNodeIds;
        vector<double> headWaveWeights;
        greatCircle->getWeights(crustNodeIds, crustWeights, false);
        greatCircle->getWeights(headWaveNodeIds, headWaveWeights, true);

        // convert crustal weights from km to seconds
        double sSource, sReceiver, tTotal, tSource, tReceiver, tMantle, tGradient;
        greatCircle->getTravelTime(tTotal, tSource, tReceiver, tMantle, tGradient);
        sSource   = greatCircle->getSourceDistanceS();
        sReceiver = greatCircle->getReceiverDistanceS();
        for (int i = 0; i < crustWeights.size(); ++i)
            crustWeights[i] *= (tSource + tReceiver) / (sSource + sReceiver);  // multiply in time, divide out distance
        
        // get ray parameter
        // double rayParameter = greatCircle->getRayParameter();

        // get node neighbors of each headwave node
        int numheadWaveNodes = headWaveNodeIds.size();  // number of nodes
        vector<vector<int> > headWaveNodeNeighbors(numheadWaveNodes);  // vector of vectors to hold nodes
        for (int i = 0; i < numheadWaveNodes; ++i)
            (*grid).getNodeNeighbors(headWaveNodeIds[i], headWaveNodeNeighbors[i]);  // put vector of neighbors into headWaveNodeNeighbors[i]

        // calculate travel time uncertainty

        travelTimeUncertainty = grid->getUncertaintyPDU()[greatCircle->getPhase()]->
            getUncertainty(greatCircle->getDistance() * RAD_TO_DEG,
                crustNodeIds, crustWeights, headWaveNodeIds, headWaveWeights,
                headWaveNodeNeighbors, calcRandomError);
    }
    else
    {
        // not path dependent ... use old weight uncertainty calculation

        getTravelTimeUncertainty(greatCircle->getPhase(),
            greatCircle->getDistance(), travelTimeUncertainty);
    }
}

void SlbmInterface::getTravelTimeUncertainty1D(double& travelTimeUncertainty)
{
    if (!grid)
    {
        ostringstream os;
        os << setiosflags(ios::fixed) << setiosflags(ios::showpoint) << setprecision(9);
        os << endl << "ERROR in SlbmInterface::getTravelTimeUncertainty" << endl
            << "Grid is invalid.  Has the earth model been loaded with call to loadVelocityModel()?" << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(), 114);
    }
    if (!isValid())
    {
        ostringstream os;
        os << setiosflags(ios::fixed) << setiosflags(ios::showpoint) << setprecision(9);
        os << endl << "ERROR in SlbmInterface::getTravelTimeUncertainty" << endl
            << "GreatCircle is invalid." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(), 113);
    }

    // return old weight uncertainty calculation

    getTravelTimeUncertainty(greatCircle->getPhase(),
        greatCircle->getDistance(), travelTimeUncertainty);
}

bool SlbmInterface::isEqual(SlbmInterface *other)
{
    // check to make sure we have grids
    Grid* grid1 = getGridObject();
    Grid* grid2 = other->getGridObject();

    // throw an error if either the current or the second SlbmInterface instance
    // haven't loaded a model
    if (!grid || !grid2)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::isEqual()" << endl
            << "SlbmInterface object has not yet loaded a velocity model." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }

    // throw an error if one SlbmInterface has a greatCircle and the other doesn't
    if (isValid() + (*other).isValid() == 1)
    {
        ostringstream os;
        os << endl << "ERROR in SlbmInterface::isEqual()" << endl
            << "One SlbmInterface object has a greatCircle and the other does not." << endl
            << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),109);
    }

    // if both have a greatCircle
    if (isValid() + (*other).isValid() == 2)
    {
        // get great circle objects
        GreatCircle *gc1 = getGreatCircleObject();
        GreatCircle *gc2 = other->getGreatCircleObject();

        // compare the two models and great circles
        return ( *(grid1->getModel()) == *(grid2->getModel()) && *gc1 == *gc2 );
    }

    // neither have a greatCircle
    else
    {
        // compare just the two models
        return ( *(grid1->getModel()) == *(grid2->getModel()) );
    }

}

bool SlbmInterface::modelsEqual(const string modelPath1, const string modelPath2)
{
    // instatiate two new grid objects and load the models
    Grid* grid1 = Grid::getGrid(modelPath1);
    Grid* grid2 = Grid::getGrid(modelPath2);

    // compare the two models
    return ( *(grid1->getModel()) == *(grid2->getModel()) );
}


} // end slbm namespace
