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
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>

//using namespace std;

#include "UncertaintyPathDep.h"
#include "SLBMException.h"
#include "IFStreamAscii.h"
#include "CPPUtils.h"

using namespace geotess;

// **** _BEGIN SLBM NAMESPACE_ **************************************************

namespace slbm {

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Default Constructor
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep() : fname("not_specified"), phaseNum(-1)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM.
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep(int phase)
        : fname("not_specified"), phaseNum(phase)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM.
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep(const string& phase)
        : fname("not_specified"), phaseNum(getPhase(phase))
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM. Phase is
    // used to construct file name which is loaded from the input path.
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep(string modelPath, const string& phase)
        : fname("not_specified"), phaseNum(getPhase(phase))
    {
        fname = "UncertaintyPathDep_" + phase + ".txt";
        fname = CPPUtils::insertPathSeparator(modelPath, fname);

        readFile(fname);
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM. Phase is
    // used to construct file name which is loaded from the input path.
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep(string modelPath, int phase)
        : fname("not_specified"), phaseNum(phase)
    {
        fname = "Uncertainty_" + getPhase(phaseNum) + ".txt";
        fname = CPPUtils::insertPathSeparator(modelPath, fname);

        readFile(fname);
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Copy constructor.
    //
    // *****************************************************************************
    UncertaintyPathDep::UncertaintyPathDep(const UncertaintyPathDep& u) :
        fname(u.fname),
        phaseNum(u.phaseNum),
        pathUncCrustError(u.pathUncCrustError),
        pathUncRandomError(u.pathUncRandomError),
        pathUncDistanceBins(u.pathUncDistanceBins),
        pathUncModelError(u.pathUncModelError),
        pathUncBias(u.pathUncBias)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Uncertainty Destructor
    //
    // *****************************************************************************
    UncertaintyPathDep::~UncertaintyPathDep()
    {
        fname = "not_specified";
        phaseNum = -1;
        pathUncCrustError.clear();
        pathUncRandomError.clear();
        pathUncDistanceBins.clear();
        pathUncModelError.clear();
        pathUncBias.clear();
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Assignment operator.
    //
    // *****************************************************************************
    UncertaintyPathDep& UncertaintyPathDep::operator=(const UncertaintyPathDep& u)
    {
        phaseNum = u.phaseNum;
        pathUncCrustError = u.pathUncCrustError;
        pathUncRandomError = u.pathUncRandomError;
        pathUncDistanceBins = u.pathUncDistanceBins;
        pathUncModelError = u.pathUncModelError;
        pathUncBias = u.pathUncBias;

        return *this;
    }

    bool UncertaintyPathDep::operator==(const UncertaintyPathDep& other)
    {
        if (this->phaseNum != other.phaseNum) return false;
        if (this->pathUncCrustError.size() != other.pathUncCrustError.size()) return false;
        if (this->pathUncRandomError.size() != other.pathUncRandomError.size()) return false;
        if (this->pathUncDistanceBins.size() != other.pathUncDistanceBins.size()) return false;
        if (this->pathUncModelError.size() != other.pathUncModelError.size()) return false;
        if (this->pathUncBias.size() != other.pathUncBias.size()) return false;

        for (int i = 0; i < (int)pathUncDistanceBins.size(); ++i)
            if (this->pathUncDistanceBins[i] != other.pathUncDistanceBins[i])
                return false;

        for (int i = 0; i < (int)pathUncCrustError.size(); ++i)
        {
            if (this->pathUncCrustError[i] != other.pathUncCrustError[i])
                return false;
        }

        for (int i = 0; i < (int)pathUncRandomError.size(); ++i)
        {
            const vector<double>& thisRandomError_i = this->pathUncRandomError[i];
            const vector<double>& otherRandomError_i = other.pathUncRandomError[i];
            if (thisRandomError_i.size() != otherRandomError_i.size()) return false;
            for (int j = 0; j < (int)thisRandomError_i.size(); ++j)
                if (thisRandomError_i[j] != otherRandomError_i[j])
                    return false;
        }

        for (int i = 0; i < (int)pathUncModelError.size(); ++i)
        {
            const vector<double>& thisModelError_i = this->pathUncModelError[i];
            const vector<double>& otherModelError_i = other.pathUncModelError[i];
            if (thisModelError_i.size() != otherModelError_i.size()) return false;
            for (int j = 0; j < (int)thisModelError_i.size(); ++j)
                if (thisModelError_i[j] != otherModelError_i[j])
                    return false;
        }

        for (int i = 0; i < (int)pathUncBias.size(); ++i)
        {
            const vector<double>& thisBias_i = this->pathUncBias[i];
            const vector<double>& otherBias_i = other.pathUncBias[i];
            if (thisBias_i.size() != otherBias_i.size()) return false;
            for (int j = 0; j < (int)thisBias_i.size(); ++j)
                if (thisBias_i[j] != otherBias_i[j])
                    return false;
        }

        return true;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(ifstream& input, int phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(ifstream& input, const string& phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(geotess::IFStreamAscii& input, int phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(geotess::IFStreamAscii& input, const string& phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(geotess::IFStreamBinary& input, int phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(geotess::IFStreamBinary& input, const string& phase)
    {
        UncertaintyPathDep* u = new UncertaintyPathDep(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPathDep* UncertaintyPathDep::getUncertainty(const string& modelPath, int phase)
    {
        string fname = "Uncertainty_" + getPhase(phase) + ".txt";
        fname = CPPUtils::insertPathSeparator(modelPath, fname);

        ifstream fin;
        fin.open(fname.c_str());
        if (fin.fail() || !fin.is_open())
            return NULL;

        UncertaintyPathDep* u = getUncertainty(fin, phase);
        fin.close();

        return u;
    }

    void UncertaintyPathDep::readFile(const string& filename)
    {
        ifstream fin;

        //open the file.  Return an error if cannot open the file.
        fin.open(filename.c_str());
        if (fin.fail() || !fin.is_open())
        {
            ostringstream os;
            os << endl << "ERROR in Uncertainty::readFile" << endl
                << "Could not open file " << filename << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }

        readFile(fin);
        fin.close();
    }

    void UncertaintyPathDep::readFile(ifstream& fin)
    {
        //clear point arrays
        pathUncCrustError.clear();   //points
        pathUncRandomError.clear();  //points
        pathUncDistanceBins.clear(); //dist
        pathUncModelError.clear();   //points, dist
        pathUncBias.clear();         //points, dist

        try
        {
            string comment;

            int numPoints;
            int numDistances;
            bool usesRandomError;

            // read in grid id
            fin >> gridId;

            // read in number of grid vertices (points) and distances
            fin >> numPoints;
            fin >> numDistances;
            fin >> usesRandomError;

            // valid only if number of points and distances are greater than zero
            if (numPoints > 0)
            {
                if (numDistances > 0)
                {
                    // read in distance bins
                    fin >> comment; // ignore comment
                    pathUncDistanceBins.resize(numDistances);
                    for (int i = 0; i < numDistances; i++)
                        fin >> pathUncDistanceBins[i];

                    // read in crust error for each point
                    fin >> comment; // ignore comment
                    pathUncCrustError.resize(numPoints);
                    for (int i = 0; i < numPoints; i++)
                        fin >> pathUncCrustError[i];

                    // read in random error for each point. Only read if present
                    // (usesRandomError = true)
                    fin >> comment; // ignore comment
                    if (usesRandomError) {
                        pathUncRandomError.resize(numDistances);
                        for (int i = 0; i < numDistances; ++i)
                        {
                            pathUncRandomError[i].resize(numPoints);
                            vector<double>& randomError_i = pathUncRandomError[i];
                            for (int j = 0; j < numPoints; j++)
                                fin >> randomError_i[j];
                        }
                    }

                    // read in model error for all distance bins of all points
                    fin >> comment; // ignore comment
                    pathUncModelError.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncModelError[i].resize(numPoints);
                        vector<double>& modelError_i = pathUncModelError[i];
                        for (int j = 0; j < numPoints; j++)
                            fin >> modelError_i[j];
                    }

                    // read in bias for all distance bins of all points
                    fin >> comment; // ignore comment
                    pathUncBias.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncBias[i].resize(numPoints);
                        vector<double>& bias_i = pathUncBias[i];
                        for (int j = 0; j < numPoints; j++)
                            fin >> bias_i[j];
                    }
                }
            }
        }
        catch (...)
        {
            ostringstream os;
            os << endl << "ERROR in Uncertainty::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }

    }

    void UncertaintyPathDep::writeFile(geotess::IFStreamAscii& output)
    {
        int nlCount = 8;

        output.writeStringNL("# UncertaintyPathDep File");
        output.writeStringNL("# Created for Seismic Phase " + getPhaseStr());
        output.writeStringNL("");

        output.writeStringNL("# Number of Distance Bins, Number of Grid Points, Uses Random Error boolean");
        output.writeString("  ");
        output.writeInt(pathUncDistanceBins.size());
        output.writeString("  ");
        output.writeInt(pathUncModelError.size());
        output.writeString("  ");
        output.writeBoolNL(pathUncRandomError.size() > 0);
        output.writeStringNL("");

        output.writeStringNL("# Created for GeoTessGrid Grid Id:");
        output.writeStringNL(getGridId());
        output.writeStringNL("");

        output.writeString("# Path Dependent Travel Time Uncertainty Distance Bins (km) [");
        output.writeInt(pathUncDistanceBins.size());
        output.writeStringNL("]");
        for (int i = 0; i < pathUncDistanceBins.size(); i++)
        {
            output.writeDouble(pathUncDistanceBins[i]);
            if ((i % nlCount == 0) && (i > 0))
                output.writeStringNL("");
            else
                output.writeString("  ");
        }
        output.writeStringNL("");

        output.writeString("# Path Dependent Travel Time Crust Error (sec) [");
        output.writeInt(pathUncCrustError.size());
        output.writeStringNL("]");
        for (int i = 0; i < pathUncCrustError.size(); i++)
        {
            output.writeDouble(pathUncCrustError[i]);
            if ((i % nlCount == 0) && (i > 0))
                output.writeStringNL("");
            else
                output.writeString("  ");
        }
        output.writeStringNL("");

        if (pathUncRandomError.size() > 0)
        {
            output.writeStringNL("# Path Dependent Travel Time Random Error (sec) [");
            output.writeInt(pathUncDistanceBins.size());
            output.writeString("][");
            output.writeInt(pathUncRandomError.size());
            output.writeStringNL("]");
            for (int i = 0; i < pathUncRandomError.size(); i++)
            {
                output.writeString("#        Point[");
                output.writeInt(i);
                output.writeStringNL("][]");
                vector<double>& randomError_i = pathUncRandomError[i];
                for (int j = 0; j < randomError_i.size(); j++)
                {
                    output.writeDouble(randomError_i[j]);
                    if ((j % nlCount == 0) && (j > 0))
                        output.writeStringNL("");
                    else
                        output.writeString("  ");
                }
                output.writeStringNL("");
            }
            output.writeStringNL("");
        }

        output.writeStringNL("# Path Dependent Travel Time Model Error (sec) [");
        output.writeInt(pathUncDistanceBins.size());
        output.writeString("][");
        output.writeInt(pathUncModelError.size());
        output.writeStringNL("]");
        for (int i = 0; i < pathUncModelError.size(); i++)
        {
            output.writeString("#        Point[");
            output.writeInt(i);
            output.writeStringNL("][]");
            vector<double>& modelError_i = pathUncModelError[i];
            for (int j = 0; j < modelError_i.size(); j++)
            {
                output.writeDouble(modelError_i[j]);
                if ((j % nlCount == 0) && (j > 0))
                    output.writeStringNL("");
                else
                    output.writeString("  ");
            }
            output.writeStringNL("");
        }
        output.writeStringNL("");

        output.writeStringNL("# Path Dependent Travel Time Bias (sec) [");
        output.writeInt(pathUncDistanceBins.size());
        output.writeString("][");
        output.writeInt(pathUncBias.size());
        output.writeStringNL("]");
        for (int i = 0; i < pathUncBias.size(); i++)
        {
            output.writeString("#        Point[");
            output.writeInt(i);
            output.writeStringNL("][]");
            vector<double>& bias_i = pathUncBias[i];
            for (int j = 0; j < bias_i.size(); j++)
            {
                output.writeDouble(bias_i[j]);
                if ((j % nlCount == 0) && (j > 0))
                    output.writeStringNL("");
                else
                    output.writeString("  ");
            }
            output.writeStringNL("");
        }
        output.writeStringNL("");

        output.writeStringNL("# EOF");
    }

    void UncertaintyPathDep::readFile(geotess::IFStreamAscii& input)
    {
        try
        {
            //clear point arrays
            pathUncCrustError.clear();   //points
            pathUncRandomError.clear();  //points
            pathUncDistanceBins.clear(); //dist
            pathUncModelError.clear();   //points, dist
            pathUncBias.clear();         //points, dist

            // Read in number of points, number of distances, and a boolean
            // (0=false, >1=true) indicating random error is stored (if true).
            string line;
            input.getLine(line);
            vector<string> tokens;
            geotess::CPPUtils::tokenizeString(line, " ", tokens);
            int numDistances = geotess::CPPUtils::stoi(tokens[1]);
            int numPoints = geotess::CPPUtils::stoi(tokens[0]);
            bool usesRandomError = geotess::CPPUtils::stoi(tokens[2]) > 0;

            // Read in grid id

            gridId = input.readString();

            //valid only if number of points and distances are greater than zero
            if (numPoints > 0)
            {
                if (numDistances > 0)
                {
                    // read in distance bins
                    pathUncDistanceBins.resize(numDistances);
                    for (int i = 0; i < numDistances; i++)
                        pathUncDistanceBins[i] = input.readDouble();

                    // read in crust error for each point
                    pathUncCrustError.resize(numPoints);
                    for (int i = 0; i < numPoints; i++)
                        pathUncCrustError[i] = input.readDouble();

                    // read in random error for each point. Only read if present
                    // (usesRandomError = true)
                    if (usesRandomError) {
                        pathUncRandomError.resize(numDistances);
                        for (int i = 0; i < numDistances; ++i)
                        {
                            pathUncRandomError[i].resize(numPoints);
                            vector<double>& randomError_i = pathUncRandomError[i];
                            for (int j = 0; j < numPoints; j++)
                                randomError_i[j] = input.readDouble();
                        }
                    }

                    // read in model error for all distance bins of all points
                    pathUncModelError.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncModelError[i].resize(numPoints);
                        vector<double>& modelError_i = pathUncModelError[i];
                        for (int j = 0; j < numPoints; j++)
                            modelError_i[j] = input.readDouble();
                    }

                    // read in bias for all distance bins of all points
                    pathUncBias.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncBias[i].resize(numPoints);
                        vector<double>& bias_i = pathUncBias[i];
                        for (int j = 0; j < numPoints; j++)
                            bias_i[j] = input.readDouble();
                    }
                }
            }
        }
        catch (...)
        {
            ostringstream os;
            os << endl << "ERROR in Uncertainty::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }
    }

    void UncertaintyPathDep::readFile(geotess::IFStreamBinary& input)
    {
        try
        {
            //clear point arrays
            pathUncCrustError.clear();   //points
            pathUncRandomError.clear();  //points
            pathUncDistanceBins.clear(); //dist
            pathUncModelError.clear();   //points, dist
            pathUncBias.clear();         //points, dist

            // Read in number of points, number of distances, and a boolean
            // (0=false, >1=true) indicating random error is stored (if true).
            int numDistances = input.readInt();
            int numPoints = input.readInt();
            bool usesRandomError = input.readBool();

            // Read in grid id

            gridId = input.readString();

            //valid only if number of points and distances are greater than zero
            if (numPoints > 0)
            {
                if (numDistances > 0)
                {
                    // read in distance bins
                    pathUncDistanceBins.resize(numDistances);
                    for (int i = 0; i < numDistances; i++)
                        pathUncDistanceBins[i] = input.readDouble();

                    // read in crust error for each point
                    pathUncCrustError.resize(numPoints);
                    for (int i = 0; i < numPoints; i++)
                        pathUncCrustError[i] = input.readDouble();

                    // read in random error for each point. Only read if present
                    // (usesRandomError = true)
                    if (usesRandomError) {
                        pathUncRandomError.resize(numDistances);
                        for (int i = 0; i < numDistances; ++i)
                        {
                            pathUncRandomError[i].resize(numPoints);
                            vector<double>& randomError_i = pathUncRandomError[i];
                            for (int j = 0; j < numPoints; j++)
                                randomError_i[j] = input.readDouble();
                        }
                    }

                    // read in model error for all distance bins of all points
                    pathUncModelError.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncModelError[i].resize(numPoints);
                        vector<double>& modelError_i = pathUncModelError[i];
                        for (int j = 0; j < numPoints; j++)
                            modelError_i[j] = input.readDouble();
                    }

                    // read in bias for all distance bins of all points
                    pathUncBias.resize(numDistances);
                    for (int i = 0; i < numDistances; ++i)
                    {
                        pathUncBias[i].resize(numPoints);
                        vector<double>& bias_i = pathUncBias[i];
                        for (int j = 0; j < numPoints; j++)
                            bias_i[j] = input.readDouble();
                    }
                }
            }
        }
        catch (...)
        {
            ostringstream os;
            os << endl << "ERROR in Uncertainty::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }
    }

    void UncertaintyPathDep::writeFile(geotess::IFStreamBinary& output)
    {
        output.writeString(gridId);

        output.writeInt(pathUncDistanceBins.size());
        output.writeInt(pathUncModelError.size());
        output.writeBool(pathUncRandomError.size() > 0);

        for (int i = 0; i < pathUncDistanceBins.size(); i++)
            output.writeDouble(pathUncDistanceBins[i]);

        for (int i = 0; i < pathUncCrustError.size(); i++)
            output.writeDouble(pathUncCrustError[i]);

        for (int i = 0; i < pathUncRandomError.size(); ++i)
        {
            vector<double>& randomError_i = pathUncRandomError[i];
            for (int j = 0; j < randomError_i.size(); j++)
                output.writeDouble(randomError_i[j]);
        }

        for (int i = 0; i < pathUncModelError.size(); ++i)
        {
            vector<double>& modelError_i = pathUncModelError[i];
            for (int j = 0; j < modelError_i.size(); j++)
                output.writeDouble(modelError_i[j]);
        }

        for (int i = 0; i < pathUncBias.size(); ++i)
        {
            vector<double>& bias_i = pathUncBias[i];
            for (int j = 0; j < bias_i.size(); j++)
                output.writeDouble(bias_i[j]);
        }
    }

    //
    // path-dependent travel time uncertainty method
    //
    /**
     * The matrix we will be computing is as follows:
     *                           /                                                                                                              \ /    \
     *                           | mu_11^2 + sigma_11^2 + tau_11^2               tau_12^2                ...               tau_1n^2             | | w1 |
     *                           |                                                                                                              | |    |
     * /                       \ |            tau_21^2                mu_22^2 + sigma_22^2 + tau_22^2    ...               tau_2n^2             | | w2 |
     * | w1    w2    ...    wn | |                                                                                                              | |    |
     * \                       / |               :                                  :                    ':,                  :                 | | :  |
     *                           |                                                                                                              | |    |
     *                           |            tau_n1^2                           tau_n2^2                ...    mu_nn^2 + sigma_nn^2 + tau_nn^2 | | wn |
     *                           \                                                                                                              / \    /
     * where
     * w     = node weight
     * mu    = bias
     * sigma = random error
     * tau   = model error
     */
    double UncertaintyPathDep::getUncertainty(double distance,
        const vector<int>& crustNodeIds, const vector<double>& crustWeights,
        const vector<int>& headWaveNodeIds, const vector<double>& headWaveWeights,
        const vector<vector<int> >& headWaveNodeNeighbors, const bool& calcRandomError,
        bool printDebugInfo) {

        // we're going to be using these counters a few times, so let's go ahead and declare them
        int i, j;


        // get some sizes which will be useful
        int numCrustNodes     = crustNodeIds.size();         // number of crustal nodes that are touched
        int numHeadwaveNodes  = headWaveNodeIds.size();      // number of headwave nodes that are touched
        int numDistanceBins   = pathUncDistanceBins.size();  // the number of distance bins


        // compute the path length. crustWeights and headWaveWeights are given in km, so the sum
        // of all weights is the path length in km. to scale the "weights" to be between [0,1],
        // we'll divide them by the path length when we are using them in the uncertainty computation
        double crustalPathLengthKm = 0.0;
        double headwavePathLengthKm = 0.0;
        double totalPathLengthKm = 0.0;

        for (i = 0; i < numCrustNodes; ++i)
            crustalPathLengthKm += crustWeights[i];
        
        for (i = 0; i < numHeadwaveNodes; ++i)
            headwavePathLengthKm += headWaveWeights[i];
        
        totalPathLengthKm = crustalPathLengthKm + headwavePathLengthKm;


        // compute distance bin weights so that we can interpolate to the correct distance.
        // for example, if we have distance bins of 2, 4, and 6 deg, and distance = 5.8 deg,
        // we select 4 and 6 and compute the weight for each bin. for a distance of 5.8 deg,
        // the weights for 4 and 6 deg will be 0.1 and 0.9, respectively. we also have to
        // deal with the special cases where the distance is either less than the smallest
        // distance bin or larger than the larger distance bin.
        double wd[2];  // initialize distance bin weights
        int  ibin[2];  // initialize the indices to the appropriate distance bins

        // if the distance is less than the smallest distance bin
        if (distance < pathUncDistanceBins[0])
        {

            // we assume that values before the first distance bin are constant and
            // the same as the first distance bin, so we'll assign 1.0 to the second
            // weight and negate the first weight
            wd[0] = 0.0;
            wd[1] = 1.0;

            // we'll also assign both indices to be the first distance bin.
            ibin[0] = ibin[1] = 0;

        }

        // if the distance is larger than the largest distance bin
        else if (distance > pathUncDistanceBins[numDistanceBins-1])
        {

            // we assume the values beyond the largest distance bin are constant, so
            // we'll assign 1.0 to the first weight and negate the second weight
            wd[0] = 1.0;
            wd[1] = 0.0;

            // and both indices will be the last distance bin
            ibin[0] = ibin[1] = numDistanceBins-1;

        }

        // if the distance lies somewhere between the first and last distance bin,
        // we compute the weights for each bin
        else
        {

            // loop through the distance bins and break when we find the last
            // bin that exceeds the distance. this is our rightmost bound and
            // will tell us the two bins that our distance falls between.
            for (i = 0; i < numDistanceBins; ++i)
                if (distance <= pathUncDistanceBins[i])
                    break;

            // note indices to left and right distance bin bounds
            ibin[0] = i-1;
            ibin[1] = i;

            // LINEAR INTERPOLATION IS DEPRECATED, SEE COSINE INTERP BELOW
            // compute the weights for the two distance bins (e.g., for distance
            // bins of 4 and 6 deg, and a distance of 5.8 deg, the weights for
            // 4 and 6 deg will be 0.1 and 0.9, respectively).
            wd[0] = (pathUncDistanceBins[ibin[1]] - distance)
                    / (pathUncDistanceBins[ibin[1]] - pathUncDistanceBins[ibin[0]]);
            // wd[1] = 1 - wd[0];

            // SMOOTH COSINE INTERPOLATION
            // compute the weights for the two distance bins using a sinusoid
            // function that goes smoothly from 0 to 1 (e.g., for distance bins
            // of 4 and 6 deg, and a distance of 5.8 deg, the weights for 4 and
            // 6 deg will be 0.0245 and 0.975, respectively).
            //
            //                  f(t) = (1 - cos(pi*t))/2
            //                  
            //                   1 |             ,,--
            //                     |           ,'
            //                     |          /
            //                     |         :
            //                     |        /
            //                     |      ,'
            //                     | __,,'
            //                   0 +----------------->
            //                     0                1
            wd[0] = (1.0 - cos(PI*wd[0]))/2.0;
            wd[1] = 1.0 - wd[0];
            
        }


        // debug message
        if (printDebugInfo)
        {

            // main information
            cout << endl;
            cout << "---- DEBUG ---\\/\\/---- UncertaintyPathDep::getUncertainty ----\\/\\/--- DEBUG ----" << endl;
            cout << endl;
            cout << "numCrustNodes:     " << numCrustNodes << endl;
            cout << "numHeadwaveNodes:  " << numHeadwaveNodes << endl;
            cout << endl;
            cout << "calcRandomError?:  " << boolalpha << calcRandomError << endl;
            cout << endl;
            cout << "crustalPathLength:   " << fixed << setprecision(2) << setw(7) << crustalPathLengthKm << " km" << endl;
            cout << "headwavePathLength:  " << fixed << setprecision(2) << setw(7) << headwavePathLengthKm << " km" << endl;
            cout << "totalPathLength:     " << fixed << setprecision(2) << setw(7) << totalPathLengthKm << " km" << endl;
            cout << endl;
            cout << "distance:             " << fixed << setprecision(2) << setw(5) << distance << " deg"
                                             << "         (" << distance * 6371. * PI / 180. << " km)" << endl;
            cout << "distanceBins:   " << fixed << setprecision(2)
                                       << setw(5) << pathUncDistanceBins[ibin[0]] << " deg    "
                                       << setw(5) << pathUncDistanceBins[ibin[1]] << " deg" << endl;
            cout << "dbWeights:      " << fixed << setprecision(4) << setw(6) << wd[0] << "       " << setw(6) << wd[1] << endl;
            cout << endl;

            // headwave node information
            for (i = 0; i < numHeadwaveNodes; ++i)
            {

                cout << "HEADWAVE node:  " << setw(5) << headWaveNodeIds[i] << fixed << setprecision(2)
                                           << "            (weight = " << headWaveWeights[i] << " km)" << endl;
                cout << "distance:                " << fixed << setprecision(2)
                                                    << setw(5) << pathUncDistanceBins[ibin[0]] << " deg     "
                                                    << setw(5) << pathUncDistanceBins[ibin[1]] << " deg     "
                                                    << setw(5) << distance << " deg" << endl;

                cout << "modelError:              " << scientific << setprecision(4)
                     << setw(10) << pathUncModelError[ibin[0]][headWaveNodeIds[i]] << "    "
                     << setw(10) << pathUncModelError[ibin[1]][headWaveNodeIds[i]] << "    "
                     << setw(10) << wd[0]*pathUncModelError[ibin[0]][headWaveNodeIds[i]]
                                   + wd[1]*pathUncModelError[ibin[1]][headWaveNodeIds[i]] << endl;

                cout << "bias:                    " << scientific << setprecision(4)
                     << setw(10) << pathUncBias[ibin[0]][headWaveNodeIds[i]] << "    "
                     << setw(10) << pathUncBias[ibin[1]][headWaveNodeIds[i]] << "    "
                     << setw(10) << wd[0]*pathUncBias[ibin[0]][headWaveNodeIds[i]]
                                   + wd[1]*pathUncBias[ibin[1]][headWaveNodeIds[i]] << endl;

                if (calcRandomError)
                    cout << "randomError:             " << scientific << setprecision(4)
                         << setw(10) << pathUncRandomError[ibin[0]][headWaveNodeIds[i]] << "    "
                         << setw(10) << pathUncRandomError[ibin[1]][headWaveNodeIds[i]] << "    "
                         << setw(10) << wd[0]*pathUncRandomError[ibin[0]][headWaveNodeIds[i]]
                                       + wd[1]*pathUncRandomError[ibin[1]][headWaveNodeIds[i]] << endl;

               cout << endl;

            }

            // crustal node information
            for (i = 0; i < numCrustNodes; ++i)
            {

                cout << "CRUSTAL node:   " << setw(5) << crustNodeIds[i] << fixed << setprecision(2)
                                           << "            (weight = " << crustWeights[i] << " km)" << endl;
                cout << "distance:                " << fixed << setprecision(2)
                                                    << setw(5) << pathUncDistanceBins[ibin[0]] << " deg     "
                                                    << setw(5) << pathUncDistanceBins[ibin[1]] << " deg     "
                                                    << setw(5) << distance << " deg" << endl;

                cout << "crustError:              " << scientific << setprecision(4)
                     << setw(10) << pathUncCrustError[crustNodeIds[i]] << "    "
                     << setw(10) << pathUncCrustError[crustNodeIds[i]] << "    "
                     << setw(10) << wd[0]*pathUncCrustError[crustNodeIds[i]]
                                   + wd[1]*pathUncCrustError[crustNodeIds[i]] << endl;

                cout << endl;

            }

            // matrix header
            cout << "HEADWAVE matrix:" << endl << "    node";
            for (i = 0; i < numHeadwaveNodes; ++i)
                cout << "       " << setw(5) << headWaveNodeIds[i];
            cout << endl;

        }


        //
        // now we can compute headwave uncertainty
        //
        bool isNodeNeighbor;  // whether or not the jth node is a neighbor of the ith node
        double ttUnc_headwave = 0.0;  // initialize travel time uncertainty in the headwave
        double modelError_i, modelError_j, tau_ij,  // model error
               bias_i,                      mu_ii,  // bias
               randomError_i,            sigma_ii,  // random error
               w_i, w_j,                            // node weights
               val_ij;                              // the value of the i,jth matrix element

        // loop through "rows"
        for (i = 0; i < numHeadwaveNodes; ++i)
        {

            // interpolate values to proper distance
            modelError_i = wd[0]*pathUncModelError[ibin[0]][headWaveNodeIds[i]] + wd[1]*pathUncModelError[ibin[1]][headWaveNodeIds[i]];
            bias_i       = wd[0]*pathUncBias[ibin[0]][headWaveNodeIds[i]] + wd[1]*pathUncBias[ibin[1]][headWaveNodeIds[i]];
            if (calcRandomError)
                randomError_i = wd[0]*pathUncRandomError[ibin[0]][headWaveNodeIds[i]] + wd[1]*pathUncRandomError[ibin[1]][headWaveNodeIds[i]];
            else
                randomError_i = 0.0;

            // compute matrix values (no averaging is necessary between the 
            // ith and jth elements of bias and randomError because we only
            // compute these on the diagonals, where i = j)
            w_i      = headWaveWeights[i];
            mu_ii    = bias_i;
            sigma_ii = randomError_i;

            // DEBUG -- print row name
            if (printDebugInfo)
                cout << "    " << fixed << setw(5) << headWaveNodeIds[i];


            // loop through "columns"
            for (j = 0; j < numHeadwaveNodes; ++j)
            {

                // start off with the value of this matrix element as zero
                val_ij = 0.0;

                // interpolate values to proper distance
                modelError_j = wd[0]*pathUncModelError[ibin[0]][headWaveNodeIds[j]] + wd[1]*pathUncModelError[ibin[1]][headWaveNodeIds[j]];

                // compute matrix values
                w_j    = headWaveWeights[j];
                tau_ij = (modelError_i + modelError_j)/2;

                // add to this element's value
                val_ij += w_i * w_j * tau_ij;

                // if we're on the matrix diagonal, add bias and random error
                if (i == j)
                    val_ij += w_i*w_i * (mu_ii + sigma_ii);

                // figure if the jth node is a neighbor to the ith node
                if (i == j)
                    isNodeNeighbor = true;  // elements are defined as neighboring themselves
                else
                    isNodeNeighbor = (find(headWaveNodeNeighbors[i].begin(), headWaveNodeNeighbors[i].end(),
                        headWaveNodeIds[j]) != headWaveNodeNeighbors[i].end());

                // if the jth node doesn't neighbor the ith node, set this matrix element to zero
                if (!isNodeNeighbor)
                    val_ij = 0.0;

                // add this element's value to the total headwave uncertainty
                ttUnc_headwave += val_ij;

                // DEBUG -- print matrix value
                if (printDebugInfo)
                    cout << "  " << scientific << setprecision(4) << setw(10) << val_ij;

            }

            // DEBUG -- end line
            if (printDebugInfo)
                cout << endl;

        }


        //
        // and now crustal uncertainty
        //
        double ttUnc_crust = 0.0;  // initialize travel time uncertainty in the crust
        double crustError;  // crustal model error
        for (i = 0; i < numCrustNodes; ++i)  // loop through each crustal node
        {

            // ordinarily we'd do this
            //   crustError_i = wd[0]*pathUncCrustError[ibin[0]][i] + wd[1]*pathUncCrustError[ibin[1]][i];
            //   crustError_j = wd[0]*pathUncCrustError[ibin[0]][j] + wd[1]*pathUncCrustError[ibin[1]][j];
            //   crustError = (crustError_i + crustError_j) / 2;
            // but since we're only computing diagonals, crustError_i == crustError_j, so
            //   crustError = crustError_i;
            // additionally, crustError is 1d, so there are no distance bins (though we still use the weights), so
            //   crustError = wd[0]*pathUncCrustError[i] + wd[1]*pathUncCrustError[i];
            // or
            crustError = (wd[0] + wd[1])*pathUncCrustError[crustNodeIds[i]];  // this is a percent

            // add to uncertainty (crustError is in percent of travel time, and crustWeights is in secs)
            ttUnc_crust += crustWeights[i] * crustError;  // this is travel time * percent

        }

        // square ttUnc_crust so it's in units of s^2
        ttUnc_crust *= ttUnc_crust;
        

        // DEBUG -- print footer
        if (printDebugInfo)
        {
            cout << endl;
            cout << "Traveltime Uncertainty" << endl;
            cout << "-------------------------" << endl;
            cout << "Crustal:     " << fixed << setprecision(4) << ttUnc_crust << " s^2" << endl;
            cout << "Headwave:    " << fixed << setprecision(4) << ttUnc_headwave << " s^2" << endl;
            cout << "Total:       " << fixed << setprecision(4) << ttUnc_crust + ttUnc_headwave << " s^2 (crust + mantle)" << endl;
            cout << "Total:       " << fixed << setprecision(4) << sqrt(ttUnc_crust + ttUnc_headwave) << " s   (crust + mantle)" << endl;
            cout << endl;
            cout << "---- DEBUG ---/\\/\\---- UncertaintyPathDep::getUncertainty ----/\\/\\--- DEBUG ----" << endl;
        }


        //
        // finally, return the path-dependent uncertainty
        //
        double ttUnc = sqrt(ttUnc_headwave + ttUnc_crust);
        return ttUnc;

    }

    string UncertaintyPathDep::toStringTable() {
        return "";
    }

    string UncertaintyPathDep::toStringFile() {
        return "";
    }

}
