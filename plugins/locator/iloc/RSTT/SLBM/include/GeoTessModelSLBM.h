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

#ifndef GEOTESSMODELSLBM_H_
#define GEOTESSMODELSLBM_H_

// **** _SYSTEM INCLUDES_ ******************************************************

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <map>

// use standard library objects

// **** _LOCAL INCLUDES_ *******************************************************

#include "SLBMGlobals.h"
#include "CPPGlobals.h"
#include "CPPUtils.h"
#include "GeoTessUtils.h"
#include "GeoTessException.h"
#include "GeoTessModel.h"
#include "UncertaintyPDU.h"
#include "UncertaintyPIU.h"
#include "SLBMException.h"

using namespace geotess;
using namespace std;

// **** _BEGIN GEOTESS NAMESPACE_ **********************************************

namespace slbm
{

// **** _FORWARD REFERENCES_ ***************************************************


// **** _CLASS DEFINITION_ *****************************************************

/**
 * This is an SLBM extension of GeoTessModel for use by SLBM.
 * Specific capabilities, beyond those of the base class, are management of
 * the average mantle P and S velocity values required by SLBM and
 * management of Uncertainty information for phases Pn, Sn, Pg and Lg
 * for attributes TT, SH and AZ.
 */
class SLBM_EXP_IMP GeoTessModelSLBM : public GeoTessModel
{
private:

    /**
     * The average P and S velocity of the upper mantle.
     */
    double averageMantleVelocity[2];

    /**
     * A 2D array of Uncertainty objects.  The array dimensions are
     * nPhases x nAttriubtes where nPhases = 4 (Pn, Sn, Pg, Lg) and
     * nAttributes is 3 (TT, SH, AZ).
     */
    vector<vector<UncertaintyPIU*> > piu;

    /**
     * Path dependent uncertainty for each supported phase.
     */
    vector<UncertaintyPDU*> pdu;

    /**
     * RSTT file format version
     */
    int fileFormatVer;

    void init();

    void checkMiddleCrustLayers();

protected:

    /**
     * Load a model (3D grid and data) from an ascii File.
     * <p>
     * The format of the file is: <br>
     * int fileFormatVersion (currently only recognizes 1). <br>
     * String gridFile: either *, or relative path to gridFile. <br>
     * int nVertices, nLayers, nAttributes, dataType(DOUBLE or FLOAT). <br>
     * int[] tessellations = new int[nLayers]; <br>
     * Profile[nVertices][nLayers]: data
     *
     * @param input ascii stream that provides input
     * @param inputDirectory the directory where the model file resides
     * @param relGridFilePath the relative path from the directory where
     * the model file resides to the directory where the grid file resides.
     * @throws GeoTessException
     */
    virtual void loadModelAscii(IFStreamAscii& input, const string& inputDirectory,
                                const string& relGridFilePath);

    /**
     * Load a model (3D grid and data) from a binary File.
     * <p>
     * The format of the file is: <br>
     * int fileFormatVersion (currently only recognizes 1). <br>
     * String gridFile: either *, or relative path to gridFile. <br>
     * int nVertices, nLayers, nAttributes, dataType(DOUBLE or FLOAT). <br>
     * int[] tessellations = new int[nLayers]; <br>
     * Profile[nVertices][nLayers]: data
     * @param input binary stream that provides input
     * @param inputDirectory the directory where the model file resides
     * @param relGridFilePath the relative path from the directory where
     * the model file resides to the directory where the grid file resides.
     * @throws GeoTessException
     */
    virtual void loadModelBinary(IFStreamBinary& input, const string& inputDirectory,
                                const string& relGridFilePath);

    /**
     * Write the model currently in memory to an ascii file. A model can be stored with the data and
     * grid in the same or separate files. This method will write the data from the 3D model to the
     * specified outputfile. If the supplied gridFileName is the single character "*", then the grid
     * information is written to the same file as the data. If the gridFileName is anything else, it
     * is assumed to be the relative path from the data file to an existing file where the grid
     * information is stored. In the latter case, the grid information is not actually written to
     * the specified file; all that happens is that the relative path is stored in the data file.
     */
    virtual void writeModelAscii(IFStreamAscii& output, const string& gridFileName);

    /**
     * Write the model currently in memory to a binary file. A model can be stored with the data and
     * grid in the same or separate files. This method will write the data from the 3D model to the
     * specified outputfile. If the supplied gridFileName is the single character "*", then the grid
     * information is written to the same file as the data. If the gridFileName is anything else, it
     * is assumed to be the relative path from the data file to an existing file where the grid
     * information is stored. In the latter case, the grid information is not actually written to
     * the specified file; all that happens is that the relative path is stored in the data file.
     */
    virtual void writeModelBinary(IFStreamBinary& output, const string& gridFileName);

public:

    /**
     * Default constructor.
     */
    GeoTessModelSLBM();

    /**
     * Construct a new GeoTessModel object and populate it with information from
     * the specified file.
     *
     * @param modelInputFile
     *            name of file containing the model.
    */
    GeoTessModelSLBM(const string& modelInputFile);

