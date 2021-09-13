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

// **** _LOCAL INCLUDES_ *******************************************************

#include "GeoTessData.h"
#include "GeoTessProfile.h"
#include "GeoTessModel.h"
#include "GeoTessModelSLBM.h"
#include "GeoTessModelPathUnc.h"
#include "IFStreamBinary.h"
#include "GeoTessInterpolatorType.h"
#include "SLBMException.h"

// **** _BEGIN GEOTESS NAMESPACE_ **********************************************

namespace slbm {

    // **** _EXPLICIT TEMPLATE INSTANTIATIONS_ *************************************

    // **** _STATIC INITIALIZATIONS_************************************************

    // **** _FUNCTION IMPLEMENTATIONS_ *********************************************


    GeoTessModelPathUnc::GeoTessModelPathUnc(vector<UncertaintyPathDep*>& pathDepUncert,
        vector<vector<Uncertainty*> >& uncert)
        : GeoTessModelSLBM(uncert), pathDepUncertainty(pathDepUncert)
    {
    }

    GeoTessModelPathUnc::GeoTessModelPathUnc(const string& modelInputFile, const string& relativeGridPath,
        vector<UncertaintyPathDep*>& pathDepUncert,
        vector<vector<Uncertainty*> >& uncert)
        : GeoTessModelSLBM(uncert), pathDepUncertainty(pathDepUncert)
    {
        loadModel(modelInputFile, relativeGridPath);
    }

    GeoTessModelPathUnc::GeoTessModelPathUnc(const string& modelInputFile,
        vector<UncertaintyPathDep*>& pathDepUncert,
        vector<vector<Uncertainty*> >& uncert)
        : GeoTessModelSLBM(uncert), pathDepUncertainty(pathDepUncert)
    {
        loadModel(modelInputFile, ".");
    }

    GeoTessModelPathUnc::GeoTessModelPathUnc(const string& gridFileName, GeoTessMetaData* metaData,
        vector<UncertaintyPathDep*>& pathDepUncert,
        vector<vector<Uncertainty*> >& uncert, double* avgMantleVel)
        : GeoTessModelSLBM(gridFileName, metaData, uncert, avgMantleVel), pathDepUncertainty(pathDepUncert)
    {
    }

    GeoTessModelPathUnc::GeoTessModelPathUnc(GeoTessGrid* grid, GeoTessMetaData* metaData,
        vector<UncertaintyPathDep*>& pathDepUncert,
        vector<vector<Uncertainty*> >& uncert, double* avgMantleVel)
        : GeoTessModelSLBM(grid, metaData, uncert, avgMantleVel), pathDepUncertainty(pathDepUncert)
    {
    }

    /**
    * Destructor.
    */
    GeoTessModelPathUnc::~GeoTessModelPathUnc()
    {
    }

