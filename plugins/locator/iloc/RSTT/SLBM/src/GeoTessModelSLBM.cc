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

// **** _SYSTEM INCLUDES_ ******************************************************

#include <sstream>
#include <cmath>
#include <ctime>

// **** _LOCAL INCLUDES_ *******************************************************

#include "CPPUtils.h"
#include "CpuTimer.h"
#include "GeoTessData.h"
#include "GeoTessProfile.h"
#include "GeoTessModel.h"
#include "GeoTessModelSLBM.h"
#include "IFStreamBinary.h"
#include "GeoTessInterpolatorType.h"
#include "SLBMException.h"

// **** _BEGIN GEOTESS NAMESPACE_ **********************************************

namespace slbm {

// **** _EXPLICIT TEMPLATE INSTANTIATIONS_ *************************************

// **** _STATIC INITIALIZATIONS_************************************************

// **** _FUNCTION IMPLEMENTATIONS_ *********************************************

GeoTessModelSLBM::GeoTessModelSLBM() : GeoTessModel()
{
    init();
}

GeoTessModelSLBM::GeoTessModelSLBM(const string& modelInputFile)
: GeoTessModel()
{
    init();
    loadModel(modelInputFile, ".");
}

GeoTessModelSLBM::GeoTessModelSLBM(const string& modelInputFile, const string& relativeGridPath)
: GeoTessModel()
{
    init();
    loadModel(modelInputFile, relativeGridPath);
}

GeoTessModelSLBM::GeoTessModelSLBM(const string& gridFileName, GeoTessMetaData* metaData)
: GeoTessModel(gridFileName, metaData)
{
    init();
}


GeoTessModelSLBM::GeoTessModelSLBM(GeoTessGrid* grid, GeoTessMetaData* metaData)
: GeoTessModel(grid, metaData)
{
    init();
}

// GeoTessModelSLBM::GeoTessModelSLBM(const GeoTessModelSLBM &other)
// // static map<string, GeoTessGrid*> reuseGridMap;
// {
//     // GeoTessMetaData* metaData;
//     ostringstream desc;
//     desc << "(Copy) " << (other.metaData)->getDescription();
//     metaData = new GeoTessMetaData(*other.metaData);
//     metaData->setDescription(desc.str());
//     metaData->checkComplete();
//     metaData->addReference();

//     // GeoTessGrid* grid;
//     grid = new GeoTessGrid(*other.grid);
//     grid->addReference();

//     // GeoTessProfile*** profiles;
//     profiles = CPPUtils::new2DArray<GeoTessProfile*>(metaData->getNVertices(), metaData->getNLayers());
//     for (int i = 0; i < metaData->getNVertices(); ++i)
//         for (int j = 0; j < metaData->getNLayers(); ++j)
//             profiles[i][j] = other.profiles[i][j]->copy();

//     // GeoTessPointMap* pointMap;
//     pointMap = new GeoTessPointMap(*other.pointMap);

//     // double averageMantleVelocity[2];
//     for (int i=0; i<2; i++)
//         averageMantleVelocity[i] = other.averageMantleVelocity[i];

//     // int fileFormatVer;
//     fileFormatVer = other.fileFormatVer;

//     // vector<vector<UncertaintyPIU*> > piu;
//     piu.resize(other.piu.size());
//     for (int i=0; i<piu.size(); i++)
//     {
//         piu[i].resize(other.piu[i].size());
//         for (int j=0; j<piu[i].size(); j++)
//             if (other.piu[i][j] != NULL)
//                piu[i][j] = new UncertaintyPIU(*other.piu[i][j]);
//     }

//     // vector<UncertaintyPDU*> pdu;
//     pdu.resize(other.pdu.size());
//     for (int i=0; i<pdu.size(); i++)
//         if (other.pdu[i] != NULL)
//             pdu[i] = new UncertaintyPDU(*other.pdu[i]);

// }

/**
 * Destructor.
 */
GeoTessModelSLBM::~GeoTessModelSLBM()
{
}

bool GeoTessModelSLBM::operator == (const GeoTessModelSLBM& other) const
{
    if (abs(averageMantleVelocity[0]/other.averageMantleVelocity[0]-1.) > 1.e-6
            || abs(averageMantleVelocity[0]/other.averageMantleVelocity[0]-1.) > 1.e-6)
        return false;

    for (int i=0; i<piu.size(); ++i)
        for (int j=0; j<piu[i].size(); ++j)
        {
            if (!(piu[i][j] == NULL && other.piu[i][j] == NULL) &&
                    !(*piu[i][j] == *other.piu[i][j]))
                return false;
        }

    for (int i=0; i<pdu.size(); ++i)
        if (!(*pdu[i] == *other.pdu[i]))
            return false;

    if (!GeoTessModel::operator==(other))
        return false;

    return true;
}

void GeoTessModelSLBM::init()
{
    averageMantleVelocity[0]=averageMantleVelocity[1]=0;
}

string GeoTessModelSLBM::toString()
{
    ostringstream os;

    os << GeoTessModel::toString() << endl;

    os << "GeoTessModelSLBM Data:" << endl << endl;

    os << "averageMantleVelocity = [" << averageMantleVelocity[0] << ", "
            << averageMantleVelocity[1] << "]" << endl << endl;

    ostringstream t;
    for (int i=0; i<piu.size(); ++i)
        for (int j=0; j<piu[i].size(); ++j)
            if (piu[i][j] != NULL)
                t << " " << piu[i][j]->getPhaseStr() << "-" << piu[i][j]->getAttributeStr();
    os << "Path independent uncertainty supported phase-attributes:" << t.str() << endl;

    ostringstream t2;
    for (int i=0; i<pdu.size(); ++i)
        if (pdu[i] != NULL)
            t2 << " " << pdu[i]->getPhaseStr();
    os << "Path dependent uncertainty supported phases:" << t2.str() << endl << endl;

    os << "UncertaintyPDU Data: " << endl << endl;
    for (int i=0; i<pdu.size(); ++i)
        os << pdu[i]->toString() << endl;

    return os.str();
}

void GeoTessModelSLBM::loadModelAscii(IFStreamAscii& input, const string& inputDirectory,
        const string& relGridFilePath)
{
    // read all the information from the standard GeoTessModel object, including the
    // model values of P and S velocity
    GeoTessModel::loadModelAscii(input, inputDirectory, relGridFilePath);

    // make sure that middle_crust_N and middle_crust_G are in the right order.
    checkMiddleCrustLayers();

    // read the model type after the base geotess info
    string s;
    input.readLine(s);

    // if there's nothing in string s, this isn't an rstt model
    if (s.length() == 0)
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelAscii()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                " Extra 'SLBM' data is missing." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }

