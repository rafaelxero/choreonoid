/**
   @file
   @author Shin'ichiro Nakaoka
*/

#include "Vector3Seq.h"
#include "PlainSeqFileLoader.h"
#include "ValueTree.h"
#include "YAMLWriter.h"
#include "GeneralSeqReader.h"
#include <boost/format.hpp>
#include <fstream>
#include "gettext.h"

using namespace std;
using namespace cnoid;
using boost::format;


Vector3Seq::Vector3Seq(int nFrames)
    : BaseSeqType("Vector3Seq", nFrames)
{

}


Vector3Seq::Vector3Seq(const Vector3Seq& org)
    : BaseSeqType(org)
{

}


AbstractSeqPtr Vector3Seq::cloneSeq() const
{
    return std::make_shared<Vector3Seq>(*this);
}


Vector3Seq::~Vector3Seq()
{

}


Vector3 Vector3Seq::defaultValue() const
{
    return Vector3::Zero();
}


bool Vector3Seq::doReadSeq(const Mapping* archive, std::ostream& os)
{
    GeneralSeqReader reader(os);

    return reader.read(
        archive, this,
        [](const Listing& v, int topIndex, Vector3& value){
            if(v.size() != topIndex + 3){
                v.throwException(_("The number of elements specified as a 3D vector is invalid."));
            }
            value << v[topIndex].toDouble(), v[topIndex+1].toDouble(), v[topIndex+2].toDouble();
        });
}


bool Vector3Seq::doWriteSeq(YAMLWriter& writer)
{
    if(!writeSeqHeaders(writer)){
        return false;
    }
    
    writer.putKey("frames");
    writer.startListing();
    const int n = numFrames();
    for(int i=0; i < n; ++i){
        writer.startFlowStyleListing();
        const Vector3& v = (*this)[i];
        for(int j=0; j < 3; ++j){
            writer.putScalar(v[j]);
        }
        writer.endListing();
    }
    writer.endListing();
    return true;
}


bool Vector3Seq::loadPlainFormat(const std::string& filename, std::ostream& os)
{
    PlainSeqFileLoader loader;

    if(!loader.load(filename, os)){
        return false;
    }

    if(loader.numParts() < 3){
        os << format(_("\"%1%\" does not have 3 columns for 3d vector elements.")) % filename << endl;
        return false;
    }
  
    setNumFrames(loader.numFrames());
    setFrameRate(1.0 / loader.timeStep());

    int frame = 0;
    for(PlainSeqFileLoader::iterator it = loader.begin(); it != loader.end(); ++it){
        vector<double>& data = *it;
        (*this)[frame++] << data[1], data[2], data[3];
    }

    return true;
}


bool Vector3Seq::saveAsPlainFormat(const std::string& filename, std::ostream& os)
{
    clearSeqMessage();
    ofstream file(filename.c_str());
    file.setf(ios::fixed);

    if(!file){
        os << format(_("\"%1%\" cannot be opened.")) % filename << endl;
        return false;
    }

    format f("%1$.4f %2$.6f %3$.6f %4$.6f\n");

    const int n = numFrames();
    const double r = frameRate();

    for(int i=0; i < n; ++i){
        const Vector3& v = (*this)[i];
        file << (f % (i / r) % v.x() % v.y() % v.z());
    }
    
    return true;
}
