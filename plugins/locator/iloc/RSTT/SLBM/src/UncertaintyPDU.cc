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

#include "UncertaintyPDU.h"
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
    UncertaintyPDU::UncertaintyPDU() : fname("not_specified"), phaseNum(-1)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM.
    //
    // **********************************************UncertaintyPDU*************
    UncertaintyPDU::UncertaintyPDU(int phase)
        : fname("not_specified"), phaseNum(phase)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM.
    //
    // *****************************************************************************
    UncertaintyPDU::UncertaintyPDU(const string& phase)
        : fname("not_specified"), phaseNum(getPhase(phase))
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM. Phase is
    // used to construct file name which is loaded from the input path.
    //
    // *****************************************************************************
    UncertaintyPDU::UncertaintyPDU(string modelPath, const string& phase)
        : fname("not_specified"), phaseNum(getPhase(phase))
    {
        fname = "UncertaintyPDU_" + phase + ".txt";
        fname = CPPUtils::insertPathSeparator(modelPath, fname);

        readFile(fname);
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Parameterized Path Dependent Uncertainty Constructor used by SLBM. Phase is
    // used to construct file name which is loaded from the input path.
    //
    // *****************************************************************************
    UncertaintyPDU::UncertaintyPDU(string modelPath, int phase)
        : fname("not_specified"), phaseNum(phase),
          gridId("????????????????????????????????")
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
    UncertaintyPDU::UncertaintyPDU(const UncertaintyPDU& u) :
        fname(u.fname),
        phaseNum(u.phaseNum),
        gridId(u.gridId),
        pathUncCrustError(u.pathUncCrustError),
        pathUncDistanceBins(u.pathUncDistanceBins),
        pathUncRandomError(u.pathUncRandomError),
        pathUncModelError(u.pathUncModelError),
        pathUncBias(u.pathUncBias)
    {
    }

    // **** _FUNCTION DESCRIPTION_ *************************************************
    //
    // Uncertainty Destructor
    //
    // *****************************************************************************
    UncertaintyPDU::~UncertaintyPDU()
    {
        fname = "not_specified";
        phaseNum = -1;
        gridId = "????????????????????????????????";
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
    UncertaintyPDU& UncertaintyPDU::operator=(const UncertaintyPDU& u)
    {
        phaseNum = u.phaseNum;
        gridId = u.gridId;
        pathUncCrustError = u.pathUncCrustError;
        pathUncRandomError = u.pathUncRandomError;
        pathUncDistanceBins = u.pathUncDistanceBins;
        pathUncModelError = u.pathUncModelError;
        pathUncBias = u.pathUncBias;

        return *this;
    }

    bool UncertaintyPDU::operator==(const UncertaintyPDU& other) const
    {
        if (this->phaseNum != other.phaseNum) return false;
        if (this->gridId != other.gridId) return false;
        if (this->pathUncCrustError.size() != other.pathUncCrustError.size()) return false;
        if (this->pathUncRandomError.size() != other.pathUncRandomError.size()) return false;
        if (this->pathUncDistanceBins.size() != other.pathUncDistanceBins.size()) return false;
        if (this->pathUncModelError.size() != other.pathUncModelError.size()) return false;
        if (this->pathUncBias.size() != other.pathUncBias.size()) return false;

        // NOTE: lots of values of type double but they were stored in files as
        // floats.  So only compare with 6 digits of precision.
        for (int i = 0; i < (int)pathUncDistanceBins.size(); ++i)
            if (this->pathUncDistanceBins[i] != other.pathUncDistanceBins[i])
                return false;

        for (int i = 0; i < (int)pathUncCrustError.size(); ++i)
            if (abs(1.-this->pathUncCrustError[i]/other.pathUncCrustError[i]) > 1e-6)
                return false;

        for (int i = 0; i < (int)pathUncRandomError.size(); ++i)
        {
            const vector<double>& thisRandomError_i = this->pathUncRandomError[i];
            const vector<double>& otherRandomError_i = other.pathUncRandomError[i];
            if (thisRandomError_i.size() != otherRandomError_i.size()) return false;
            for (int j = 0; j < (int)thisRandomError_i.size(); ++j)
                if (abs(1.-thisRandomError_i[j]/otherRandomError_i[j]) > 1e-6)
                    return false;
        }

        for (int i = 0; i < (int)pathUncModelError.size(); ++i)
        {
            const vector<double>& thisModelError_i = this->pathUncModelError[i];
            const vector<double>& otherModelError_i = other.pathUncModelError[i];
            if (thisModelError_i.size() != otherModelError_i.size()) return false;
            for (int j = 0; j < (int)thisModelError_i.size(); ++j)
                if (abs(1.-thisModelError_i[j]/otherModelError_i[j]) > 1e-6)
                    return false;
        }

       for (int i = 0; i < (int)pathUncBias.size(); ++i)
        {
            const vector<double>& thisBias_i = this->pathUncBias[i];
            const vector<double>& otherBias_i = other.pathUncBias[i];
            if (thisBias_i.size() != otherBias_i.size()) return false;
            for (int j = 0; j < (int)thisBias_i.size(); ++j)
                if (abs(1.-thisBias_i[j]/otherBias_i[j]) > 1e-6)
                    return false;
        }

        return true;
    }

    UncertaintyPDU* UncertaintyPDU::getUncertainty(ifstream& input, int phase)
    {
        UncertaintyPDU* u = new UncertaintyPDU(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPDU* UncertaintyPDU::getUncertainty(ifstream& input, const string& phase)
    {
        UncertaintyPDU* u = new UncertaintyPDU(phase);
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPDU* UncertaintyPDU::getUncertainty(geotess::IFStreamAscii& input)
    {
        UncertaintyPDU* u = new UncertaintyPDU();
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPDU* UncertaintyPDU::getUncertainty(geotess::IFStreamBinary& input)
    {
        UncertaintyPDU* u = new UncertaintyPDU();
        u->readFile(input);
        if (u->getPathUncDistanceBins().size() == 0)
        {
            delete u;
            u = NULL;
        }
        return u;
    }

    UncertaintyPDU* UncertaintyPDU::getUncertainty(const string& modelPath, int phase)
    {
        string fname = "UncertaintyPDU_" + getPhase(phase) + ".txt";
        fname = CPPUtils::insertPathSeparator(modelPath, fname);

        // ifstream fin;
        // fin.open(fname.c_str());
        // if (fin.fail() || !fin.is_open())
        //     return NULL;

        // UncertaintyPDU* u = getUncertainty(fin, phase);
        // fin.close();
        IFStreamAscii fin;
        fin.openForRead(fname);
        UncertaintyPDU* u = getUncertainty(fin);
        fin.close();

        return u;
    }

    void UncertaintyPDU::readFile(const string& filename)
    {
        // ifstream fin;

        // //open the file.  Return an error if cannot open the file.
        // fin.open(filename.c_str());
        // if (fin.fail() || !fin.is_open())
        // {
        //     ostringstream os;
        //     os << endl << "ERROR in UncertaintyPIU::readFile" << endl
        //         << "Could not open file " << filename << endl
        //         << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        //     throw SLBMException(os.str(), 115);
        // }

        // readFile(fin);
        // fin.close();
    }

    void UncertaintyPDU::readFile(ifstream& fin)
    {
        // pathUncDistanceBins.clear(); // nBins
        // pathUncCrustError.clear();   // nVertices
        // pathUncRandomError.clear();  // nBins x nVertices (optional)
        // pathUncModelError.clear();   // nBins x nVertices
        // pathUncBias.clear();         // nBins x nVertices

        // string line;
        // string comment;

        // try
        // {
        //     // comment line
        //     getline(fin, line);
        //     if (line.rfind("# RSTT Path Dependent Uncertainty", 0) != 0)
        //         throw SLBMException("Expected file to start with '# RSTT Path Dependent Uncertainty' but found: " + line, 115);

        //     // fileFormatVersion
        //     getline(fin, line);
        //     cout << __FILE__ << ":" << __LINE__ << " '" << line << "'" << endl;
        //     if (line.rfind("FileFormatVersion ", 0) != 0)
        //         throw SLBMException("Expected to find 'FileFormatVersion int' but found: " + line, 115);
        //     int fileFormatVersion;
        //     sscanf(line.c_str(), "%*s %d", &fileFormatVersion);

        //     // comment line
        //     getline(fin, line);
        //     if (line.rfind("# Properties", 0) != 0)
        //         throw SLBMException("Expected file to start with '# Properties' but found: " + line, 115);
        // }
        // catch (SLBMException e)
        // {
        //     ostringstream os;
        //     os << endl << "ERROR in UncertaintyPDU::readFile" << endl
        //         << e.emessage << endl
        //         << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        //     throw SLBMException(os.str(), e.ecode);
        // }
        /*
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
            os << endl << "ERROR in UncertaintyPDU::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }
        */

    }

    void UncertaintyPDU::writeFile(geotess::IFStreamAscii& output)
    {
        int nDistanceBins = pathUncDistanceBins.size();
        int nVertices = pathUncCrustError.size();
        bool includeRandomError = pathUncRandomError.size() > 0;

        output.writeStringNL("# RSTT Path Dependent Uncertainty Parameters");
        output.writeStringNL("FileFormatVersion 1");
        output.writeStringNL("# Properties: (phase, gridId, nDistanceBins, nVertices, includeRandomError are required; others optional)");

        // update properties that may have changed.  Other properties
        // are simply copied from the input file.
        properties["phase"] = getPhaseStr();
        properties["gridId"] = gridId;
        properties["nDistanceBins"] = CPPUtils::itos(nDistanceBins);
        properties["nVertices"] = CPPUtils::itos(nVertices);
        properties["includeRandomError"] = includeRandomError ? "true" : "false";

        for (int i=0; i<keys.size(); ++i)
            output.writeStringNL(keys[i]+" = "+
                    CPPUtils::stringReplaceAll(CPPUtils::NEWLINE, "<NEWLINE>", properties[keys[i]]));

        output.writeNL();

        ostringstream os;
        os << fixed << setprecision(2);

        os << "# Distance Bins" << endl;
        for (int i = 0; i < nDistanceBins; i++)
            os << " " << pathUncDistanceBins[i];
        os << endl;
        output.writeStringNL(os.str());
        os.str("");
        os.clear();

        os << scientific << setprecision(7);

        os << "# Crustal Error" << endl;
        for (int i = 0; i < nVertices; i++)
            os << pathUncCrustError[i] << endl;
        output.writeStringNL(os.str());
        os.str("");
        os.clear();


        if (includeRandomError)
        {
            os << "# Random Error" << endl;
            for (int j=0; j<nVertices; ++j)
            {
                for (int i = 0; i < nDistanceBins; i++)
                    os << " " << pathUncRandomError[i][j];
                os << endl;
            }
            output.writeStringNL(os.str());
            os.str("");
            os.clear();
        }

        os << "# Model Error" << endl;
        for (int j=0; j<nVertices; ++j)
        {
            for (int i = 0; i < nDistanceBins; i++)
                os << " " << pathUncModelError[i][j];
            os << endl;
        }
        output.writeStringNL(os.str());
        os.str("");
        os.clear();

        os << "# Bias" << endl;
        for (int j=0; j<nVertices; ++j)
        {
            for (int i = 0; i < nDistanceBins; i++)
                os << " " << pathUncBias[i][j];
            os << endl;
        }
        output.writeString(os.str());
        os.str("");
        os.clear();
    }

    void UncertaintyPDU::readFile(geotess::IFStreamAscii& input)
    {
        try
        {
            pathUncDistanceBins.clear(); // nBins
            pathUncCrustError.clear();   // nVertices
            pathUncRandomError.clear();  // nBins x nVertices (optional)
            pathUncModelError.clear();   // nBins x nVertices
            pathUncBias.clear();         // nBins x nVertices

            string line, key, value;
            input.getLine(line);
            input.getLine(line);

            // start reading properties
            properties.clear();
            keys.clear();

            input.readLine(line);
            while (line != "")
            {
                int pos = line.find("=");
                if (pos !=std::string::npos)
                {
                    key = CPPUtils::trim(line.substr(0, pos));
                    value = CPPUtils::stringReplaceAll("<NEWLINE>", CPPUtils::NEWLINE,
                            CPPUtils::trim(line.substr(pos+1, line.length())));
                    keys.push_back(key);
                    properties[key] = value;
                }
                input.getline(line);
            }

            phaseNum = UncertaintyPIU::getPhase(properties["phase"]);

            if (phaseNum < 0 || phaseNum > 3)
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "Phase " << line << " is not a recognized phase." << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 115);
            }

            gridId = properties["gridId"];

            if (gridId.length() != 32 || gridId.find("?", 0) != string::npos)
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "GridId " << gridId << " is not a valid gridId." << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 115);
            }

            int nDistanceBins = geotess::CPPUtils::stoi(properties["nDistanceBins"]);
            int nVertices = geotess::CPPUtils::stoi(properties["nVertices"]);
            bool usesRandomError = properties["includeRandomError"] == "true";

            //valid only if number of points and distances are greater than zero
            if (nVertices > 0)
            {
                if (nDistanceBins > 0)
                {
                    // read in distance bins
                    input.getline(line);
                    while (line != "# Distance Bins")
                        input.getline(line);

                    pathUncDistanceBins.resize(nDistanceBins);
                    for (int i = 0; i < nDistanceBins; i++)
                        pathUncDistanceBins[i] = input.readDouble();

                    input.getline(line);
                    while (line != "# Crustal Error")
                        input.getline(line);

                    // read in crust error for each point
                    pathUncCrustError.resize(nVertices);
                    for (int i = 0; i < nVertices; i++)
                        pathUncCrustError[i] = input.readDouble();

                    if (usesRandomError) {

                        input.getline(line);
                        while (line != "# Random Error")
                            input.getline(line);

                        pathUncRandomError.resize(nDistanceBins, vector<double>(nVertices));
                        for (int j = 0; j < nVertices; j++)
                            for (int i = 0; i < nDistanceBins; ++i)
                                pathUncRandomError[i][j] = input.readDouble();
                    }

                    input.getline(line);
                    while (line != "# Model Error")
                        input.getline(line);

                    pathUncModelError.resize(nDistanceBins, vector<double>(nVertices));
                    for (int j = 0; j < nVertices; j++)
                        for (int i = 0; i < nDistanceBins; ++i)
                            pathUncModelError[i][j] = input.readDouble();

                    input.getline(line);
                    while (line != "# Bias")
                        input.getline(line);

                    pathUncBias.resize(nDistanceBins, vector<double>(nVertices));
                    for (int j = 0; j < nVertices; j++)
                        for (int i = 0; i < nDistanceBins; ++i)
                            pathUncBias[i][j] = input.readDouble();

                }
            }
        }
        catch (...)
        {
            ostringstream os;
            os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }
    }

    void UncertaintyPDU::readFile(geotess::IFStreamBinary& input)
    {
        try
        {
            pathUncDistanceBins.clear(); // nBins
            pathUncCrustError.clear();   // nVertices
            pathUncRandomError.clear();  // nBins x nVertices (optional)
            pathUncModelError.clear();   // nBins x nVertices
            pathUncBias.clear();         // nBins x nVertices

            // start reading properties
            properties.clear();
            keys.clear();

            string classname;
            input.readCharArray(classname, 14);

            if (classname != "UncertaintyPDU")
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "Expected first 14 characters to be UncertaintyPDU but found " << endl
                        << "'" << classname << "'" << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                cout << os.str() << endl;
                throw SLBMException(os.str(), 115);
            }

            int fileFormat = input.readInt();

            if (fileFormat != 1)
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "Expected fileFormat == 1 but found " << fileFormat << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 115);
            }

            int nProperties = input.readInt();

            string key, value;

            for (int i=0; i<nProperties; ++i)
            {
                key = input.readString();
                value = CPPUtils::stringReplaceAll("<NEWLINE>", CPPUtils::NEWLINE,
                        input.readString());
                keys.push_back(key);
                properties[key] = value;
            }

            phaseNum = UncertaintyPIU::getPhase(properties["phase"]);

            if (phaseNum < 0 || phaseNum > 3)
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "Phase " << phaseNum << " is not a recognized phase." << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 115);
            }

            gridId = properties["gridId"];

            if (gridId.length() != 32 || gridId.find("?", 0) != string::npos)
            {
                ostringstream os;
                os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                        << "GridId " << gridId << " is not a valid gridId." << endl
                        << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
                throw SLBMException(os.str(), 115);
            }

            // Read in number of points, number of distances, and a boolean
            // (0=false, >1=true) indicating random error is stored (if true).
            int nDistanceBins = CPPUtils::stoi(properties["nDistanceBins"]);
            int nVertices = CPPUtils::stoi(properties["nVertices"]);
            int includeRandomError = properties["includeRandomError"] == "true";

            //valid only if number of points and distances are greater than zero
            if (nDistanceBins > 0)
            {
                if (nVertices > 0)
                {
                    // read in distance bins
                    pathUncDistanceBins.resize(nDistanceBins);
                    for (int i = 0; i < nDistanceBins; i++)
                        pathUncDistanceBins[i] = input.readFloat();

                    // read in crust error for each point
                    pathUncCrustError.resize(nVertices);
                    for (int i = 0; i < nVertices; i++)
                        pathUncCrustError[i] = input.readFloat();

                    // read in random error for each point. Only read if present
                    // (usesRandomError = true)
                    if (includeRandomError) {
                        pathUncRandomError.resize(nDistanceBins);
                        for (int i = 0; i < nDistanceBins; ++i)
                        {
                            pathUncRandomError[i].resize(nVertices);
                            vector<double>& randomError_i = pathUncRandomError[i];
                            for (int j = 0; j < nVertices; j++)
                                randomError_i[j] = input.readFloat();
                        }
                    }

                    // read in model error for all distance bins of all points
                    pathUncModelError.resize(nDistanceBins);
                    for (int i = 0; i < nDistanceBins; ++i)
                    {
                        pathUncModelError[i].resize(nVertices);
                        vector<double>& modelError_i = pathUncModelError[i];
                        for (int j = 0; j < nVertices; j++)
                            modelError_i[j] = input.readFloat();
                    }

                    // read in bias for all distance bins of all points
                    pathUncBias.resize(nDistanceBins);
                    for (int i = 0; i < nDistanceBins; ++i)
                    {
                        pathUncBias[i].resize(nVertices);
                        vector<double>& bias_i = pathUncBias[i];
                        for (int j = 0; j < nVertices; j++)
                            bias_i[j] = input.readFloat();
                    }
                }
            }
        }
        catch (...)
        {
            ostringstream os;
            os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                << "Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 115);
        }
    }

    void UncertaintyPDU::writeFile(geotess::IFStreamBinary& output)
    {
        output.writeCharArray("UncertaintyPDU", 14);
        output.writeInt(1); // file format version number

        int nDistanceBins = pathUncDistanceBins.size();
        int nVertices = pathUncCrustError.size();
        bool includeRandomError = pathUncRandomError.size() > 0;

        // update properties that may have changed.  Other properties
        // are simply copied from the input file.
        properties["phase"] = getPhaseStr();
        properties["gridId"] = gridId;
        properties["nDistanceBins"] = CPPUtils::itos(nDistanceBins);
        properties["nVertices"] = CPPUtils::itos(nVertices);
        properties["includeRandomError"] = includeRandomError ? "true" : "false";

        output.writeInt(keys.size());
        for (int i=0; i<keys.size(); ++i)
        {
            output.writeString(keys[i]);
            output.writeString(CPPUtils::stringReplaceAll(CPPUtils::NEWLINE, "<NEWLINE>",
                    properties[keys[i]]));
        }

        for (int i = 0; i < nDistanceBins; ++i)
            output.writeFloat(pathUncDistanceBins[i]);

        for (int i = 0; i < nVertices; ++i)
            output.writeFloat(pathUncCrustError[i]);

        if (includeRandomError) {
            for (int i = 0; i < nDistanceBins; ++i)
                for (int j = 0; j < nVertices; ++j)
                    output.writeFloat(pathUncRandomError[i][j]);
        }

        for (int i = 0; i < nDistanceBins; ++i)
            for (int j = 0; j < nVertices; ++j)
                output.writeFloat(pathUncModelError[i][j]);

        for (int i = 0; i < nDistanceBins; ++i)
            for (int j = 0; j < nVertices; ++j)
                output.writeFloat(pathUncBias[i][j]);

    }

    void UncertaintyPDU::writeFile(const string& directoryName)
    {
        // make a filename
        string fname = "UncertaintyPDU_" + getPhase(phaseNum) + ".txt";
        string filename = CPPUtils::insertPathSeparator(directoryName, fname);

        IFStreamAscii output;
        output.openForWrite(filename);
        writeFile(output);
        output.close();

        /*
        // code based off of GeoTessExplorer UncertaintyPDU.writeFileAscii()

        // make a filename
        string fname = "UncertaintyPDU_" + getPhase(phaseNum) + ".txt";
        string filename = CPPUtils::insertPathSeparator(directoryName, fname);

        // get
        int nDistanceBins = pathUncDistanceBins.size();
        int nVertices = pathUncCrustError.size();
        bool includeRandomError = pathUncRandomError.size() > 0;

        // update properties that may have changed.  Other properties
        // are simply copied from the input file.
        properties["phase"] = getPhaseStr();
        properties["gridId"] = gridId;
        properties["nDistanceBins"] = CPPUtils::itos(nDistanceBins);
        properties["nVertices"] = CPPUtils::itos(nVertices);
        properties["includeRandomError"] = includeRandomError ? "true" : "false";

        // create and open a text file
        ofstream os(filename.c_str());

        // First line of the file must be "# RSTT Path Dependent Uncertainty".
        os << "# RSTT Path Dependent Uncertainty" << endl;

        // write the fileFormatVersion.  The currently supported version is 1 but
        // could change in the future if it becomes necessary to add additional information.
        os << "FileFormatVersion 1" << endl;

        // write out required and optional properties.  Required properties will be 
        // loaded here and may be modified if this model is later written to output.
        // Optional properties will not be modified by this code and will be output 
        // to new files and by the toString function.
        os << "# Properties: (phase, gridId, nDistanceBins, nVertices, includeRandomError are required; others optional)" << endl;

        // os << keys.size() << endl;
        for (int i=0; i<keys.size(); ++i)
        {
            os << keys[i] << " = " << properties[keys[i]] << endl;
        }
        os << endl;

        // write distance binds
        os << "# Distance Bins" << endl;
        for (int i = 0; i < nDistanceBins; ++i)
        {
            os << setw(5) << setprecision(2) << fixed << pathUncDistanceBins[i];
            os << (i == nDistanceBins - 1 ? "\n" : " ");
        }
        os << endl;

        // write crustal error
        os << "# Crustal Error" << endl;
        for (int i = 0; i < nVertices; ++i)
            os << " " << setw(13) << setprecision(7) << scientific << pathUncCrustError[i] << endl;
        os << endl;

        // write random error
        if (includeRandomError)
        {
            os <<  "# Random Error" << endl;
            for (int j = 0; j < nVertices; ++j)                
                for (int i = 0; i < nDistanceBins; ++i)
                {
                    os << " " << setw(13) << setprecision(7) << scientific << pathUncRandomError[i][j];
                    os << (i == nDistanceBins - 1 ? "\n" : " ");
                }
            os << endl;
        }

        // write model error
        os << "# Model Error" << endl;
        for (int j = 0; j < nVertices; ++j)
            for (int i = 0; i < nDistanceBins; ++i)
            {
                os << " " << setw(13) << setprecision(7) << scientific << pathUncModelError[i][j];
                os << (i == nDistanceBins - 1 ? "\n" : " ");
            }
        os << endl;

        // bias
        os << "# Bias" << endl;
        for (int j = 0; j < nVertices; ++j)
            for (int i = 0; i < nDistanceBins; ++i)
            {
                os << " " << setw(13) << setprecision(7) << scientific << pathUncBias[i][j];
                os << (i == nDistanceBins - 1 ? "\n" : " ");
            }

        // close the file
        os.close();
        */
    }

    string UncertaintyPDU::toString()
    {
        ostringstream os;
        for (int i=0; i<keys.size(); ++i)
            os << keys[i] << " = " << properties[keys[i]] << endl;

        return os.str();
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
    double UncertaintyPDU::getUncertainty(double distance,
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
            cout << "---- DEBUG ---\\/\\/---- UncertaintyPDU::getUncertainty ----\\/\\/--- DEBUG ----" << endl;
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
            cout << "---- DEBUG ---/\\/\\---- UncertaintyPDU::getUncertainty ----/\\/\\--- DEBUG ----" << endl;
        }


        //
        // finally, return the path-dependent uncertainty
        //
        double ttUnc = sqrt(ttUnc_headwave + ttUnc_crust);
        return ttUnc;

    }

    string UncertaintyPDU::toStringTable() {
        return "";
    }

    string UncertaintyPDU::toStringFile() {
        return "";
    }

}
