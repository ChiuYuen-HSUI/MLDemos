#ifndef PROJECTOR_H
#define PROJECTOR_H

#include <vector>
#include "mymaths.h"

class Projector
{
public:
    std::vector<fvec> projected;
    std::vector<fvec> source;
    u32 dim;
    u32 startIndex, stopIndex;

    Projector() : dim(2), startIndex(0), stopIndex(-1) {}
    ~Projector(){}

    virtual void Train(std::vector< fvec > samples, ivec labels){}
    virtual fvec Project(const fvec &sample){ return sample; }
    virtual fvec Project(const fVec &sample){ return Project((fvec)sample); }
    virtual const char *GetInfoString(){return NULL;}
    virtual std::vector<fvec> GetProjected(){ return projected; }
};

#endif // PROJECTOR_H
