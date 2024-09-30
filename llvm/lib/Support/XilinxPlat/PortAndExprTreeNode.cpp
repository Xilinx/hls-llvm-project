

#include <algorithm>
#include <sstream>
#include <assert.h>
#if XILINX_HLS_FE_STANDALONE
#include "llvm/Support/XilinxPlat/PortAndExprTreeNode.h"
#else
#include "PortAndExprTreeNode.h"
#endif 

using std::string;

namespace platform
{

CPort::CPort(string name,
             DirType dir,
             unsigned bit_width,
             string constant)
    : mName(name), mDir(dir), mBitWidth(bit_width), mConstString(constant)
{
    assert(dir != CPortEnd);
}


CPort::CPort(const CPort& other)
{
    setName(other.getName());
    setDirection(other.getDirection());
    setBitWidth(other.getBitWidth());
    setConstString(other.getConstString());
}


CPort::DirType CPort::string2DirType(string dir_str)
{
    static const char* StrInPort = "IN";
    static const char* StrOutPort = "OUT";
    static const char* StrInoutPort = "INOUT";

    std::transform(dir_str.begin(), dir_str.end(),
                   dir_str.begin(), (int(*)(int))toupper);

    if (dir_str == StrInPort)
        return CPortIn;

    if (dir_str == StrOutPort)
        return CPortOut;

    if (dir_str == StrInoutPort)
        return CPortInOut;

    return CPortEnd;
}


CPort::DirType MemPort::string2DirType(string dir_str)
{
    DirType dir = CPort::string2DirType(dir_str);
    if (dir != CPortEnd)
        return dir;

    static const char* StrInPort = "WO";
    static const char* StrOutPort = "RO";
    static const char* StrInoutPort = "RW";

    std::transform(dir_str.begin(), dir_str.end(),
                   dir_str.begin(), (int(*)(int))toupper);

    if (dir_str == StrInPort)
        return CPortIn;

    if (dir_str == StrOutPort)
        return CPortOut;

    if (dir_str == StrInoutPort)
        return CPortInOut;

    return CPortEnd;
}

#if 0
string MemPort::dirType2String(CPort::DirType dir)
{
    static const char* StrInPort = "wo";
    static const char* StrOutPort = "ro";
    static const char* StrInoutPort = "rw";
    static const char* StrEndPort = "open";

    switch (dir)
    {
      case CPortIn:
          return StrInPort;
      case CPortOut:
          return StrOutPort;
      case CPortInOut:
          return StrInoutPort;
      default:
          return StrEndPort;
    }
}


ExprTreeNode::ExprTreeNode(ExprTreeNode_const_shared_ptr other)
{
    mName = other->getName();
    mOpcode = other->getOpcode();
    if (other->isBinaryOp())
    {
        mLHS = std::make_shared<ExprTreeNode>(other->getLHS());
        mRHS = std::make_shared<ExprTreeNode>(other->getRHS());
    }
    else
    mLHS = mRHS = 0;
}
#endif

string ExprTreeNode::toString() const
{
    std::stringstream msg;
    msg << " ";
    if (!isLeaf())
        msg << "{";
    msg << getName();
    if (ExprTreeNode_const_shared_ptr lhs = getLHS())
        msg << lhs->toString();

    if (ExprTreeNode_const_shared_ptr rhs = getRHS())
        msg << rhs->toString();

    if (!isLeaf())
        msg << "}";
    return msg.str();
}


} //< namespace platform

