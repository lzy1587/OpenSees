#ifndef RockingLimit20_h
#define RockingLimit20_h

#include <Element.h>
#include <Matrix.h>
#include <Vector.h>

class Node;
class Domain;

// Generalized radial rocking stop acting on the least-squares plane of 20 Z DOFs.
class RockingLimit20 : public Element {
public:
    RockingLimit20(int tag, const ID &nodes, double stiffness, double limit);
    ~RockingLimit20();

    const char *getClassType(void) const { return "RockingLimit20"; }
    int getNumExternalNodes(void) const { return 20; }
    const ID &getExternalNodes(void) { return connected; }
    Node **getNodePtrs(void) { return theNodes; }
    int getNumDOF(void) { return 120; }
    void setDomain(Domain *domain);
    int revertToLastCommit(void) { return 0; }
    const Matrix &getTangentStiff(void);
    const Matrix &getInitialStiff(void);
    const Matrix &getMass(void);
    const Matrix &getDamp(void);
    const Vector &getResistingForce(void);
    const Vector &getResistingForceIncInertia(void);
    int sendSelf(int commitTag, Channel &channel);
    int recvSelf(int commitTag, Channel &channel, FEM_ObjectBroker &broker);
    void Print(OPS_Stream &stream, int flag = 0);
    Response *setResponse(const char **argv, int argc, OPS_Stream &stream);
    int getResponse(int responseID, Information &info);

private:
    void evaluate(void);
    ID connected;
    Node *theNodes[20];
    Matrix tangent;
    Matrix zero;
    Vector force;
    Vector thetaMoment;
    double projection[2][20];
    double reference[20];
    double kTheta;
    double thetaLimit;
    bool ready;
};

#endif