    // get the file format verison
    int fileFormatVersion = input.readInteger();
    fileFormatVer = fileFormatVersion;  // record the version to a property 

    // check to ensure file format is valid (this string of conditionals could be compressed a little,
    // but it's more readable broken down by version and doesn't really impact execution speed)
    if (fileFormatVersion < 1)  // probably something went wrong and the fileFormatVersion is 0
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelAscii()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "SLBM IO Version number is " << fileFormatVersion << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }
    else if (fileFormatVersion < 3)  // fileFormatVersion is 1 or 2
    {
        // first thing after base model info is the name of this class, which should be SLBMN
        if (s != "SLBM" && s != "GeoTessModelSLBM")
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelSLBM::loadModelAscii()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                    " Extra 'SLBM' data is missing." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
    }
    else if (fileFormatVersion >= 3) // v3 files should be of type GeoTessModelSLBM
    {
        if (s != class_name())
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelSLBM::loadModelAscii()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                    " Extra 'SLBM' data is missing." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
    }
    else  // something has gone terribly wrong (negative file format version?)
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelAscii()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "SLBM IO Version number is " << fileFormatVersion << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }

    // read the standard GeoTessModelSLBM info
    averageMantleVelocity[0] = input.readFloat();
    averageMantleVelocity[1] = input.readFloat();

    // read path independent uncertainty (piu) information.
    // np is the number of phases (Pn, Sn, Pg, Lg); always 4
    // na is the number of attribute (TT, SH, AZ); always 3
    // nModels is the number of non-NULL models that actually
    // got written to the file.
    int np = input.readInteger();
    int na = input.readInteger();
    int pIndex, aIndex;
    string phase, attribute;

    piu.resize(np);
    for (int i=0; i<np; i++)
        piu[i].resize(na);

    for (int i=0; i<np*na; ++i)
    {
        phase = input.readString();
        attribute = input.readString();
        pIndex = UncertaintyPIU::getPhase(phase);
        aIndex = UncertaintyPIU::getAttribute(attribute);
        piu[pIndex][aIndex] = UncertaintyPIU::getUncertaintyPIU(input, pIndex, aIndex);
    }

    // if fileFormatVersion == 3 then this file also contains
    // path dependent uncertainty information.
    if (fileFormatVersion >= 3)
    {
        np = input.readInteger();
        pdu.resize(np);
        for (int i = 0; i < np; ++i)
        {
            UncertaintyPDU* u = UncertaintyPDU::getUncertainty(input);
            pdu[u->getPhase()] = u;
        }
    }
}

