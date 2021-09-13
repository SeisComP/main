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

#ifndef UncertaintyPathDep_H
#define UncertaintyPathDep_H

// **** _SYSTEM INCLUDES_ ******************************************************

#include <string>
#include <vector>

#include "SLBMGlobals.h"
#include "IFStreamAscii.h"
#include "IFStreamBinary.h"
#include "Uncertainty.h"

using namespace std;

// **** _BEGIN SLBM NAMESPACE_ **************************************************

namespace slbm {

    // **** _LOCAL INCLUDES_ *******************************************************

    //!
    //! \brief A UncertaintyPathDep object contains the raw data to calculate a
    //! path dependent modeling error in seconds as a function of distance in
    //! radians along a specific source-receiver path traced through the model.
    //!
    //! A UncertaintyPathDep object contains the raw data to calculate a
    //! path dependent modeling error in seconds as a function of distance in
    //! radians along a specific source-receiver path traced through the model.
    //!
    //! <p> Code includes functionality to store and compute path dependent
    //! uncertainty for 3D grid vertex uncertainty tables (grid point and distance
    //! specific crustal error, random error, model error, and bias).
    //! This object is used by GeoTessModelPathUnc to override the 2D distance/
    //! depth dependent uncertainty (Uncertainty) used by GeotessModelSLBM. The
    //! path dependent uncertainty is only defined for the Travel Time attribute
    //! and not for azimuth and slowness. The latter two attributes still use
    //! the 2D uncertainty definitions.
    //!
    class SLBM_EXP UncertaintyPathDep
    {

    public:

        //! \brief Default constructor.
        //!
        //! Default constructor.
        UncertaintyPathDep();

        //! \brief Parameterized constructor that defines an empty path
        //! path specific model error for the input phase.
        //!
        //! Parameterized constructor that defines an empty path
        //! path specific model error for the input phase.
        UncertaintyPathDep(int phase);

        //! \brief Parameterized constructor that that defines an empty path
        //! path specific model error for the input phase.
        //!
        //! Parameterized constructor that that defines an empty path
        //! path specific model error for the input phase.
        UncertaintyPathDep(const string& phase);

        //! \brief Parameterized constructor that loads path dependent model error
        //! definition from a specified file.
        //!
        //! Parameterized constructor that loads path dependent model error
        //! definition from a specified file. Uses the input phase ordering
        //! index to define the file name.
        UncertaintyPathDep(string modelPath, int phase);

        // \brief Parameterized constructor that loads path dependent model error
        //! definition from a specified file.
        //!
        //! Parameterized constructor that loads path dependent model error
        //! definition from a specified file. Uses the input phase string
        //! to find the uncertainty data file and assigns a phase ordering
        //! index.
        UncertaintyPathDep(string modelPath, const string& phase);

        //! \brief Copy constructor.
        //!
        //! Copy constructor.
        UncertaintyPathDep(const UncertaintyPathDep& u);

        //! \brief Destructor.
        //!
        //! Destructor.
        ~UncertaintyPathDep();

        //! \brief Assignment operator.
        //!
        //! Assignment operator.
        UncertaintyPathDep& operator=(const UncertaintyPathDep& u);

        /**
        * Overloaded equality operator
        * @param other reference to the other UncertaintyPathDep object to which
        * this UncertaintyPathDep object is to be compared
        * @return true if this and other are equal.
        */
        bool operator==(const UncertaintyPathDep& other);