    /**
     * Construct a new GeoTessModel object and populate it with information from
     * the specified file.
     *
     * @param modelInputFile
     *            name of file containing the model.
     * @param relativeGridPath
     *            the relative path from the directory where the model is stored
     *            to the directory where the grid is stored. Often, the model
     *            and grid are stored together in the same file in which case
     *            this parameter is ignored. Sometimes, however, the grid is
     *            stored in a separate file and only the name of the grid file
     *            (without path information) is stored in the model file. In
     *            this case, the code needs to know which directory to search
     *            for the grid file. The default is "" (empty string), which
     *            will cause the code to search for the grid file in the same
     *            directory in which the model file resides. Bottom line is that
     *            the default value is appropriate when the grid is stored in
     *            the same file as the model, or the model file is in the same
     *            directory as the model file.
     */
    GeoTessModelSLBM(const string& modelInputFile, const string& relativeGridPath);

    /**
     * Parameterized constructor, specifying the grid and metadata for the
     * model. The grid is constructed and the data structures are initialized
     * based on information supplied in metadata. The data structures are not
     * populated with any information however (all Profiles are null). The
     * application should populate the new model's Profiles after this
     * constructor completes.
     *
     * <p>
     * Before calling this constructor, the supplied MetaData object must be
     * populated with required information by calling the following MetaData
     * methods:
     * <ul>
     * <li>setDescription()
     * <li>setLayerNames()
     * <li>setAttributes()
     * <li>setDataType()
     * <li>setLayerTessIds() (only required if grid has more than one
     * multi-level tessellation)
     * </ul>
     *
     * @param gridFileName
     *            name of file from which to load the grid.
     * @param metaData
     *            MetaData the new GeoTessModel instantiates a reference to the
     *            supplied metaData. No copy is made.
     * @throws GeoTessException
     *             if metadata is incomplete.
     */
    GeoTessModelSLBM(const string& gridFileName, GeoTessMetaData* metaData);

    /**
     * Parameterized constructor, specifying the grid and metadata for the
     * model. The grid is constructed and the data structures are initialized
     * based on information supplied in metadata. The data structures are not
     * populated with any information however (all Profiles are null). The
     * application should populate the new model's Profiles after this
     * constructor completes.
     *
     * <p>
     * Before calling this constructor, the supplied MetaData object must be
     * populated with required information by calling the following MetaData
     * methods:
     * <ul>
     * <li>setDescription()
     * <li>setLayerNames()
     * <li>setAttributes()
     * <li>setDataType()
     * <li>setLayerTessIds() (only required if grid has more than one
     * multi-level tessellation)
     * <li>setSoftwareVersion()
     * <li>setGenerationDate()
     * </ul>
     *
     * @param grid
     *            a pointer to the GeoTessGrid that will support this
     *            GeoTessModel.  GeoTessModel assumes ownership of the
     *            supplied grid object and will delete it when it is
     *            done with it.
     * @param metaData
     *            MetaData the new GeoTessModel instantiates a reference to the
     *            supplied metaData. No copy is made.
     * @throws GeoTessException
     *             if metadata is incomplete.
     */
    GeoTessModelSLBM(GeoTessGrid* grid, GeoTessMetaData* metaData);

    virtual bool operator == (const GeoTessModelSLBM& other) const;

    virtual bool operator != (const GeoTessModelSLBM& other) const { return !(*this == other); } ;

    /**
     * Copy constructor
     */
    // GeoTessModelSLBM(const GeoTessModelSLBM &other);

    /**
     * Destructor.
     */
    virtual ~GeoTessModelSLBM();

    /**
    * Returns the class name.
    * @return class name
    */
    virtual  string class_name() { return "GeoTessModelSLBM"; }

    /**
     * Returns true if this model contains pdu information
     */
    inline bool isPathDepUncModel() { return (pdu.size() > 0) ? true : false; }

    /**
     * Returns the file format verison of the model file
     */
    inline int getFileFormatVersion() { return fileFormatVer; }

    /**
     * Get the average mantle velocity for P (index=0)
     * or S (index=1).
     */
    double getAverageMantleVelocity(const int &index) const
    {
        return averageMantleVelocity[index];
    }

    /**
     * Get the average mantle velocity for P (index=0)
     * or S (index=1).
     */
    void setAverageMantleVelocity(const int &index, const double &velocity)
    {
        averageMantleVelocity[index]=velocity;
    }

    /**
     * A 2D array of Uncertainty objects.  The array dimensions are
     * nPhases x nAttriubtes where nPhases = 4 (Pn, Sn, Pg, Lg) and
     * nAttributes is 3 (TT, SH, AZ).
     */
    const vector<vector<UncertaintyPIU*> >& getPIU() const { return piu; }

    /**
    * Path dependent uncertainty for each supported phase.
    */
    vector<UncertaintyPDU*>& getPDU()
    {
        if (isPathDepUncModel())
        {
            return pdu;
        }
        else
        {
            ostringstream os;
            os << endl << "ERROR in GeoTessModelSLBM::getPDU()." << endl <<
                    "File " << getMetaData().getInputModelFile() << endl <<
                    "This model does not contain any PDU information." << endl
                    << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
            throw SLBMException(os.str(), 105);
        }
    }


    int getBufferSize() { return 0; }

    string toString();

};
// end class GeoTessModelSLBM

}// end namespace geotess

#endif // GEOTESSMODELSLBM_H_