void GeoTessModelSLBM::writeModelAscii(IFStreamAscii& output, const string& gridFileName)
{
    // make sure that middle_crust_N and middle_crust_G are in the right order.
    checkMiddleCrustLayers();

    // write all the information from the standard GeoTessModel object, including the
    // model values of P and S velocity
    GeoTessModel::writeModelAscii(output, gridFileName);

    output.writeStringNL(class_name());

    // if path dependent uncertainty information is available, write version = 3.
    // if not, write version = 2;
    int fileFormatVersion = pdu.size() == 0 ? 2 : 3;

    output.writeIntNL(fileFormatVersion);

    output.writeFloatNL((float)averageMantleVelocity[0]);
    output.writeFloatNL((float)averageMantleVelocity[1]);

    int nphases = 4;
    int nattributes = 3;

    // path independent uncertainty
    output.writeInt(nphases);
    output.writeString(" ");
    output.writeIntNL(nattributes);

    for (int p=0; p<nphases; ++p)
        for (int a=0; a<nattributes; ++a)
        {
            output.writeString(UncertaintyPIU::getPhase(p));
            output.writeString(" ");
            output.writeStringNL(UncertaintyPIU::getAttribute(a));
            if (piu[p][a] == NULL)
                output.writeStringNL("  0  0");
            else
                output.writeString(piu[p][a]->toStringFile());
        }

    // if path dependent uncertainty information is available, write it
    if (fileFormatVersion == 3)
    {
        output.writeIntNL(pdu.size());
        for (int p=0; p<pdu.size(); ++p)
            pdu[p]->writeFile(output);
    }
}

void GeoTessModelSLBM::loadModelBinary(IFStreamBinary& input, const string& inputDirectory,
        const string& relGridFilePath)
{
    // read all the information from the standard GeoTessModel object, including the
    // model values of P and S velocity
    GeoTessModel::loadModelBinary(input, inputDirectory, relGridFilePath);

    // make sure that middle_crust_N and middle_crust_G are in the right order.
    checkMiddleCrustLayers();

    // read the model type after the base geotess info
    string s;
    input.readString(s);

    // if there's nothing in string s, this isn't an rstt model
    if (s.length() == 0)
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelBinary()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                " Extra 'SLBM' data is missing." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }

    // get the file format verison
    int fileFormatVersion = input.readInt();
    fileFormatVer = fileFormatVersion;  // record the version to a property 

    // check to ensure file format is valid (this string of conditionals could be compressed a little,
    // but it's more readable broken down by version and doesn't really impact execution speed)
    if (fileFormatVersion < 1)  // probably something went wrong and the fileFormatVersion is 0
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelBinary()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "SLBM IO Version number is " << fileFormatVersion << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }
    else if (fileFormatVersion < 3)  // fileFormatVersion is 1 or 2
    {
        // first thing after base model info is the name of this class, which should be SLBMN
        if (s != "SLBM" && s != "GeoTessModelSLBM")
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelSLBM::loadModelBinary()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                    " Extra 'SLBM' data is missing." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
    }
    else if (fileFormatVersion >= 3) // v3 files should be of type GeoTessModelSLBM
    {
        if (s != class_name())
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelSLBM::loadModelBinary()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "While this file is a GeoTessModel file, it does not appear to be GeoTessModelSLBM file." <<
                    " Extra 'SLBM' data is missing." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
    }
    else  // something has gone terribly wrong (negative file format version?)
    {
        ostringstream os;
        os << endl << "ERROR in GeoTessModelSLBM::loadModelBinary()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "SLBM IO Version number is " << fileFormatVersion << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),105);
    }

    // read the standard GeoTessModelSLBM info
    averageMantleVelocity[0] = input.readFloat();
    averageMantleVelocity[1] = input.readFloat();

    // read path independent uncertainty (piu) information.
    // np is the number of phases (Pn, Sn, Pg, Lg); always 4
    // na is the number of attribute (TT, SH, AZ); always 3
    // nModels is the number of non-NULL models that actually
    // got written to the file.
    int np = input.readInt();
    int na = input.readInt();
    int pIndex, aIndex;
    string phase, attribute;

    piu.resize(np);
    for (int i=0; i<np; i++)
        piu[i].resize(na);

    for (int i=0; i<np*na; ++i)
    {
        input.readString(phase);
        input.readString(attribute);
        pIndex = UncertaintyPIU::getPhase(phase);
        aIndex = UncertaintyPIU::getAttribute(attribute);
        piu[pIndex][aIndex] = UncertaintyPIU::getUncertaintyPIU(input, pIndex, aIndex);
    }

    // if fileFormatVersion == 3 then this file also contains
    // path dependent uncertainty information.
    if (fileFormatVersion >= 3)
    {
        np = input.readInt();
        pdu.resize(np);
        for (int i = 0; i < np; ++i)
        {
            UncertaintyPDU* u = UncertaintyPDU::getUncertainty(input);
            pdu[u->getPhase()] = u;
        }
    }
}