    /**
    * Test a file to see if it is a GeoTessModelPathUnc file.
    *
    * @param fileName
    * @return true if fileName is a GeoTessModelPathUnc file.
    */
    bool GeoTessModelPathUnc::isGeoTessModelPathUnc(const string& fileName)
    {
        return GeoTessModel::isGeoTessModel(fileName, CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()));
    }

    void GeoTessModelPathUnc::loadModelAscii(IFStreamAscii& input, const string& inputDirectory,
        const string& relGridFilePath)
    {
        // read in the class name and verify this is a GeoTessModelPathUnc file.

        string s;
        if (!input.readLine(s) || s != CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()))
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelAscii()." << endl <<
                "  File " << getMetaData().getInputModelFile() << endl <<
                "  This file is NOT a GeoTessModelPathUnc file. Exiting ..." << endl << endl;
            throw SLBMException(os.str(), 105);
        }

        // read all the information from the standard GeoTessModelSLBM object, including the
        // model values of P and S velocity

        GeoTessModelSLBM::loadModelAscii(input, inputDirectory, relGridFilePath);

        if (!input.readLine(s) || s != CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()))
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelAscii()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "While this file is a GeoTessModelSLBM file, it does not appear to be GeoTessModelPathUnc file. Extra Path Uncertainty data is missing." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
        int version = input.readInteger();
        if (version !=  1)
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelAscii()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "Path Uncertainty Uncertainty IO Version number is " << version << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }

        // read uncertainty information.
        // np is the number of phases (Pn, Sn, Pg, Lg); always 4
        int np = input.readInteger();
        int phase;

        for (int p = 0; p<np; ++p)
        {
            phase = Uncertainty::getPhase(input.readString());
            pathDepUncertainty[phase] = UncertaintyPathDep::getUncertainty(input, phase);
        }

    }

    void GeoTessModelPathUnc::writeModelAscii(IFStreamAscii& output, const string& gridFileName)
    {
        // write low level SLBM model first

        output.writeStringNL(CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()));
        GeoTessModelSLBM::writeModelAscii(output, gridFileName);

        // write the Path Uncertainty (PATHUNC) label and version number for the remaining data

        output.writeStringNL(CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()));
        output.writeIntNL(1); // path uncertainty specific io version number

        // write out path dependent uncertainty

        if (isIOUncertainty())
        {
            // uncertainty
            output.writeStringNL("4");
            for (int p = 0; p<4; ++p)
            {
                output.writeString(UncertaintyPathDep::getPhase(p));
                output.writeString(" ");
                if (pathDepUncertainty[p] == NULL)
                    output.writeStringNL("0");
                else
                    output.writeString(pathDepUncertainty[p]->toStringFile());
            }
        }
        else
            output.writeStringNL("0");
    }

    void GeoTessModelPathUnc::loadModelBinary(IFStreamBinary& input, const string& inputDirectory,
        const string& relGridFilePath)
    {
        // read in the class name and verify this is a GeoTessModelPathUnc file.

        string s;
        input.readCharArray(s, GeoTessModelPathUnc::class_name().size());
        if (s != CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()))
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelAscii()." << endl <<
                "  File " << getMetaData().getInputModelFile() << endl <<
                "  This file is NOT a GeoTessModelPathUnc file. Exiting ..." << endl << endl;
            throw SLBMException(os.str(), 105);
        }

        // read in basic GeoTessModelSLBM information first

        GeoTessModelSLBM::loadModelBinary(input, inputDirectory, relGridFilePath);

        input.readString(s);
        if (s != CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()))
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelBinary()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "While this file is a GeoTessModel file, it does not appear to be GeoTessModelPathUnc file. Extra path uncertainty data is missing." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
        int version = input.readInt();
        if (version != 1)
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelPathUnc::loadModelBinary()." << endl <<
                "File " << getMetaData().getInputModelFile() << endl <<
                "Path Uncertainty IO Version number is " << version << " but that is not a supported version number." << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }

        // read in path dependent uncertainty

        int np = input.readInt();
        if (np > 0)
        {
            if (np != 4)
            {
                ostringstream os;
                os << endl << "ERROR in GeoTessModelPathUnc::loadModelBinary()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "Expecting path dependent uncertainty information for 4 phases." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 105);
            }
            for (int p = 0; p<np; ++p)
            {
                int phase = Uncertainty::getPhase(input.readString());
                // will be null if numDistances is zero
                pathDepUncertainty[phase] = UncertaintyPathDep::getUncertainty(input, phase);
            }
        }
    }

    void GeoTessModelPathUnc::writeModelBinary(IFStreamBinary& output, const string& gridFileName)
    {
        output.writeCharArray(CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()).c_str(), GeoTessModelPathUnc::class_name().length());
        GeoTessModelSLBM::writeModelBinary(output, gridFileName);

        output.writeString(CPPUtils::uppercase_string(GeoTessModelPathUnc::class_name()));
        output.writeInt(1); // path uncertainty specific io version number

        if (isIOUncertainty())
        {
            // uncertainty
            output.writeInt(4);  // 4 phases: Pn, Sn, Pg, Lg
            for (int p = 0; p<4; ++p)
            {
                output.writeString(UncertaintyPathDep::getPhase(p));
                if (pathDepUncertainty[p] != NULL)
                    pathDepUncertainty[p]->writeFile(output);
                else
                {
                    // number of phases = 0
                    output.writeInt(0);
                }
            }
        }
        else { output.writeInt(0);}
    }

} // end namespace slbm
