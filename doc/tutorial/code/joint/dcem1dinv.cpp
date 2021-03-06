/***************************************************************************
 * This file belongs to GIMLi (Geophysical Inversion and Modelling) library*
 * and was created for documentation purposes only.                        *
 * DCEM1dInv is a block joint inversion of DC and EM resistivity soundings *
 * Run with example file using: dc1dblock sond1-100dat                     *
***************************************************************************/

#include <gimli.h>
#include <dc1dmodelling.h>
#include <em1dmodelling.h>
#include <inversion.h>
#include <modellingbase.h>

#include <string>
//using namespace std;
using namespace GIMLI;

#define vcout if ( verbose ) std::cout
#define dcout if ( debug ) std::cout
#define DEBUG if ( debug )

class DCEM1dModelling : public ModellingBase {
public:
    DCEM1dModelling( size_t nlay, RVector & ab2, RVector & mn2, RVector & freq, double coilspacing, bool verbose )
    : ModellingBase( createMesh1DBlock( nlay ), verbose ), fDC_( nlay, ab2, mn2, verbose ), fEM_( nlay, freq, coilspacing, verbose ) { 
        setMesh( createMesh1DBlock( nlay ) );
    }
    RVector response( const RVector & model ){ 
        return cat( fDC_.response( model ), fEM_.response( model ) ); 
    }
protected:
    DC1dModelling fDC_;
    FDEM1dModelling fEM_;
};

int main( int argc, char *argv [] )
{
    std::string dataFile( NOT_DEFINED );
    double errDC = 3., errEM = 1., lambda = 300., lbound = 1., ubound = 1000., coilspacing = 50.;
    size_t nlay = 3, nModel = 2 * nlay - 1;
    bool verbose = true;

    /*! DC data Schlumberger sounding */
    RVector ab2( 20, 1.0 );
    for ( size_t i = 1; i < ab2.size(); i++ ) ab2[ i ] = ab2[ i - 1 ] * 1.3;
    RVector mn2( ab2.size(), ab2[ 0 ] / 3.0 );

    /*! EM data Maxmin type sounding */
    RVector freq( 10, 110. );
    for ( size_t i = 1; i < freq.size(); i++ ) freq[ i ] = freq[ i - 1 ] * 2.0;

    /*! initialize forward operator */
    DCEM1dModelling f( nlay, ab2, mn2, freq, coilspacing, verbose );
   
    /*! synthetic data */
    RVector synthModel( nModel, 15. );
    synthModel[ nlay - 1 ] = 200.;
    synthModel[ nlay ] = 10.;
    synthModel[ nlay + 1 ] = 50.;
    std::cout << "synthModel: " << synthModel << std::endl;
    RVector synthData( f( synthModel ) );
    std::cout << "synthData: " << synthData << std::endl;
    
    /*! error models: relative percentage for DC, absolute for EM */
    RVector errorDC = synthData( 0, ab2.size() ) * errDC / 100.0;
    RVector errorEM( freq.size() * 2, errEM );
    RVector errorAbs( cat( errorDC, errorEM ) );
    
    /*! noisify synthetic data using the determined error model */
    RVector rand( synthData.size() );
    randn( rand );
    synthData = synthData + rand * errorAbs;

    /*! Model transformations: log for thickness, logLU for resistivity */
    RTransLog transThk;
    RTransLogLU transRho( lbound, ubound );
    f.region( 0 )->setTransModel( transThk );
    f.region( 1 )->setTransModel( transRho );
    f.region( 0 )->setStartValue( 20.0 );
    f.region( 1 )->setStartValue( synthModel[ 0 ] );
    
    /*! Data transformations: log for app. resistivity, lin for EM values */
    RTransLog transRhoa;
    RTrans transEM;
    CumulativeTrans< RVector > transData;
    transData.push_back( transRhoa, ab2.size() );
    transData.push_back( transEM, freq.size() * 2 );
    
    /*! Starting model */
    RVector model = f.createStartVector();
    model[ nlay ] *= 1.5;
    std::cout << "starting model: " << model << std::endl;

    /*! Set up inversion with full matrix, data and forward operator */
    RInversion inv( synthData, f, transData, verbose );
    /*! set up options */
    inv.setLambda( lambda );
    inv.setAbsoluteError( errorAbs );      //! error model
    inv.setModel( model );       //! starting model
    inv.setMarquardtScheme( 0.9 );

    /*! actual computation: run the inversion and save/print result */
    model = inv.run();
    save( model, "model.vec" );
    std::cout << "model = " << model << std::endl;
    std::cout << "synthModel: " << synthModel << std::endl;

    /*! compute resolution properties and save/print them */
    RVector resolution( nModel );
    RVector resMDiag ( nModel );
    RMatrix resM;
    for ( size_t iModel = 0; iModel < nModel; iModel++ ) {
        resolution = inv.modelCellResolution( iModel );
        resM.push_back( resolution );
        resMDiag[ iModel ] = resolution[ iModel ];
    }
    save( resMDiag, "resMDiag.vec" );
    save( resM,     "resM" );
    vcout << "resolution = " << resMDiag << std::endl;

    return EXIT_SUCCESS;
}