        /**
        * Overloaded inequality operator
        * @param other reference to the other UncertaintyPathDep object to which
        * this UncertaintyPathDep object is to be compared
        * @return true if this and other are not equal.
        */
        bool operator!=(const UncertaintyPathDep& other) { return !(*this == other); }

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(ifstream& input, int phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(ifstream& input, const string& phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(geotess::IFStreamAscii& input, int phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(geotess::IFStreamAscii& input, const string& phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(geotess::IFStreamBinary& input, int phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param input data source
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(geotess::IFStreamBinary& input, const string& phase);

        /**
        * Retrieve a new UncertaintyPathDep object for the specified phase
        * loaded from specified input source.
        * @param modelPath data source path
        * @param phase 0:Pn, 1:Sn, 2:Pg, 3:Lg
        * @return pointer to an UncertaintyPathDep object.
        */
        static UncertaintyPathDep* getUncertainty(const string& modelPath, int phase);

        void readFile(ifstream& fin);

        void readFile(geotess::IFStreamAscii& fin);

        void readFile(geotess::IFStreamBinary& fin);

        void writeFile(geotess::IFStreamAscii& output);

        void writeFile(geotess::IFStreamBinary& fout);

        //! \brief A public convenience accessor used to verify the error data for the
        //! correct model phase is loaded in memory.
        //!
        //! A public convenience accessor used to verify the error data for the
        //! correct model phase is loaded in memory.
        int getPhase() const {
            return phaseNum;
        }

        //! \brief A public convenience accessor used to verify the error data for the
        //! correct model phase is loaded in memory.
        //!
        //! A public convenience accessor used to verify the error data for the
        //! correct model phase is loaded in memory.
        const string getPhaseStr() const {
            return getPhase(phaseNum);
        }

        //! \brief Returns loaded file name or "not specified".
        //!
        //! Returns loaded file name or "not specified".
        const string& getLoadedFileName() const {
            return fname;
        }

        //! \brief Returns the grid id for which this path dependent uncertainty
        //! objects data are defined.
        //!
        //! Returns the grid id for which this path dependent uncertainty
        //! objects data are defined.
        const string& getGridId() const {
            return gridId;
        }

        //! \brief Returns the model uncertainty as a function of angular distance
        //! (degrees), crustal grid vertex indices, crustal grid weights (sec),
        //! head wave grid vertex indices, head wave grid weights (km), and head wave
        //! node neighbors. An optional argument, calcRandomError, can be provided
        //! to enable/disable using the random error component in the uncertainty
        //! computation.
        //!
        //! Returns the model uncertainty as a function of angular distance
        //! (degrees), crustal grid vertex indices, crustal grid weights (sec),
        //! head wave grid vertex indices, head wave grid weights (km), and head wave
        //! node neighbors. An optional argument, calcRandomError, can be
        //! provided to enable/disable using the random error component in the
        //! uncertainty computation.
        double getUncertainty(double distance,
            const vector<int>& crustNodeIds, const vector<double>& crustWeights,
            const vector<int>& headWaveNodeIds, const vector<double>& headWaveWeights,
            const vector<vector<int> >& headWaveNodeNeighbors, const bool& calcRandomError = false,
            bool printDebugInfo = false);

        //! \brief Returns true if random error is defined.
        //!
        //! Returns true if random error is defined.
        bool isRandomErrorDefined() const {
            return (pathUncRandomError.size() > 0);
        }

        string toStringTable();

        string toStringFile();

        static string getPhase(const int& phaseIndex)
        {
            return Uncertainty::getPhase(phaseIndex);
        }

        static int getPhase(const string& phase)
        {
            return Uncertainty::getPhase(phase);
        }

        vector<double>& getPathUncCrustError() {
            return pathUncCrustError;
        }

        const vector<double>& getPathUncCrustError() const {
            return pathUncCrustError;
        }

        vector<vector<double> >& getPathUncRandomError() {
            return pathUncRandomError;
        }

        const vector<vector<double> >& getPathUncRandomError() const {
            return pathUncRandomError;
        }

        vector<double>& getPathUncDistanceBins() {
            return pathUncDistanceBins;
        }

        const vector<double>& getPathUncDistanceBins() const {
            return pathUncDistanceBins;
        }

        vector<vector<double> >& getPathUncModelError() {
            return pathUncModelError;
        }

        const vector<vector<double> >& getPathUncModelError() const {
            return pathUncModelError;
        }

        vector<vector<double> >& getPathUncBias() {
            return pathUncBias;
        }

        const vector<vector<double> >& getPathUncBias() const {
            return pathUncBias;
        }

    private:

        //! \brief A function called by the constructor to read model error data
        //! from an ASCII text file.
        //!
        //! \brief A function called by the constructor to read model error data
        //! from an ASCII text file.
        void readFile(const string& filename);

        //! \brief The path and filename for the currently loaded seismic phase
        //! modeling error file.
        //!
        //! The path and filename for the currently loaded seismic phase
        //! modeling error file.
        string fname;

        //! \brief The seismic phase for which modeling error data was loaded.
        //!
        //! The seismic phase for which modeling error data was loaded.
        int phaseNum;

        /**
        * The Uncertainty objects grid id. This is the string returned from
        * GeoTessModel.getGrid().getGridID(). To use this path dependent
        * uncertainty properly the Grid defined in the GeoTessModel must have
        * the same grid id as this value.
        */
        string gridId;

        /**
        * A 1D array of crustal error over each grid point vertex (point)
        * in the GeoTess model indexed as pathUncCrustError[point].
        */
        vector<double> pathUncCrustError;

        /**
        * A 1D array of distance bins (dist in km) used by the model error, random
        * error, and bias, and indexed as pathUncDistanceBins[dist].
        */
        vector<double> pathUncDistanceBins;

        /**
        * A 2D array of random error over each distance bin (dist), and each grid
        * point vertex (point) in the GeoTess model, for which the path uncertainty
        * is defined. The array is indexed as pathUncRandomError[dist][point].
        * This is an optional data field and can be defined to be empty.
        */
        vector<vector<double> > pathUncRandomError;

        /**
        * A 2D array of model error over each distance bin (dist), and each grid
        * point vertex (point) in the GeoTess model, for which the path uncertainty
        * is defined. The array is indexed as pathUncModelError[dist][point].
        */
        vector<vector<double> > pathUncModelError;

        /**
        * A 2D array of bias over each distance bin (dist), and each grid point
        * vertex (point) in the GeoTess model, for which the path uncertainty
        * is defined. The array is indexed as pathUncBias[dist][point].
        */
        vector<vector<double> > pathUncBias;

    };

} // end slbm namespace

#endif // UncertaintyPathDep_H