void GeoTessModelSLBM::writeModelBinary(IFStreamBinary& output, const string& gridFileName)
{
    // make sure that middle_crust_N and middle_crust_G are in the right order.
    checkMiddleCrustLayers();

    // write all the information from the standard GeoTessModel object, including the
    // model values of P and S velocity
    GeoTessModel::writeModelBinary(output, gridFileName);

    output.writeString(class_name());

    // if path dependent uncertainty information is available, write version = 3.
    // if not, write version = 2;
    int fileFormatVersion = pdu.size() == 0 ? 2 : 3;

    output.writeInt(fileFormatVersion);

    output.writeFloat((float)averageMantleVelocity[0]);
    output.writeFloat((float)averageMantleVelocity[1]);

    output.writeInt(4);
    output.writeInt(3);

    for (int p=0; p<4; ++p)
        for (int a=0; a<3; ++a)
        {
            output.writeString(UncertaintyPIU::getPhase(p));
            output.writeString(UncertaintyPIU::getAttribute(a));
            if (piu[p][a] == NULL)
            {
                output.writeInt(0);
                output.writeInt(0);
            }
            else
                piu[p][a]->writeFile(output);
        }

    // if path dependent uncertainty information is available, write it
    if (fileFormatVersion == 3)
    {
        output.writeInt(pdu.size());
        for (int p=0; p<pdu.size(); ++p)
            if (pdu[p] != NULL)
                pdu[p]->writeFile(output);
    }
}

/**
 * Some old versions of SLBM incorrectly wrote the model slowness values
 * in the wrong order.  They had the attribute names and attribute values
 * for layers middle_crust_N and middle_crust_G reversed.  This
 * method checks to see if the layers are in the right order and,
 * if not, swaps them.
 */
void GeoTessModelSLBM::checkMiddleCrustLayers()
{
    if (getMetaData().getLayerIndex("middle_crust_N") == 3 &&
            getMetaData().getLayerIndex("middle_crust_G") == 4)
    {
        float vn[2], vg[2];
        GeoTessData *dn, *dg;
        vector<string> layerNames;

        getMetaData().getLayerNames(layerNames);
        layerNames[3] = "middle_crust_G";
        layerNames[4] = "middle_crust_N";
        getMetaData().setLayerNames(layerNames);
        for (int v = 0; v < getNVertices(); ++v)
        {
            dn = getProfile(v, 3)->getData(0);
            dg = getProfile(v, 4)->getData(0);

            dn->getValues(vn, 2);
            dg->getValues(vg, 2);
            dn->setValue(0, vg[0]);
            dn->setValue(1, vg[1]);
            dg->setValue(0, vn[0]);
            dg->setValue(1, vn[1]);
        }
    }
}

} // end namespace slbm

